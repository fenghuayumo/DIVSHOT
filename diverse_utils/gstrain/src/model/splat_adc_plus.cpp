#include "splat_adc_plus.hpp"
#include "rasterize_gaussians.hpp"
#include <tensor_math.hpp>
#include "selective_adam.hpp"
#if defined(USE_CUDA)
#include <c10/cuda/CUDACachingAllocator.h>
#elif defined(USE_HIP)
#include <c10/hip/HIPCachingAllocator.h>
#endif

void SplatADCPlusDensify::resetState()
{
    xysGradNorm = torch::Tensor();
    visCounts = torch::Tensor();
}

extern torch::Tensor multinomial_sample(const torch::Tensor& weights, int n, bool replacement);


torch::Tensor scaleDownLargestDim(const torch::Tensor& scales, float factor) {
    auto maxVal = std::get<0>(scales.max(1,true));
    auto maxMask = scales == maxVal;
	auto scale = torch::ones_like(scales,scales.device()).masked_fill(maxMask, factor);
	return scales * scale;
}

void SplatADCPlusDensify::stepAfterbackward(int step,std::unordered_map<std::string_view, torch::Tensor>& infos)
{
    torch::NoGradGuard noGrad;
    const auto& trainConfig = gaussian->trainConfig;
    const float opac_decay = 0.004;
    const float scale_decay = 0.002;
    const float mean_noise_weight = 100.0f;
    auto& device = gaussian->device;
    auto& means = gaussian->means;
    auto& scales = gaussian->scales;
    auto& opacities = gaussian->opacities;
    auto& featuresDc = gaussian->featuresDc;
    auto& featuresRest = gaussian->featuresRest;
    auto& quats = gaussian->quats;
    auto& meansOpt = gaussian->meansOpt;
    auto& scalesOpt = gaussian->scalesOpt;
    auto& quatsOpt = gaussian->quatsOpt;
    auto& featuresDcOpt = gaussian->featuresDcOpt;
    auto& featuresRestOpt = gaussian->featuresRestOpt;
    auto& opacitiesOpt = gaussian->opacitiesOpt;
    torch::Tensor visibleMask;
    const float train_t = static_cast<float>(step) / trainConfig.numIters;
    const bool hasGrad = step % trainConfig.refineEvery == 0 && step > trainConfig.warmupLength && train_t < 0.85;
    if (hasGrad){
        const auto packed = trainConfig.packLevel & GSPackLevel::PackTileID;
        torch::Tensor grads = RasterizeGaussians::means2dGrad.detach();
        // Check if using GUT rasterization by gradient dimension:
        // GUT mode: means2dGrad is [N, 3] (derived from 3D gradients)
        // Traditional 3DGS: means2dGrad is [C, N, 2] or [nnz, 2] (2D gradients)
        bool useGut = grads.size(-1) == 3;
        if(!trainConfig.useMask && !useGut)
        {
            // Only apply depth-based scaling for traditional 3DGS mode
            auto depth = infos["viewZ"].detach();
            auto scaleFactor = torch::minimum(torch::pow(depth / trainConfig.depthThreshold,2), torch::ones_like(depth));
            auto scaleFactorExpand = scaleFactor.unsqueeze(-1).expand_as(grads);
            grads *= scaleFactorExpand;
        }
        torch::Tensor gs_ids, radii;
        if (packed)
        {
            // In packed mode:
            // - Traditional 3DGS: grads (means2dGrad) is [nnz, 2]
            // - GUT mode: grads (means2dGrad) is [N, 3] (already aggregated to all gaussians)
            visibleMask = torch::zeros({ opacities.size(0) }, torch::kBool).to(device);
            gs_ids = infos["gaussianIds"]; //[nnz]
            radii = std::get<0>(infos["radii"].max(-1)); //[nnz]
            visibleMask.scatter_(0, gs_ids, 1);
            visibleMask = visibleMask.unsqueeze(-1);
            
            // GUT mode: grads is [N, 3], need to filter by visible gaussians
            if (useGut) {
                grads = grads.index({gs_ids});  // [N, 3] -> [nnz, 3]
            }
            // Traditional 3DGS packed mode: grads is already [nnz, 2], no filtering needed
        }
        else
        {
            //grads is [C, N, 2] or [N, 3] for GUT
            visibleMask = (infos["radii"] > 0.0).all(-1);        //[1,nnz]
            gs_ids = torch::where(visibleMask)[1];
            if (!useGut) {
                grads = grads.index({visibleMask});  // [nnz, 2]
            } else {
                // GUT mode: grads is [N, 3], filter by visible gaussians
                grads = grads.index({gs_ids});  // [nnz, 3]
            }
            radii = std::get<0>(infos["radii"].index({visibleMask}).max(-1));  // [nnz]
            visibleMask = visibleMask.squeeze(0).unsqueeze(-1);
        }
        // For GUT mode, means2dGrad is already derived from 3D gradients (v_means * 2 / depth)
        // and doesn't need pixel-space scaling. For traditional 3DGS, scale by image size.
        if (!useGut) {
            grads.index({ "...", 0 }) *= gaussian->lastWidth * 0.5f;
            grads.index({ "...", 1 }) *= gaussian->lastHeight * 0.5f;
        }
        if (!xysGradNorm.numel())
        {
            xysGradNorm = torch::zeros({ means.size(0)}, grads.options());
            visCounts = torch::zeros_like(xysGradNorm);
        }
        xysGradNorm.index_add_(0, gs_ids, torch::linalg_vector_norm(grads, 2, { -1 }, false, torch::kFloat32));
        visCounts.index_add_(0, gs_ids, torch::ones_like(gs_ids, torch::kFloat));
    }
    // else {
    //     visibleMask = torch::zeros_like(opacities, torch::kBool).to(device);
    // }
     //add noise
    // if(step > trainConfig.warmupLength)
    {
        auto boundsExtent = glm::length(gaussian->boundsMax - gaussian->boundsMin);
        float medianScale = boundsExtent;
        auto inv_opac = 1.0 - torch::sigmoid(opacities);
        auto noise_weight = hasGrad ? inv_opac.pow(150.0).clamp(0.0, 1.0) * visibleMask.to(torch::kFloat) : inv_opac.pow(150.0).clamp(0.0, 1.0);
        auto samples = torch::randn({ means.size(0), 3 }, device);
        auto max_noise = medianScale;
        noise_weight = noise_weight * (gaussian->meansOptScheduler->getlr(step) * mean_noise_weight);
        auto noise = (samples * noise_weight).clamp(-max_noise, max_noise);
        if (trainConfig.enableBg) {
            noise.index_put_({ torch::indexing::Slice(0, trainConfig.numSkyPoints) }, 0);
        }
        means.add_(noise);
        means.requires_grad_(true);
    }
    if (step % trainConfig.refineEvery == 0 && step > trainConfig.warmupLength && train_t < 0.85){
         bool doDensification = step < trainConfig.refineStopIter;
        //bool doDensification = step < trainConfig.refineStopIter && step % trainConfig.resetAlphaEvery > (trainConfig.refineEvery);
        float minOpacity = trainConfig.minOpacity;//1.0 / 255.0f;
        int numPointsBefore = means.size(0);
        torch::Tensor avgGradNorm = (xysGradNorm / visCounts.clamp_min(1)) * 5 * std::clamp( static_cast<float>(gaussian->numCameras) / 500.0f, 1.0f, 3.0f);
        torch::Tensor highGrads = (avgGradNorm > trainConfig.growGrad2d);
        if (trainConfig.enableBg) {
            highGrads.index_put_({ torch::indexing::Slice(0, trainConfig.numSkyPoints) }, false);
        }
        // Split gaussians that are too large
        auto boundsMin = gaussian->boundsMin;
        auto boundsMax = gaussian->boundsMax;
        auto center = (boundsMin + boundsMax) / 2.0f;
        auto boundCenter = torch::tensor({center.x,center.y,center.z},torch::kFloat).to(device);
        auto maxAllowExtent = 100.0f * glm::length(boundsMax - boundsMin);
        auto expScales = scales.exp();
        auto culls = torch::sigmoid(opacities).squeeze(-1) <= minOpacity;
        auto isSmall = (std::get<0>(expScales.min(-1)) < 1e-10).squeeze();
        auto isHuge = (std::get<0>(expScales.max(-1)) > maxAllowExtent * 0.8).squeeze();
        auto splatDist = torch::linalg_vector_norm(means - boundCenter, 2, { -1 }, true, torch::kFloat32);
        if(trainConfig.enableFocusRegion){
            auto focusBox = infos["focusBox"];
            auto focusBoxMin = focusBox.data_ptr<float>();
            auto focusBoxMax = focusBoxMin + 3;
            auto focusBoxMask = (means.index({Slice(), 0}) >= focusBoxMin[0]) & (means.index({Slice(), 1}) >= focusBoxMin[1]) & (means.index({Slice(), 2}) >= focusBoxMin[2]) &
                            (means.index({Slice(), 0}) <= focusBoxMax[0]) & (means.index({Slice(), 1}) <= focusBoxMax[1]) & (means.index({Slice(), 2}) <= focusBoxMax[2]);
            culls &= focusBoxMask;
        }else{
            auto isFar = (splatDist > maxAllowExtent).squeeze();
            culls |= isFar;
        }
        culls |= isHuge;
        culls |= isSmall;
        culls |= ((quats * quats).sum(-1) < 1e-8f);
        if(trainConfig.enableBg){
            culls.index_put_({torch::indexing::Slice(0, trainConfig.numSkyPoints)}, false); // Do not cull sky points
        }
        auto pruneCount = culls.nonzero().numel();
        if(pruneCount > 0){
            // Cull
            int numPointsBefore = means.size(0);

            means = means.index({~culls}).detach().requires_grad_();
            scales = scales.index({~culls}).detach().requires_grad_();
            quats = quats.index({~culls}).detach().requires_grad_();
            featuresDc = featuresDc.index({~culls}).detach().requires_grad_();
            featuresRest = featuresRest.index({~culls}).detach().requires_grad_();
            opacities = opacities.index({~culls}).detach().requires_grad_();
            visibleMask = visibleMask.index({ ~culls });
            highGrads = highGrads.index({ ~culls });
            avgGradNorm = avgGradNorm.index({~culls });
            removeFromOptimizer(meansOpt.get(), means, culls);
            removeFromOptimizer(scalesOpt.get(), scales, culls);
            removeFromOptimizer(quatsOpt.get(), quats, culls);
            removeFromOptimizer(featuresDcOpt.get(), featuresDc, culls);
            removeFromOptimizer(featuresRestOpt.get(), featuresRest, culls);
            removeFromOptimizer(opacitiesOpt.get(), opacities, culls);
               
            if (trainConfig.verbose)
                std::cout << "Culled " << (numPointsBefore - means.size(0)) << " gaussians, remaining " << means.size(0) << std::endl;
        }
        // Replace dead gaussians.
        std::set<int64_t> refineIdSets;
        if(pruneCount > 0){
            auto reSampleWeights = torch::sigmoid(opacities) * visibleMask.to(torch::kFloat32);
            torch::Tensor sampledIds = multinomial_sample(reSampleWeights.squeeze(-1), pruneCount, true).cpu();
            auto flat_tensor = sampledIds.flatten();
            int64_t* data_ptr = flat_tensor.data_ptr<int64_t>();
            int64_t num_elements = flat_tensor.numel();
            for (auto i = 0; i < num_elements; i++) {
                refineIdSets.insert(data_ptr[i]);
            }
        }
        if(trainConfig.enableFocusRegion){
            auto focusBox = infos["focusBox"];
            auto focusBoxMin = focusBox.data_ptr<float>();
            auto focusBoxMax = focusBoxMin + 3;
            auto focusBoxMask = (means.index({Slice(), 0}) >= focusBoxMin[0]) & (means.index({Slice(), 1}) >= focusBoxMin[1]) & (means.index({Slice(), 2}) >= focusBoxMin[2]) &
                            (means.index({Slice(), 0}) <= focusBoxMax[0]) & (means.index({Slice(), 1}) <= focusBoxMax[1]) & (means.index({Slice(), 2}) <= focusBoxMax[2]);
            highGrads &= focusBoxMask;
        }
        if (doDensification) {
            int32_t highGradCount = highGrads.nonzero().numel();
            int32_t growCount = highGradCount * trainConfig.growFraction;
            int32_t sampleHighGradCount = std::max<int32_t>(growCount, 0);
            int current_n_points = means.size(0) + refineIdSets.size();
            growCount = std::min<int32_t>(trainConfig.capMax - current_n_points, sampleHighGradCount);
            if(growCount > 0){
                auto weights = highGrads.to(torch::kFloat32) * avgGradNorm;
                auto growthIds = multinomial_sample(weights, growCount, true).cpu();
                auto flat_tensor = growthIds.flatten();
                int64_t* data_ptr = flat_tensor.data_ptr<int64_t>();
                int64_t num_elements = flat_tensor.numel();
                for (auto i = 0; i < num_elements; i++) {
                    refineIdSets.insert(data_ptr[i]);
                }
            }
        }
        std::vector<int64_t> refineIds(refineIdSets.begin(), refineIdSets.end());
        auto resampleIds = torch::tensor(refineIds, torch::kLong).to(device);
        auto nRefineCounts = resampleIds.size(0);

        auto expScalesSelected = torch::exp(scales.index({ resampleIds }));
        torch::Tensor qs = quats.index({ resampleIds }) / torch::linalg_vector_norm(quats.index({ resampleIds }), 2, { -1 }, true, torch::kFloat32);

        torch::Tensor rots = quatToRotMat(qs);

        torch::Tensor centeredSamples = torch::randn({ nRefineCounts, 3 }, device);  // Nx3 of axis-aligned scales
        torch::Tensor scaledSamples = expScalesSelected * centeredSamples;
        torch::Tensor rotatedSamples = torch::bmm(rots, scaledSamples.index({ "...", None })).squeeze();
        torch::Tensor refineMeans = rotatedSamples + means.index({ resampleIds });
        torch::Tensor refineScales = torch::log(scaleDownLargestDim(expScalesSelected, 0.5));
        torch::Tensor refineFeaturesDc = featuresDc.index({ resampleIds });
        torch::Tensor refineFeaturesRest = featuresRest.index({ resampleIds });
        torch::Tensor refineOpacities;
        if (trainConfig.revisedOpacity)
        {
            auto newOpacities = 1.0 - torch::sqrt(1.0 - torch::sigmoid(opacities.index({ resampleIds })));
            refineOpacities = torch::logit(newOpacities.clamp(minOpacity, 1.0f - minOpacity));  // [2N]
        }
        else
            refineOpacities = opacities.index({ resampleIds });

        torch::Tensor refineQuats = quats.index({ resampleIds });

        means.index_put_({ resampleIds }, means.index({ resampleIds }) - rotatedSamples);
        scales.index_put_({ resampleIds }, refineScales);
        opacities.index_put_({ resampleIds }, refineOpacities);

        means = torch::cat({ means.detach(), refineMeans }, 0).requires_grad_();
        featuresDc = torch::cat({ featuresDc.detach(), refineFeaturesDc }, 0).requires_grad_();
        featuresRest = torch::cat({ featuresRest.detach(), refineFeaturesRest }, 0).requires_grad_();
        opacities = torch::cat({ opacities.detach(), refineOpacities }, 0).requires_grad_();
        scales = torch::cat({ scales.detach(), refineScales }, 0).requires_grad_();
        quats = torch::cat({ quats.detach(), refineQuats }, 0).requires_grad_();
        visibleMask = torch::cat({ visibleMask.detach(), torch::ones_like(refineOpacities).to(device) }, 0);

        addToOptimizer(meansOpt.get(), means, resampleIds, 1);
        addToOptimizer(scalesOpt.get(), scales, resampleIds, 1);
        addToOptimizer(quatsOpt.get(), quats, resampleIds, 1);
        addToOptimizer(featuresDcOpt.get(), featuresDc, resampleIds, 1);
        addToOptimizer(featuresRestOpt.get(), featuresRest, resampleIds, 1);
        addToOptimizer(opacitiesOpt.get(), opacities, resampleIds, 1);

        if (trainConfig.verbose)
            std::cout << "Added " << (means.size(0) - numPointsBefore) << " gaussians, new count " << means.size(0) << std::endl;

        // if (step % trainConfig.resetAlphaEvery == 0){
        //    float resetValue = trainConfig.minOpacity * 2.0f;
        //     // opacities = torch::clamp_max(opacities, torch::logit(torch::tensor(resetValue)).item<float>());
        //    auto resetOpacities = torch::logit(torch::minimum(torch::sigmoid(opacities), torch::ones_like(opacities) * resetValue));
        //    if(trainConfig.enableBg){
        //       //don't reset sky points
        //       resetOpacities.index_put_({torch::indexing::Slice(0, trainConfig.numSkyPoints)}, opacities.index({torch::indexing::Slice(0, trainConfig.numSkyPoints)}));
        //    }
        //    opacities = resetOpacities;
        //     // Reset optimizer
        //     torch::Tensor param = opacitiesOpt->param_groups()[0].params()[0];
        //     #if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
        //         auto pId = param.unsafeGetTensorImpl();
        //     #else
        //         auto pId = c10::guts::to_string(param.unsafeGetTensorImpl());
        //     #endif    
        //     auto paramState = std::make_unique<AdamCustomParamState>(static_cast<AdamCustomParamState&>(*opacitiesOpt->state()[pId]));
        //     paramState->exp_avg(torch::zeros_like(paramState->exp_avg()));
        //     paramState->exp_avg_sq(torch::zeros_like(paramState->exp_avg_sq()));
        //     if(paramState->step_per_gaussian().defined()){
        //         paramState->step_per_gaussian(torch::zeros_like(paramState->step_per_gaussian()));
        //     }
        //     if (trainConfig.verbose)
        //         std::cout << "Alpha reset" << std::endl;
        //}
        auto t_shrink_strength = 1.0 - train_t;

        // if(trainConfig.opacityReg <= 0.0){
             auto minus_opac = opac_decay * t_shrink_strength;
             auto new_opac = torch::sigmoid(opacities) - minus_opac;
             opacities = torch::logit(new_opac.clamp(1e-12, 1.0 - 1e-12)).requires_grad_();
        // }
        // if(trainConfig.scaleReg <= 0.0){
        auto scale_scaling = 1.0 - scale_decay * t_shrink_strength;
        auto new_scale = torch::exp(scales) * scale_scaling;
        scales = torch::log(new_scale).requires_grad_();
        // }
        auto optimizeFunc = [&](torch::Tensor& v)->torch::Tensor {
            return v;
        };
        updateParamWithOptimizer(opacitiesOpt.get(), opacities, optimizeFunc);
        updateParamWithOptimizer(scalesOpt.get(), scales, optimizeFunc);
        gaussian->updateBounds(0.8f);
        // Clear
        xysGradNorm = torch::Tensor();
        visCounts = torch::Tensor();

        if (gaussian->device != torch::kCPU){
            #ifdef USE_HIP
                    c10::hip::HIPCachingAllocator::emptyCache();
            #elif defined(USE_CUDA)
                    c10::cuda::CUDACachingAllocator::emptyCache();
            #elif defined(USE_MPS)
                    // torch::mps::emptyCache();
            #endif
        }
    }
}