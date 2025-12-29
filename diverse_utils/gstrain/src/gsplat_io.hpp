#pragma once
#include <string>
#include <unordered_map>
#include <torch/torch.h>

auto load_gsplat_model(const std::string& filepath, 
    torch::Tensor& means,
    torch::Tensor& opacities,
    torch::Tensor& scales,
    torch::Tensor& quats,
    torch::Tensor& sh0,
    torch::Tensor& shn,
    bool& mip_antialiased)->void;

auto save_splat_models(const std::string& filename,
    const torch::Tensor& means,
    const torch::Tensor& opacities,
    const torch::Tensor& scales,
    const torch::Tensor& quats,
    const torch::Tensor& sh0,
    const torch::Tensor& shn,
    bool mip_antialiased = false,
    bool quantised = true,
    bool halfFloat = true) ->void;

//auto produceClusters(
//    const torch::Tensor& means,
//    const torch::Tensor& scales,
//    const torch::Tensor& quats,
//    const torch::Tensor& opacities,
//    const torch::Tensor& featuresDc,
//    const torch::Tensor& featuresRest,
//    const torch::Tensor& degrees,
//    int shDegree = 3,
//    int numClusters = 256) -> std::unordered_map<std::string, tinygsplat::CodeBook>;