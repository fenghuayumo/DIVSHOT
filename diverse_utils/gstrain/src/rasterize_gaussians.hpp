#ifndef RASTERIZE_GAUSSIANS_H
#define RASTERIZE_GAUSSIANS_H

#include <torch/torch.h>
#include "tile_bounds.hpp"

using namespace torch::autograd;

#if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)

auto binAndSortGaussians(int numPoints, int numIntersects,
                        torch::Tensor xys,
                        torch::Tensor depths,
                        torch::Tensor radii,
                        torch::Tensor cumTilesHit,
                        TileBounds tileBounds)
                        ->std::tuple<torch::Tensor,
                                    torch::Tensor,
                                    torch::Tensor,
                                    torch::Tensor,
                                    torch::Tensor>;
                    
auto rasterize_to_pixels(torch::Tensor means2d,
                        torch::Tensor conics,
                        torch::Tensor colors,
                        torch::Tensor opacities,
                        torch::Tensor backgrounds,
                        torch::Tensor radii,
                        torch::Tensor depths,
                        int imgWidth,
                        int imgHeight,
                        int tileSize,
                        int C = 1,
                        bool packed = false,
                        bool absgrad = false,
                        const at::optional<torch::Tensor>& camera_ids = {},   // [nnz]
                        const at::optional<torch::Tensor>& gaussian_ids = {}, // [nnz]
                        const at::optional<torch::Tensor>& ray_ts = {}, // [nnz]
                        const at::optional<torch::Tensor>& ray_planes = {}, // [nnz]
                        const at::optional<torch::Tensor>& normals = {}, // [nnz]
                        const at::optional<torch::Tensor>& Ks = {},
                        bool geo = false
                        )->std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor>;

auto rasterize_splats_buffers_fwd(torch::Tensor means2d,
                                torch::Tensor conics,
                                torch::Tensor colors,
                                torch::Tensor opacities,
                                torch::Tensor backgrounds,
                                torch::Tensor radii,
                                torch::Tensor depths,
                                int imgWidth,
                                int imgHeight,
                                int tileSize,
                                int C = 1,
                                bool packed = false,
                                bool absgrad = false,
                                const at::optional<torch::Tensor>& camera_ids = {},   // [nnz]
                                const at::optional<torch::Tensor>& gaussian_ids = {}, // [nnz]
                                const bool splat_count = false
                                )->std::tuple<torch::Tensor, torch::Tensor,  torch::Tensor, torch::Tensor>;

class RasterizeGaussians : public Function<RasterizeGaussians>{
public:
    static tensor_list forward(AutogradContext *ctx,
                            torch::Tensor means2d, //[C, N, 2] or [nnz, 2]
                            torch::Tensor conics, //[C, N, 3] or [nnz, 3]
                            torch::Tensor colors, //[C, N, channels] or [nnz, channels]
                            torch::Tensor opacities, //[C, N] or [nnz]
                            torch::Tensor backgrounds, // [C, channels]
                            torch::Tensor isect_offsets, //[C, tile_height, tile_width]
                            torch::Tensor flatten_ids,
                            int imgWidth,
                            int imgHeight,
                            int tileSize,
                            bool absgrad = true);
    static tensor_list backward(AutogradContext *ctx, tensor_list grad_outputs);

    static torch::Tensor means2dGrad;
};

class RasterizeGaussians_wDepth : public Function<RasterizeGaussians_wDepth> {
public:
    static tensor_list forward(AutogradContext* ctx,
        torch::Tensor means2d, //[C, N, 2] or [nnz, 2]
        torch::Tensor conics, //[C, N, 3] or [nnz, 3]
        torch::Tensor colors, //[C, N, channels] or [nnz, channels]
        torch::Tensor opacities, //[C, N] or [nnz]
        torch::Tensor backgrounds, // [C, channels]
        torch::Tensor isect_offsets, //[C, tile_height, tile_width]
        torch::Tensor flatten_ids,
        torch::Tensor ray_ts,
        torch::Tensor ray_planes,
        torch::Tensor normals,
        torch::Tensor Ks,
        int imgWidth,
        int imgHeight,
        int tileSize,
        bool absgrad = true);
    static tensor_list backward(AutogradContext* ctx, tensor_list grad_outputs);
};

#endif

#endif