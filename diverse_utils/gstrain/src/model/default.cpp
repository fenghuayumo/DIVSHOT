
#include "default.hpp"
#include "rasterize_gaussians.hpp"
#include <tensor_math.hpp>
#include "selective_adam.hpp"
#if defined(USE_CUDA)
#include <c10/cuda/CUDACachingAllocator.h>
#elif defined(USE_HIP)
#include <c10/hip/HIPCachingAllocator.h>
#endif
void DefaultDensify::resetState()
{
    xysGradNorm = torch::Tensor();
    visCounts = torch::Tensor();
    max2DSize = torch::Tensor();
    // max2DSize = torch::zeros({ gaussian->means.size(0) }).to(gaussian->device, true);
}

void DefaultDensify::stepAfterbackward(int step,std::unordered_map<std::string_view, torch::Tensor>& infos)
{
    torch::NoGradGuard noGrad;
    const auto& trainConfig = gaussian->trainConfig;
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
    if (step < trainConfig.refineStopIter){
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
            gs_ids = infos["gaussianIds"]; //[nnz]
            radii = std::get<0>(infos["radii"].max(-1)); //[nnz]
            
            // GUT mode: grads is [N, 3], need to filter by visible gaussians
            if (useGut) {
                grads = grads.index({gs_ids});  // [N, 3] -> [nnz, 3]
            }
            // Traditional 3DGS packed mode: grads is already [nnz, 2], no filtering needed
        }
        else
        {
            //grads is [C, N, 2] or [N, 3] for GUT
            auto visibleMask = (infos["radii"] > 0.0).all(-1);        //[1,nnz]
            gs_ids = torch::where(visibleMask)[1];
            if (!useGut) {
                grads = grads.index({visibleMask});  // [nnz, 2]
            } else {
                // GUT mode: grads is [N, 3], filter by visible gaussians
                grads = grads.index({gs_ids});  // [nnz, 3]
            }
            radii = std::get<0>(infos["radii"].index({visibleMask}).max(-1));  // [nnz]
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
            max2DSize = torch::zeros_like(visCounts, torch::kFloat32);
        }
        xysGradNorm.index_add_(0, gs_ids, torch::linalg_vector_norm(grads, 2, { -1 }, false, torch::kFloat32));
        visCounts.index_add_(0, gs_ids, torch::ones_like(gs_ids, torch::kFloat));
        if (trainConfig.refineScale2dStopIter > 0 )
        {
            auto normRadii = radii / static_cast<float>(std::max(gaussian->lastHeight, gaussian->lastWidth));
            max2DSize.index_put_({ gs_ids }, torch::maximum(max2DSize.index({gs_ids}), normRadii));
        }
    }
    const float train_t = static_cast<float>(step) / trainConfig.numIters;
    if (step % trainConfig.refineEvery == 0 && step > trainConfig.warmupLength && train_t < 0.85){
        int resetInterval = trainConfig.resetAlphaEvery;
        bool doDensification = step < trainConfig.refineStopIter && step % resetInterval > trainConfig.refineEvery;
        const float cullAlphaThresh = /*0.1f*/trainConfig.pruneOpacity;
       
        if (doDensification){
            int numPointsBefore = means.size(0);
            torch::Tensor avgGradNorm = (xysGradNorm / visCounts.clamp_min(1)) * std::clamp( static_cast<float>(gaussian->numCameras) / 500.0f, 1.0f, 3.0f);;
                                        // * 0.5 * static_cast<float>(std::max<int>(gaussian->lastWidth, gaussian->lastHeight));
                                        //* (int)std::ceil(gaussian->numCameras / 100.0f);
            torch::Tensor highGrads = (avgGradNorm > trainConfig.growGrad2d);
            if (trainConfig.enableBg) {
                highGrads.index_put_({ torch::indexing::Slice(0, trainConfig.numSkyPoints) }, false);
            }
            // Split gaussians that are too large
            auto isHuge = std::get<0>(scales.exp().max(-1)) > (trainConfig.growScale3d * gaussian->sceneScale);
            torch::Tensor splits = isHuge;
            if (step < trainConfig.refineScale2dStopIter){
                splits |= (max2DSize > trainConfig.growScale2d);
            }

            splits &= highGrads;
            torch::Tensor focusBoxMask;
            if(trainConfig.enableFocusRegion){
                auto focusBox = infos["focusBox"];
                auto focusBoxMin = focusBox.data_ptr<float>();
                auto focusBoxMax = focusBoxMin + 3;
                //means[:,0] >= focusBoxMin[0] && means[:,1] >= focusBoxMin[1] && means[:2] >= focusBoxMin[2]
                focusBoxMask = (means.index({Slice(), 0}) >= focusBoxMin[0]) & (means.index({Slice(), 1}) >= focusBoxMin[1]) & (means.index({Slice(), 2}) >= focusBoxMin[2]) &
                               (means.index({Slice(), 0}) <= focusBoxMax[0]) & (means.index({Slice(), 1}) <= focusBoxMax[1]) & (means.index({Slice(), 2}) <= focusBoxMax[2]);
                splits &= focusBoxMask;
            }
            const int nSplitSamples = 2;
            int nSplits = splits.sum().item<int>();
            auto current_n_points = means.size(0);
            if( (nSplits + current_n_points) > trainConfig.capMax){
                //Cap the number GSs
                nSplits = std::min<int>(nSplits,std::max<int>(0, trainConfig.capMax - current_n_points));
                auto indices = torch::nonzero(splits).slice(0, 0, nSplits);
                splits = torch::zeros_like(splits);
                splits.index_put_({ indices }, 1);
            }
            auto expScalesSelected = torch::exp(scales.index({splits}));
            torch::Tensor centeredSamples = torch::randn({nSplitSamples * nSplits, 3}, device);  // Nx3 of axis-aligned scales
            torch::Tensor scaledSamples = expScalesSelected.repeat({nSplitSamples, 1}) * centeredSamples;
            torch::Tensor qs = quats.index({splits}) / torch::linalg_vector_norm(quats.index({splits}), 2, { -1 }, true, torch::kFloat32);
            torch::Tensor rots = quatToRotMat(qs.repeat({nSplitSamples, 1}));
            torch::Tensor rotatedSamples = torch::bmm(rots, scaledSamples.index({"...", None})).squeeze();
            torch::Tensor splitMeans = rotatedSamples + means.index({splits}).repeat({nSplitSamples, 1});
            
            torch::Tensor splitFeaturesDc = featuresDc.index({splits}).repeat({nSplitSamples, 1, 1});
            torch::Tensor splitFeaturesRest = featuresRest.index({splits}).repeat({nSplitSamples, 1, 1});
            torch::Tensor splitOpacities;
            if( trainConfig.revisedOpacity )
            {
                auto newOpacities = 1.0 - torch::sqrt(1.0 - torch::sigmoid(opacities.index({splits})));
                splitOpacities = torch::logit(newOpacities).repeat({nSplitSamples, 1});  // [2N]
            }
            else
                splitOpacities = opacities.index({splits}).repeat({nSplitSamples, 1});
        
            const float sizeFac = 1.6f;
            torch::Tensor splitScales = torch::log(expScalesSelected / sizeFac).repeat({nSplitSamples, 1});
            scales.index_put_({splits}, torch::log(expScalesSelected / sizeFac));
            //means.index_put_({splits}, means.index({splits}) - rotatedSamples);
            torch::Tensor splitQuats = quats.index({splits}).repeat({nSplitSamples, 1});
            // Duplicate gaussians that are too small
            torch::Tensor dups = ~isHuge;
            dups &= highGrads;
            if(trainConfig.enableFocusRegion){
                dups &= focusBoxMask;
            }
            int nDups = dups.sum().item<int>();
            current_n_points = splitMeans.size(0) + means.size(0);
            if ((nDups + current_n_points) > trainConfig.capMax) {
                //Cap the number GSs
                nDups = std::min<int>(nDups, std::max<int>(0, trainConfig.capMax - current_n_points));
                auto indices = torch::nonzero(dups).slice(0, 0, nDups);
                dups = torch::zeros_like(dups);
                dups.index_put_({ indices }, 1);
            }
            torch::Tensor dupMeans = means.index({dups});
            torch::Tensor dupFeaturesDc = featuresDc.index({dups});
            torch::Tensor dupFeaturesRest = featuresRest.index({dups});
            torch::Tensor dupOpacities = opacities.index({dups});
            torch::Tensor dupScales = scales.index({dups});
            torch::Tensor dupQuats = quats.index({dups});

            means = torch::cat({means.detach(), splitMeans, dupMeans}, 0).requires_grad_();
            featuresDc = torch::cat({featuresDc.detach(), splitFeaturesDc, dupFeaturesDc}, 0).requires_grad_();
            featuresRest = torch::cat({featuresRest.detach(), splitFeaturesRest, dupFeaturesRest}, 0).requires_grad_();
            opacities = torch::cat({opacities.detach(), splitOpacities, dupOpacities}, 0).requires_grad_();
            scales = torch::cat({scales.detach(), splitScales, dupScales}, 0).requires_grad_();
            quats = torch::cat({quats.detach(), splitQuats, dupQuats}, 0).requires_grad_();
            if( trainConfig.refineScale2dStopIter > 0)
            {
                max2DSize = torch::cat({
                    max2DSize,
                    torch::zeros_like(splitScales.index({Slice(), 0})),
                    torch::zeros_like(dupScales.index({Slice(), 0}))
                }, 0);
            }
            torch::Tensor splitIdcs = torch::where(splits)[0];

            addToOptimizer(meansOpt.get(), means, splitIdcs, nSplitSamples);
            addToOptimizer(scalesOpt.get(), scales, splitIdcs, nSplitSamples);
            addToOptimizer(quatsOpt.get(), quats, splitIdcs, nSplitSamples);
            addToOptimizer(featuresDcOpt.get(), featuresDc, splitIdcs, nSplitSamples);
            addToOptimizer(featuresRestOpt.get(), featuresRest, splitIdcs, nSplitSamples);
            addToOptimizer(opacitiesOpt.get(), opacities, splitIdcs, nSplitSamples);
            
            torch::Tensor dupIdcs = torch::where(dups)[0];
            addToOptimizer(meansOpt.get(), means, dupIdcs, 1);
            addToOptimizer(scalesOpt.get(), scales, dupIdcs, 1);
            addToOptimizer(quatsOpt.get(), quats, dupIdcs, 1);
            addToOptimizer(featuresDcOpt.get(), featuresDc, dupIdcs, 1);
            addToOptimizer(featuresRestOpt.get(), featuresRest, dupIdcs, 1);
            addToOptimizer(opacitiesOpt.get(), opacities, dupIdcs, 1);

            if (trainConfig.verbose)
                std::cout << "Added " << (means.size(0) - numPointsBefore) << " gaussians, new count " << means.size(0) << std::endl;
        }

        if (doDensification){
            // Cull
            int numPointsBefore = means.size(0);

            torch::Tensor culls = (torch::sigmoid(opacities) < cullAlphaThresh).squeeze();

            if (step > trainConfig.resetAlphaEvery){
                torch::Tensor huge = std::get<0>(torch::exp(scales).max(-1)) > (trainConfig.pruneScale3d * gaussian->sceneScale);
                if (step < trainConfig.refineScale2dStopIter){
                    huge |= max2DSize > trainConfig.pruneScale2d;
                }
                culls |= huge;
            }
            if(trainConfig.enableFocusRegion){
                auto focusBox = infos["focusBox"];
                auto focusBoxMin = focusBox.data_ptr<float>();
                auto focusBoxMax = focusBoxMin + 3;
                //means[:,0] >= focusBoxMin[0] && means[:,1] >= focusBoxMin[1] && means[:2] >= focusBoxMin[2]
                auto focusBoxMask = (means.index({Slice(), 0}) >= focusBoxMin[0]) & (means.index({Slice(), 1}) >= focusBoxMin[1]) & (means.index({Slice(), 2}) >= focusBoxMin[2]) &
                               (means.index({Slice(), 0}) <= focusBoxMax[0]) & (means.index({Slice(), 1}) <= focusBoxMax[1]) & (means.index({Slice(), 2}) <= focusBoxMax[2]);
                culls &= focusBoxMask;
            }
            else{
                torch::Tensor distances = (means - gaussian->pointsCenter.to(device)).norm(2, -1);
                culls |= distances > gaussian->skyDist * 10.0f;
            }
            if(trainConfig.enableBg){
                culls.index_put_({torch::indexing::Slice(0, trainConfig.numSkyPoints)}, false); // Do not cull sky points
            }
            int cullCount = torch::sum(culls).item<int>();
            if (cullCount > 0){
                means = means.index({~culls}).detach().requires_grad_();
                scales = scales.index({~culls}).detach().requires_grad_();
                quats = quats.index({~culls}).detach().requires_grad_();
                featuresDc = featuresDc.index({~culls}).detach().requires_grad_();
                featuresRest = featuresRest.index({~culls}).detach().requires_grad_();
                opacities = opacities.index({~culls}).detach().requires_grad_();

                removeFromOptimizer(meansOpt.get(), means, culls);
                removeFromOptimizer(scalesOpt.get(), scales, culls);
                removeFromOptimizer(quatsOpt.get(), quats, culls);
                removeFromOptimizer(featuresDcOpt.get(), featuresDc, culls);
                removeFromOptimizer(featuresRestOpt.get(), featuresRest, culls);
                removeFromOptimizer(opacitiesOpt.get(), opacities, culls);
                if (trainConfig.verbose)
                    std::cout << "Culled " << (numPointsBefore - means.size(0)) << " gaussians, remaining " << means.size(0) << std::endl;
            }
        }

        if (step % resetInterval == 0){
            float resetValue = cullAlphaThresh * 2.0f;
             // opacities = torch::clamp_max(opacities, torch::logit(torch::tensor(resetValue)).item<float>());
            auto resetOpacities = torch::logit(torch::minimum(torch::sigmoid(opacities), torch::ones_like(opacities) * resetValue));
            if(trainConfig.enableBg){
               //don't reset sky points
               resetOpacities.index_put_({torch::indexing::Slice(0, trainConfig.numSkyPoints)}, opacities.index({torch::indexing::Slice(0, trainConfig.numSkyPoints)}));
            }
            opacities = resetOpacities;
             // Reset optimizer
             torch::Tensor param = opacitiesOpt->param_groups()[0].params()[0];
             #if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
                 auto pId = param.unsafeGetTensorImpl();
             #else
                 auto pId = c10::guts::to_string(param.unsafeGetTensorImpl());
             #endif    
             auto paramState = std::make_unique<AdamCustomParamState>(static_cast<AdamCustomParamState&>(*opacitiesOpt->state()[pId]));
             paramState->exp_avg(torch::zeros_like(paramState->exp_avg()));
             paramState->exp_avg_sq(torch::zeros_like(paramState->exp_avg_sq()));
             if(paramState->step_per_gaussian().defined()){
                 paramState->step_per_gaussian(torch::zeros_like(paramState->step_per_gaussian()));
             }
             if (trainConfig.verbose)
                 std::cout << "Alpha reset" << std::endl;
         }

        // Clear
        xysGradNorm = torch::Tensor();
        visCounts = torch::Tensor();
        max2DSize = torch::Tensor();

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