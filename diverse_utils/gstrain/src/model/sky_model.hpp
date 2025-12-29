#pragma once

#ifndef SKY_MODEL_H
#define SKY_MODEL_H

#include <iostream>
#include <array>
#include <torch/torch.h>
#include <torch/csrc/api/include/torch/version.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <gaussian_train_config.hpp>

class SkyGaussianModel
{
public:
    SkyGaussianModel(int num_sky_points, torch::Tensor points3D);
    ~SkyGaussianModel();

    torch::Tensor points;
    torch::Tensor sky_center;
    torch::Tensor rgbs;
    float         sky_dist = 0.0;
};

#endif