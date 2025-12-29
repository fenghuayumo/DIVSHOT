#pragma once
#include <torch/torch.h>
using namespace torch::autograd;

class FusedSSIMMap : public Function<FusedSSIMMap>{
public:
    static tensor_list forward(AutogradContext *ctx,
                float C1, 
                float C2, 
                torch::Tensor img1, 
                torch::Tensor img2, 
                const std::string_view& padding = "same",
                bool train = true);
    static tensor_list backward(AutogradContext *ctx, tensor_list grad_outputs);
};

torch::Tensor fused_ssim(
            torch::Tensor img1, 
            torch::Tensor img2, 
            const std::string_view& padding = "same",
            bool train = true);