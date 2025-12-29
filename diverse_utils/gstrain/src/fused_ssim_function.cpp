#include "fused_ssim_function.hpp"

#if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)

#if defined(USE_HIP) || defined(USE_CUDA)
#include <rasterizer/gsplat-cuda/ops.h>
#endif

#if defined(USE_MPS)
#include <rasterizer/gsplat-metal/ops.h>
#endif

using namespace torch::indexing;

tensor_list FusedSSIMMap::forward(AutogradContext *ctx,
            float C1, 
            float C2, 
            torch::Tensor img1, 
            torch::Tensor img2,
            const std::string_view& padding,
            bool train)
{
    auto [ssim_map, dm_dmu1, dm_dsigma1_sq, dm_dsigma12] = gsplat::fusedssim(C1, C2, img1, img2, train);
    if (padding == "valid")
    {
        ssim_map = ssim_map.index({Slice(), Slice(), Slice(5, -5), Slice(5, -5)});
    }
    ctx->save_for_backward({ img1.detach(), img2, dm_dmu1, dm_dsigma1_sq, dm_dsigma12 });
    ctx->saved_data["C1"] = C1;
    ctx->saved_data["C2"] = C2;
    ctx->saved_data["padding"] = padding.data();
    return {ssim_map};
}

tensor_list FusedSSIMMap::backward(AutogradContext *ctx, tensor_list grad_outputs)
{
    auto saved = ctx->get_saved_variables();
    auto img1 = saved[0];
    auto img2 = saved[1];
    auto dm_dmu1 = saved[2];
    auto dm_dsigma1_sq = saved[3];
    auto dm_dsigma12 = saved[4];
    auto C1 = ctx->saved_data["C1"].toDouble();
    auto C2 = ctx->saved_data["C2"].toDouble();
    auto padding = ctx->saved_data["padding"].toStringView();
    auto dL_dmap = grad_outputs[0];
    if (padding == "valid")
    {
        dL_dmap = torch::zeros_like(img1);
        dL_dmap.index_put_({Slice(), Slice(), Slice(5, -5), Slice(5, -5)}, grad_outputs[0]);
	}
	auto grad = gsplat::fusedssim_backward(C1, C2, img1, img2, dL_dmap, dm_dmu1, dm_dsigma1_sq, dm_dsigma12);
	return {torch::Tensor(), torch::Tensor(), grad, torch::Tensor(), torch::Tensor(), torch::Tensor()};
}

torch::Tensor fused_ssim(torch::Tensor img1, torch::Tensor img2, const std::string_view& padding, bool train)
{
	float C1 = 0.01 * 0.01;
	float C2 = 0.03 * 0.03;
	auto map = FusedSSIMMap::apply(C1, C2, img1, img2, padding, train);
	return map[0].mean();
}

#endif
