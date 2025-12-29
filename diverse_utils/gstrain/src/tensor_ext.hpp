#ifndef TENSOR_EXT_HPP
#define TENSOR_EXT_HPP
#include <torch/torch.h>

struct AbsGradTensor : public torch::Tensor {
    AbsGradTensor& operator=(const torch::Tensor& tensor) {
        torch::Tensor::operator=(tensor);
        return *this;
    }
    torch::Tensor absgrad;

};

#endif