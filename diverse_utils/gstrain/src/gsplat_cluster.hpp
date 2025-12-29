#pragma once

#include <torch/torch.h>
#include <tuple>
#include "input_data.hpp"

void findMinimumRedundancyValue(
    const int P,
    const int *redundancy_values,
    const int *neighbours_indices,
    const bool *intersection_mask,
    int *minimum_redundancy_values,
    const int knn);

void transformCentersNDC(
    const int P,
    const float *centers,
    const float *projmatrix,
    const float *inverse_projmatrix,
    const int image_height,
    const int image_width,
    float *pixel_sizes);

void sphereEllipsoidIntersection(
    const int P,
    const float *means3D,
    const float *scales,
    const float *quats,
    const int *neighbours_indices,
    const float *sphere_radius,
    int *redundancy_values,
    bool *intersection_mask,
    const int knn);

auto intersectionTest(
        const torch::Tensor &means3D,
        const torch::Tensor &scales,
        const torch::Tensor &quats,
        const torch::Tensor &neighbours_indices,
        const torch::Tensor &sphere_radius,
        const int knn)->std::tuple<torch::Tensor, torch::Tensor>;

auto calculatePixelSize(
        const torch::Tensor& means3D,
        const torch::Tensor &w2ndc_transforms,
        const torch::Tensor &w2ndc_transforms_inverse,
        const torch::Tensor &image_height,
        const torch::Tensor &image_width)->torch::Tensor;

auto assignFinalRedundancyValue(
        const torch::Tensor &redundancy_values,
        const torch::Tensor &neighbours_indices,
        const torch::Tensor &intersection_mask,
        const int knn)->torch::Tensor;

auto calculateColourVariance(
    const torch::Tensor& means,
    const torch::Tensor& opacity,
    const torch::Tensor& scales,
    const torch::Tensor& quats,
    const torch::Tensor& shs,
    const torch::Tensor& degrees,
    const torch::Tensor& camToWorld,
    const torch::Tensor& KsMat,
    const uint32_t& height,
    const uint32_t& width,
    const int shDegree,
    bool  is2dgs = false
) -> std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>;

auto lowDistanceColorCulling(
        float threshold,
        int activeShDegree,
        torch::Tensor&  degrees,
        torch::Tensor&  featuresRest,
        torch::Tensor& colorDistances)->void;

auto lowVarianceColorCulling(
    float threshold,
    torch::Tensor& degrees,
    torch::Tensor& featuresDc,
    torch::Tensor& featuresRest,
    torch::Tensor weightedVariance, 
    torch::Tensor weightMean)->void;

auto calculateRedundancyMetric(
    torch::Tensor& means,
    torch::Tensor& opacities,
    torch::Tensor& scales,
    torch::Tensor& quats,
    std::vector<Camera>& cameras,
    int step,
    float pixelScale,
    int numNeighbours,
    float scaleFactor) -> std::tuple<torch::Tensor, torch::Tensor>;

auto cullShBands(
    torch::Tensor& means,
    torch::Tensor& scales,
    torch::Tensor& quats,
    torch::Tensor& opacities,
    torch::Tensor& degree,
    torch::Tensor& featuresDc,
    torch::Tensor& featuresRest,
    std::vector<Camera>& cam,
    int step,
    float threshold,
    float stdThreshold,
    int shDegree,
    int shDegreeInterval,
    float scaleFactor,
    bool is2gs = false) -> void;