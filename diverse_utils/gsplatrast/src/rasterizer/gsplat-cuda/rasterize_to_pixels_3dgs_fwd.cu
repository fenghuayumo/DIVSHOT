#include <ATen/Dispatch.h>
#include <ATen/core/Tensor.h>
#include <c10/cuda/CUDAStream.h>
#include <cooperative_groups.h>

#include "common.h"
#include "rasterization.h"

namespace gsplat {

namespace cg = cooperative_groups;

////////////////////////////////////////////////////////////////
// Forward
////////////////////////////////////////////////////////////////
#define GAMMA 0.5
template <uint32_t CDIM, bool GEO = false, bool Count = false>
__global__ void rasterize_to_pixels_3dgs_fwd_kernel(
    const uint32_t C,
    const uint32_t N,
    const uint32_t n_isects,
    const bool packed,
    const vec2 *__restrict__ means2d,         // [C, N, 2] or [nnz, 2]
    const vec3 *__restrict__ conics,          // [C, N, 3] or [nnz, 3]
    const float *__restrict__ colors,      // [C, N, CDIM] or [nnz, CDIM]
    const float *__restrict__ opacities,   // [C, N] or [nnz]
    const float *__restrict__ ray_ts,        // [C, N] or [nnz]
    const vec2 *__restrict__ ray_planes,    // [C, N, 2] or [nnz, 2]
    const vec3 *__restrict__ normals,       // [C, N, 3] or [nnz, 3]
    const float* __restrict__ Ks, // [C, 3, 3]
    const float *__restrict__ backgrounds, // [C, CDIM]
    const bool *__restrict__ masks,           // [C, tile_height, tile_width]
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_size,
    const uint32_t tile_width,
    const uint32_t tile_height,
    const int32_t *__restrict__ tile_offsets, // [C, tile_height, tile_width]
    const int32_t *__restrict__ flatten_ids,  // [n_isects]
    float
        *__restrict__ render_colors, // [C, image_height, image_width, CDIM]
    float *__restrict__ render_alphas, // [C, image_height, image_width, 1]
    float *__restrict__ render_depths, // [C, image_height, image_width, 1]
    float *__restrict__ median_depths, // [C, image_height, image_width, 1]
    float *__restrict__ render_normals,// [C, image_height, image_width, 3]
    int32_t *__restrict__ median_ids, // [C, image_height, image_width]
    int32_t *__restrict__ last_ids, // [C, image_height, image_width]
    int32_t* __restrict__ touchedPixels,
	float* __restrict__ splatT
    
) {
    // each thread draws one pixel, but also timeshares caching gaussians in a
    // shared tile

    auto block = cg::this_thread_block();
    int32_t camera_id = block.group_index().x;
    int32_t tile_id =
        block.group_index().y * tile_width + block.group_index().z;
    uint32_t i = block.group_index().y * tile_size + block.thread_index().y;
    uint32_t j = block.group_index().z * tile_size + block.thread_index().x;

    tile_offsets += camera_id * tile_height * tile_width;
    render_colors += camera_id * image_height * image_width * CDIM;
    render_alphas += camera_id * image_height * image_width;
    last_ids += camera_id * image_height * image_width;
    if (backgrounds != nullptr) {
        backgrounds += camera_id * CDIM;
    }
    if (masks != nullptr) {
        masks += camera_id * tile_height * tile_width;
    }

    float px = (float)j + 0.5f;
    float py = (float)i + 0.5f;
    int32_t pix_id = i * image_width + j;
    float ln;
    if constexpr (GEO)
    {
        Ks += camera_id * 9;
        float fx = Ks[0], cx = Ks[2], fy = Ks[4], cy = Ks[5];
        vec3 pixnf = {(px - cx) / fx, (py - cy) / fy, 1};
        ln = glm::length(pixnf);
    }

    // return if out of bounds
    // keep not rasterizing threads around for reading data
    bool inside = (i < image_height && j < image_width);
    bool done = !inside;

    // when the mask is provided, render the background color and return
    // if this tile is labeled as False
    if (masks != nullptr && inside && !masks[tile_id]) {
#pragma unroll
        for (uint32_t k = 0; k < CDIM; ++k) {
            render_colors[pix_id * CDIM + k] =
                backgrounds == nullptr ? 0.0f : backgrounds[k];
        }
        return;
    }

    // have all threads in tile process the same gaussians in batches
    // first collect gaussians between range.x and range.y in batches
    // which gaussians to look through in this tile
    int32_t range_start = tile_offsets[tile_id];
    int32_t range_end =
        (camera_id == C - 1) && (tile_id == tile_width * tile_height - 1)
            ? n_isects
            : tile_offsets[tile_id + 1];
    const uint32_t block_size = block.size();
    uint32_t num_batches =
        (range_end - range_start + block_size - 1) / block_size;

    extern __shared__ int s[];
    int32_t *id_batch = (int32_t *)s; // [block_size]
    vec3 *xy_opacity_batch =
        reinterpret_cast<vec3 *>(&id_batch[block_size]); // [block_size]
    vec3 *conic_batch =
        reinterpret_cast<vec3 *>(&xy_opacity_batch[block_size]); // [block_size]
    float *ray_t_batch;
    vec2 *ray_plane_batch;
    vec3 *normal_batch;
    if constexpr (GEO)
    {
        ray_t_batch = 
            reinterpret_cast<float *>(&conic_batch[block_size]); // [block_size]
        ray_plane_batch =
            reinterpret_cast<vec2 *>(&ray_t_batch[block_size]); // [block_size]
        normal_batch =
            reinterpret_cast<vec3 *>(&ray_plane_batch[block_size]); // [block_size]
    }
    // current visibility left to render
    // transmittance is gonna be used in the backward pass which requires a high
    // numerical precision so we use double for it. However double make bwd 1.5x
    // slower so we stick with float for now.
    float T = 1.0f;
    // index of most recent gaussian to write to this thread's pixel
    uint32_t cur_idx = 0;
    uint32_t median_idx = -1;
    // collect and process batches of gaussians
    // each thread loads one gaussian at a time before rasterizing its
    // designated pixel
    uint32_t tr = block.thread_rank();

    float pix_out[CDIM] = {0.f};
    float t_out = 0.f;
    float normal_out[3] = {0.f};
    float t_median = 0.f;
    for (uint32_t b = 0; b < num_batches; ++b) {
        // resync all threads before beginning next batch
        // end early if entire tile is done
        if (__syncthreads_count(done) >= block_size) {
            break;
        }

        // each thread fetch 1 gaussian from front to back
        // index of gaussian to load
        uint32_t batch_start = range_start + block_size * b;
        uint32_t idx = batch_start + tr;
        if (idx < range_end) {
            int32_t g = flatten_ids[idx]; // flatten index in [C * N] or [nnz]
            id_batch[tr] = g;
            const vec2 xy = means2d[g];
            const float opac = opacities[g];
            xy_opacity_batch[tr] = {xy.x, xy.y, opac};
            conic_batch[tr] = conics[g];
            if constexpr (GEO)
            {
                ray_t_batch[tr] = ray_ts[g];
                ray_plane_batch[tr] = ray_planes[g];
                normal_batch[tr] = normals[g];
            }
        }

        // wait for other threads to collect the gaussians in batch
        block.sync();

        // process gaussians in the current batch for this pixel
        uint32_t batch_size = min(block_size, range_end - batch_start);
        for (uint32_t t = 0; (t < batch_size) && !done; ++t) {
            const vec3 conic = conic_batch[t];
            const vec3 xy_opac = xy_opacity_batch[t];
            const float opac = xy_opac.z;
            const vec2 delta = {xy_opac.x - px, xy_opac.y - py};
            const float sigma = 0.5f * (conic.x * delta.x * delta.x +
                                        conic.z * delta.y * delta.y) +
                                conic.y * delta.x * delta.y;
            float alpha = min(0.999f, opac * __expf(-sigma));
            if (sigma < 0.f || alpha < ALPHA_THRESHOLD) {
                continue;
            }

            const float next_T = T * (1.0f - alpha);
            if (next_T <= 1e-4f) { // this pixel is done: exclusive
                done = true;
                break;
            }

            int32_t g = id_batch[t];
            const float vis = alpha * T;
            const float *c_ptr = colors + g * CDIM;
#pragma unroll
            for (uint32_t k = 0; k < CDIM; ++k) {
                pix_out[k] += c_ptr[k] * vis;
            }
            if constexpr (GEO)
            {
                const float ray_t = ray_t_batch[t];
                const vec2 ray_plane = ray_plane_batch[t];
                const vec3 normal = normal_batch[t];
#pragma unroll
                for (uint32_t k = 0; k < 3; ++k) {
                    normal_out[k] += normal[k] * vis;
                }

                float t_opt = ray_t + glm::dot(delta, ray_plane);
                t_out += t_opt * vis;
                if (T > 0.5)
                {
                    median_idx = batch_start + t;
                    t_median = t_opt;
                }
            }
            if constexpr (Count){
                // touchedPixels[g]++;
                // splatT[g] += opac;
                if (touchedPixels)
                    atomicAdd(&touchedPixels[g],1);
                if (splatT)
                    atomicAdd(&splatT[g], opac);
            }
            else {
                if( touchedPixels )
                    atomicAdd(&touchedPixels[g],1);
                if( splatT ){
                    float trans = pow(alpha, 2 * GAMMA) * pow(T, 2 - 2 * GAMMA);
                    atomicAdd(&splatT[g], trans);
                }
            }
            cur_idx = batch_start + t;

            T = next_T;
        }
    }

    if (inside) {
        // Here T is the transmittance AFTER the last gaussian in this pixel.
        // We (should) store double precision as T would be used in backward
        // pass and it can be very small and causing large diff in gradients
        // with float32. However, double precision makes the backward pass 1.5x
        // slower so we stick with float for now.
        render_alphas[pix_id] = 1.0f - T;
#pragma unroll
        for (uint32_t k = 0; k < CDIM; ++k) {
            render_colors[pix_id * CDIM + k] =
                backgrounds == nullptr ? pix_out[k]
                                       : (pix_out[k] + T * backgrounds[k]);
        }
        if constexpr (GEO)
        {
            render_depths[pix_id] = t_out / ln;
            median_depths[pix_id] = t_median / ln;
            median_ids[pix_id] = median_idx;
            #pragma unroll
            for (uint32_t k = 0; k < 3; ++k){
                render_normals[pix_id * 3 + k] = normal_out[k];
            }
        }
        // index in bin of last gaussian in this pixel
        last_ids[pix_id] = static_cast<int32_t>(cur_idx);
    }
}

template <uint32_t CDIM, bool GEO>
void launch_rasterize_to_pixels_3dgs_fwd_kernel(
    // Gaussian parameters
    const at::Tensor means2d,   // [C, N, 2] or [nnz, 2]
    const at::Tensor conics,    // [C, N, 3] or [nnz, 3]
    const at::Tensor colors,    // [C, N, channels] or [nnz, channels]
    const at::Tensor opacities, // [C, N]  or [nnz]
    const at::optional<at::Tensor> ray_ts,
    const at::optional<at::Tensor> ray_planes,
    const at::optional<at::Tensor> normals,
    const at::optional<at::Tensor> Ks,
    const at::optional<at::Tensor> backgrounds, // [C, channels]
    const at::optional<at::Tensor> masks,       // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_size,
    // intersections
    const at::Tensor tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor flatten_ids,  // [n_isects]
    // outputs
    at::Tensor renders, // [C, image_height, image_width, channels]
    at::Tensor alphas,  // [C, image_height, image_width]
    at::Tensor last_ids, // [C, image_height, image_width]
    at::Tensor expected_depths, 
    at::Tensor median_depths, 
    at::Tensor expected_normals, 
    at::Tensor median_ids,
    at::optional<at::Tensor> touchedPixel,
    at::optional<at::Tensor> splatT,
    bool count
) {
    bool packed = means2d.dim() == 2;

    uint32_t C = tile_offsets.size(0);         // number of cameras
    uint32_t N = packed ? 0 : means2d.size(1); // number of gaussians
    uint32_t tile_height = tile_offsets.size(1);
    uint32_t tile_width = tile_offsets.size(2);
    uint32_t n_isects = flatten_ids.size(0);

    // Each block covers a tile on the image. In total there are
    // C * tile_height * tile_width blocks.
    dim3 threads = {tile_size, tile_size, 1};
    dim3 grid = {C, tile_height, tile_width};
    int64_t shmem_size =
        tile_size * tile_size * (GEO ? sizeof(int32_t) + sizeof(vec3) + sizeof(vec3) + sizeof(float) + sizeof(vec2) + sizeof(vec3)
            : sizeof(int32_t) + sizeof(vec3) + sizeof(vec3));

    // TODO: an optimization can be done by passing the actual number of
    // channels into the kernel functions and avoid necessary global memory
    // writes. This requires moving the channel padding from python to C side.
   
    if (touchedPixel.has_value()) {
        if (cudaFuncSetAttribute(
            rasterize_to_pixels_3dgs_fwd_kernel<CDIM, false>,
            cudaFuncAttributeMaxDynamicSharedMemorySize,
            shmem_size
        ) != cudaSuccess) {
            AT_ERROR(
                "Failed to set maximum shared memory size (requested ",
                shmem_size,
                " bytes), try lowering tile_size."
            );
        }
        if(count){
            rasterize_to_pixels_3dgs_fwd_kernel<CDIM,false, true>
                << <grid, threads, shmem_size, at::cuda::getCurrentCUDAStream() >> > (
                    C,
                    N,
                    n_isects,
                    packed,
                    reinterpret_cast<vec2*>(means2d.data_ptr<float>()),
                    reinterpret_cast<vec3*>(conics.data_ptr<float>()),
                    colors.data_ptr<float>(),
                    opacities.data_ptr<float>(),
                    GEO ? ray_ts.value().data_ptr<float>() : nullptr,
                    GEO ? reinterpret_cast<vec2*>(ray_planes.value().data_ptr<float>()) : nullptr,
                    GEO ? reinterpret_cast<vec3*>(normals.value().data_ptr<float>()) : nullptr,
                    GEO ? Ks.value().data_ptr<float>() : nullptr,
                    backgrounds.has_value() ? backgrounds.value().data_ptr<float>()
                    : nullptr,
                    masks.has_value() ? masks.value().data_ptr<bool>() : nullptr,
                    image_width,
                    image_height,
                    tile_size,
                    tile_width,
                    tile_height,
                    tile_offsets.data_ptr<int32_t>(),
                    flatten_ids.data_ptr<int32_t>(),
                    renders.data_ptr<float>(),
                    alphas.data_ptr<float>(),
                    GEO ? expected_depths.data_ptr<float>() : nullptr,
                    GEO ? median_depths.data_ptr<float>() : nullptr,
                    GEO ? expected_normals.data_ptr<float>() : nullptr,
                    GEO ? median_ids.data_ptr<int32_t>() : nullptr,
                    last_ids.data_ptr<int32_t>(),
                    touchedPixel.value().data_ptr<int32_t>(),
                    splatT.value().data_ptr<float>()
                    );
        }
        else {
            rasterize_to_pixels_3dgs_fwd_kernel<CDIM,false, false>
                << <grid, threads, shmem_size, at::cuda::getCurrentCUDAStream() >> > (
                    C,
                    N,
                    n_isects,
                    packed,
                    reinterpret_cast<vec2*>(means2d.data_ptr<float>()),
                    reinterpret_cast<vec3*>(conics.data_ptr<float>()),
                    colors.data_ptr<float>(),
                    opacities.data_ptr<float>(),
                    GEO ? ray_ts.value().data_ptr<float>() : nullptr,
                    GEO ? reinterpret_cast<vec2*>(ray_planes.value().data_ptr<float>()) : nullptr,
                    GEO ? reinterpret_cast<vec3*>(normals.value().data_ptr<float>()) : nullptr,
                    GEO ? Ks.value().data_ptr<float>() : nullptr,
                    backgrounds.has_value() ? backgrounds.value().data_ptr<float>()
                    : nullptr,
                    masks.has_value() ? masks.value().data_ptr<bool>() : nullptr,
                    image_width,
                    image_height,
                    tile_size,
                    tile_width,
                    tile_height,
                    tile_offsets.data_ptr<int32_t>(),
                    flatten_ids.data_ptr<int32_t>(),
                    renders.data_ptr<float>(),
                    alphas.data_ptr<float>(),
                    GEO ? expected_depths.data_ptr<float>() : nullptr,
                    GEO ? median_depths.data_ptr<float>() : nullptr,
                    GEO ? expected_normals.data_ptr<float>() : nullptr,
                    GEO ? median_ids.data_ptr<int32_t>() : nullptr,
                    last_ids.data_ptr<int32_t>(),
                    touchedPixel.value().data_ptr<int32_t>(),
                    splatT.value().data_ptr<float>()
                    );
        }
    }else{
        if (cudaFuncSetAttribute(
            rasterize_to_pixels_3dgs_fwd_kernel<CDIM, GEO, false>,
            cudaFuncAttributeMaxDynamicSharedMemorySize,
            shmem_size
        ) != cudaSuccess) {
            AT_ERROR(
                "Failed to set maximum shared memory size (requested ",
                shmem_size,
                " bytes), try lowering tile_size."
            );
        }
        rasterize_to_pixels_3dgs_fwd_kernel<CDIM, GEO, false>
            <<<grid, threads, shmem_size, at::cuda::getCurrentCUDAStream()>>>(
                C,
                N,
                n_isects,
                packed,
                reinterpret_cast<vec2 *>(means2d.data_ptr<float>()),
                reinterpret_cast<vec3 *>(conics.data_ptr<float>()),
                colors.data_ptr<float>(),
                opacities.data_ptr<float>(),
                GEO ? ray_ts.value().data_ptr<float>() : nullptr,
                GEO ? reinterpret_cast<vec2*>(ray_planes.value().data_ptr<float>()) : nullptr,
                GEO ? reinterpret_cast<vec3*>(normals.value().data_ptr<float>()) : nullptr,
                GEO ? Ks.value().data_ptr<float>() : nullptr,
                backgrounds.has_value() ? backgrounds.value().data_ptr<float>()
                                        : nullptr,
                masks.has_value() ? masks.value().data_ptr<bool>() : nullptr,
                image_width,
                image_height,
                tile_size,
                tile_width,
                tile_height,
                tile_offsets.data_ptr<int32_t>(),
                flatten_ids.data_ptr<int32_t>(),
                renders.data_ptr<float>(),
                alphas.data_ptr<float>(),
                GEO ? expected_depths.data_ptr<float>() : nullptr,
                GEO ? median_depths.data_ptr<float>() : nullptr,
                GEO ? expected_normals.data_ptr<float>() : nullptr,
                GEO ? median_ids.data_ptr<int32_t>() : nullptr,
                last_ids.data_ptr<int32_t>(),
                nullptr,
                nullptr
            );
        }
}

// Explicit Instantiation: this should match how it is being called in .cpp
// file.
// TODO: this is slow to compile, can we do something about it?
#define __INS__(CDIM,FLAG)                                                          \
    template void launch_rasterize_to_pixels_3dgs_fwd_kernel<CDIM,FLAG>(            \
        const at::Tensor means2d,                                              \
        const at::Tensor conics,                                               \
        const at::Tensor colors,                                               \
        const at::Tensor opacities,                                            \
        const at::optional<at::Tensor> ray_ts,                                 \
        const at::optional<at::Tensor> ray_planes,                                 \
        const at::optional<at::Tensor> ray_normals,                                 \
        const at::optional<at::Tensor> Ks,                                 \
        const at::optional<at::Tensor> backgrounds,                            \
        const at::optional<at::Tensor> masks,                                  \
        uint32_t image_width,                                                  \
        uint32_t image_height,                                                 \
        uint32_t tile_size,                                                    \
        const at::Tensor tile_offsets,                                         \
        const at::Tensor flatten_ids,                                          \
        at::Tensor renders,                                                    \
        at::Tensor alphas,                                                     \
        at::Tensor last_ids,                                                    \
        at::Tensor expected_depths,                                              \
        at::Tensor median_depths,                                              \
        at::Tensor expected_normals,                                           \
        at::Tensor median_ids,                                                   \
        at::optional<at::Tensor> touchedPixel,                                  \
        at::optional<at::Tensor> splatT,                                         \
        bool count                                                             \
);

__INS__(1,false)
__INS__(2,false)
__INS__(3,false)
__INS__(4,false)
// __INS__(5,false)
// __INS__(8,false)
// __INS__(9,false)
// __INS__(16,false)
// __INS__(17,false)
// __INS__(32,false)
// __INS__(33,false)
// __INS__(64,false)
// __INS__(65,false)
// __INS__(128,false)
// __INS__(129,false)
// __INS__(256,false)
// __INS__(257,false)
// __INS__(512,false)
// __INS__(513,false)

__INS__(1,true)
__INS__(2,true)
__INS__(3,true)
__INS__(4,true)
// __INS__(5,true)
// __INS__(8,true)
// __INS__(9,true)
// __INS__(16,true)
// __INS__(17,true)
// __INS__(32,true)
// __INS__(33,true)
// __INS__(64,true)
// __INS__(65,true)
// __INS__(128,true)
// __INS__(129,true)
// __INS__(256,true)
// __INS__(257,true)
// __INS__(512,true)
// __INS__(513,true)
#undef __INS__

} // namespace gsplat
