#include "mcmc.hpp"
#include "selective_adam.hpp"
#if defined(USE_CUDA)
#include <c10/cuda/CUDACachingAllocator.h>
#elif defined(USE_HIP)
#include <c10/hip/HIPCachingAllocator.h>
#endif
#include <exception>
#include <iostream>
#include <random>

inline int div_up(int a, int b) {
	return (a + b - 1) / b;
}

void MCMCDensify::resetState()
{
	constexpr int nMax = 51;
	binoms = torch::zeros({nMax,nMax}, torch::kFloat);
	auto binoms_accessor = binoms.accessor<float, 2>();
	for (int n = 0; n < nMax; ++n) {
		for (int k = 0; k <= n; ++k) {
			// Compute binomial coefficient C(n,k)
			float binom = 1.0f;
			for (int i = 0; i < k; ++i) {
				binom *= static_cast<float>(n - i) / static_cast<float>(i + 1);
			}
			binoms_accessor[n][k] = binom;
		}
	}
	binoms = binoms.to(gaussian->device);
}

torch::Tensor multinomial_sample(const torch::Tensor& weights, int n, bool replacement) {
	const int64_t num_elements = weights.size(0);

	// PyTorch's multinomial has a limit of 2^24 elements
	if (num_elements <= (1 << 24)) {
		return torch::multinomial(weights, n, replacement);
	}
	else {
		// For larger arrays, we need to implement sampling manually
		auto weights_normalized = weights / weights.sum();
		auto weights_cpu = weights_normalized.cpu();

		std::vector<int64_t> sampled_indices;
		sampled_indices.reserve(n);

		// Create cumulative distribution
		auto cumsum = weights_cpu.cumsum(0);
		auto cumsum_data = cumsum.accessor<float, 1>();

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(0.0, 1.0);

		for (int i = 0; i < n; ++i) {
			float u = dis(gen);
			// Binary search for the index
			int64_t idx = 0;
			int64_t left = 0, right = num_elements - 1;
			while (left <= right) {
				int64_t mid = (left + right) / 2;
				if (cumsum_data[mid] < u) {
					left = mid + 1;
				}
				else {
					idx = mid;
					right = mid - 1;
				}
			}
			sampled_indices.push_back(idx);
		}

		auto result = torch::tensor(sampled_indices, torch::kLong);
		return result.to(weights.device());
	}
}

torch::Tensor scaleDownLargestDim(const torch::Tensor& scales, float factor) {
    auto maxVal = std::get<0>(scales.max(1,true));
    auto maxMask = scales == maxVal;
	auto scale = torch::ones_like(scales,scales.device()).masked_fill(maxMask, factor);
	return scales * scale;
}

void MCMCDensify::stepAfterbackward(int step,std::unordered_map<std::string_view, torch::Tensor>& infos)
{
	torch::NoGradGuard noGrad;
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
	const auto& trainConfig = gaussian->trainConfig;
	if (step < trainConfig.refineStopIter && step > trainConfig.warmupLength && step % trainConfig.refineEvery == 0)
	{
		//reloctae gs
		const int nMax = binoms.size(0);
		auto tOpacity = torch::sigmoid(opacities).squeeze(-1);
		auto deadMask = tOpacity <= trainConfig.minOpacity;
		auto isHuge = (std::get<0>(scales.exp().max(-1)) > (trainConfig.pruneScale3d * gaussian->sceneScale * 10)).squeeze();
		deadMask |= isHuge;
		deadMask |= (quats * quats).sum(-1) < 1e-8f;
		torch::Tensor focusBoxMask;
		if(trainConfig.enableFocusRegion){
			auto focusBox = infos["focusBox"];
			auto focusBoxMin = focusBox.data_ptr<float>();
			auto focusBoxMax = focusBoxMin + 3;
			focusBoxMask = (means.index({Slice(), 0}) < focusBoxMin[0]) & (means.index({Slice(), 1}) < focusBoxMin[1]) & (means.index({Slice(), 2}) < focusBoxMin[2]) &
			(means.index({Slice(), 0}) > focusBoxMax[0]) & (means.index({Slice(), 1}) > focusBoxMax[1]) & (means.index({Slice(), 2}) > focusBoxMax[2]);
			deadMask |= focusBoxMask;
		}
		else{
			torch::Tensor distances = (means - gaussian->pointsCenter.to(device)).norm(2, -1);
			deadMask |= distances > gaussian->skyDist * 10.0f;
		}
		auto deadIndices = deadMask.nonzero().squeeze(-1);
		auto nRelocatedSplats = deadIndices.numel();
		if (nRelocatedSplats > 0)
		{
			auto aliveIndices = (~deadMask).nonzero().squeeze(-1);
			if( aliveIndices.numel() > 0 )
			{
				if (trainConfig.enableBg) {
					tOpacity.index_put_({ torch::indexing::Slice(0, trainConfig.numSkyPoints) }, 0);
				}
				// sample new gs indices
				auto probs = tOpacity.index_select(0, aliveIndices);
				auto sampledIdxs = multinomial_sample(probs, nRelocatedSplats, true);
				sampledIdxs = aliveIndices.index_select(0, sampledIdxs);
				tOpacity = tOpacity.index_select(0, sampledIdxs).contiguous();
				auto tScales = torch::exp(scales).index_select(0, sampledIdxs).contiguous();
				auto ratios = torch::bincount(sampledIdxs).index({ sampledIdxs }) + 1;
				ratios = torch::clamp_max_(ratios, nMax);
				ratios = ratios.to(torch::kInt).contiguous();
				auto [newOpaciticies, newScales] = gsplat::relocation(
					tOpacity,
					tScales,
					ratios,
					binoms,
					nMax);
				// newOpaciticies = torch::clamp(newOpaciticies, trainConfig.minOpacity, 1.0 - eps);
				newOpaciticies = torch::clamp_(newOpaciticies, trainConfig.minOpacity, 1.0f - trainConfig.minOpacity);
				newOpaciticies = torch::logit(newOpaciticies);
				newScales = torch::log(scaleDownLargestDim(newScales, 0.5));

				opacities.index_put_({sampledIdxs}, newOpaciticies.unsqueeze(-1));
				scales.index_put_({sampledIdxs}, newScales);

				means.index_put_({deadIndices}, means.index_select(0, sampledIdxs)).requires_grad_();
				scales.index_put_({deadIndices}, scales.index_select(0, sampledIdxs)).requires_grad_();
				opacities.index_put_({ deadIndices }, opacities.index_select(0, sampledIdxs)).requires_grad_();
				quats.index_put_({ deadIndices }, quats.index_select(0, sampledIdxs)).requires_grad_();
				featuresDc.index_put_({ deadIndices }, featuresDc.index_select(0, sampledIdxs)).requires_grad_();
				featuresRest.index_put_({ deadIndices }, featuresRest.index_select(0, sampledIdxs)).requires_grad_();
				auto optimizeFunc = [&](torch::Tensor& v)->torch::Tensor{
					v.index_put_({sampledIdxs}, 0);
					v.index_put_({deadIndices}, 0);
					return v;
				};
				updateParamWithOptimizer(meansOpt.get(), means, optimizeFunc);
				updateParamWithOptimizer(scalesOpt.get(), scales, optimizeFunc);
				updateParamWithOptimizer(quatsOpt.get(), quats, optimizeFunc);
				updateParamWithOptimizer(opacitiesOpt.get(), opacities, optimizeFunc);
				updateParamWithOptimizer(featuresDcOpt.get(), featuresDc, optimizeFunc);
				updateParamWithOptimizer(featuresRestOpt.get(), featuresRest, optimizeFunc);

				if (trainConfig.verbose)
				{
					std::cout << std::format("Step {}: Relocated: {} Splats.\n", step, nRelocatedSplats);
				}
			}
		}
		//add gs
		auto currentNpoints = means.size(0);
		auto frac = 1.0f + 0.2f / std::max<int>(1, div_up(currentNpoints, 50000));
		// auto frac = 1.05f;
		auto nTarget = std::min<int>(trainConfig.capMax, frac * currentNpoints);
		auto nAddSplats = std::max<int>(0, nTarget - currentNpoints);
		if (nAddSplats > 0)
		{
			tOpacity = torch::sigmoid(opacities).squeeze(-1);
			if(trainConfig.enableFocusRegion){
				tOpacity *= (~focusBoxMask).to(torch::kInt);
			}
			if (trainConfig.enableBg) {
				tOpacity.index_put_({ torch::indexing::Slice(0, trainConfig.numSkyPoints) }, 0);
			}
			auto probs = tOpacity;
			auto sampledIdxs = multinomial_sample(probs,nAddSplats, true);
			tOpacity = tOpacity.index_select(0, sampledIdxs).contiguous();
			auto tScales = torch::exp(scales).index_select(0, sampledIdxs).contiguous();
			auto ratios = torch::bincount(sampledIdxs).index({ sampledIdxs }) + 1;
			ratios = torch::clamp_max_(ratios, nMax);
			ratios = ratios.to(torch::kInt).contiguous();
			auto [newOpaciticies, newScales] = gsplat::relocation(
				tOpacity,
				tScales,
				ratios,
				binoms,
				nMax);
			// newOpaciticies = torch::clamp(newOpaciticies, trainConfig.minOpacity, 1.0 - eps);
			newOpaciticies = torch::clamp_(newOpaciticies, trainConfig.minOpacity, 1.0f - trainConfig.minOpacity);
			newOpaciticies = torch::logit(newOpaciticies);
			newScales = torch::log(scaleDownLargestDim(newScales, 0.5));
			opacities.index_put_({ sampledIdxs }, newOpaciticies.unsqueeze(-1));
			scales.index_put_({ sampledIdxs }, newScales);
			
			means = torch::cat({means, means.index_select(0, sampledIdxs)}).requires_grad_();
			scales = torch::cat({ scales, scales.index_select(0, sampledIdxs) }).requires_grad_();
			quats = torch::cat({ quats, quats.index_select(0, sampledIdxs) }).requires_grad_();
			opacities = torch::cat({ opacities, opacities.index_select(0, sampledIdxs) }).requires_grad_();
			featuresDc = torch::cat({ featuresDc, featuresDc.index_select(0, sampledIdxs) }).requires_grad_();
			featuresRest = torch::cat({ featuresRest, featuresRest.index_select(0, sampledIdxs) }).requires_grad_();

			auto optimizeFunc = [&](torch::Tensor& v)->torch::Tensor {
				auto shapes = v.sizes().vec();
				shapes[0] = sampledIdxs.size(0);
				auto vnew = torch::zeros(shapes, v.device());
				return torch::cat({v, vnew});
			};
			updateParamWithOptimizer(meansOpt.get(), means, optimizeFunc);
			updateParamWithOptimizer(scalesOpt.get(), scales, optimizeFunc);
			updateParamWithOptimizer(quatsOpt.get(), quats, optimizeFunc);
			updateParamWithOptimizer(opacitiesOpt.get(), opacities, optimizeFunc);
			updateParamWithOptimizer(featuresDcOpt.get(), featuresDc, optimizeFunc);
			updateParamWithOptimizer(featuresRestOpt.get(), featuresRest, optimizeFunc);
			if (trainConfig.verbose)
			{
				std::cout << std::format("Step {} Added {} Splats. Now having {} Splats.\n", step, nAddSplats, means.size(0));
			}
		}
		if (device != torch::kCPU)
		{
#ifdef USE_HIP
			c10::hip::HIPCachingAllocator::emptyCache();
#elif defined(USE_CUDA)
			c10::cuda::CUDACachingAllocator::emptyCache();
#endif
		}
	}
	//injectNoise
	//if (step < trainConfig.refineStopIter && step > trainConfig.warmupLength)
	{
		auto noise = torch::randn_like(means);
		if (trainConfig.enableBg) {
			noise.index_put_({ torch::indexing::Slice(0, trainConfig.numSkyPoints) }, 0);
		}
		auto current_lr = gaussian->meansOptScheduler->getlr(step) * trainConfig.noiselr;
		gsplat::add_noise(
			opacities,
			scales,
			quats,
			noise,
			means,
			current_lr);
		means.requires_grad_(true);
	}
}