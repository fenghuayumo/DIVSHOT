#ifndef TENSOR_MATH_H
#define TENSOR_MATH_H

#include <torch/torch.h>
#include <tuple>
#include "gsplat_constants.hpp"
#include <glm/glm.hpp>

torch::Tensor randomQuatTensor(long long n);
torch::Tensor quatToRotMat(const torch::Tensor &quat);
std::tuple<torch::Tensor, torch::Tensor, float> autoScaleAndCenterPoses(const torch::Tensor &poses);
torch::Tensor rotationMatrix(const torch::Tensor &a, const torch::Tensor &b);
torch::Tensor rodriguesToRotation(const torch::Tensor &rodrigues);

torch::Tensor depth_to_points(
    const torch::Tensor& depths,
    const torch::Tensor& camtoworlds,
    const torch::Tensor& Ks,
    bool z_depth = true
);

torch::Tensor depth_to_normal(
    const torch::Tensor& depths,
    const torch::Tensor& camtoworlds,
    const torch::Tensor& Ks,
    bool z_depth = true
);

std::pair<torch::Tensor, torch::Tensor> pointsBounds(const torch::Tensor& points);
float pointsBoundsExtent(const torch::Tensor& points);
#endif