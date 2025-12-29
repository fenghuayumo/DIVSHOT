#include "rasterize_gaussians.hpp"
#include "gsplat.hpp"
#if defined(USE_MPS)
#define assert(...)  
#endif
#if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)

auto isect_tiles(torch::Tensor means2d,
    torch::Tensor radii,
    torch::Tensor depths,
    int tile_size,
    int tile_width,
    int tile_height,
    bool sort = true,
    bool packed = true,
    const at::optional<int>& n_cameras = {},
    const at::optional<torch::Tensor>& camera_ids = {},   // [nnz]
    const at::optional<torch::Tensor>& gaussian_ids = {}, // [nnz]
    const at::optional<torch::Tensor>& conics = {}, // [C,N,3] or [nnz,3]
    const at::optional<torch::Tensor>& opacities = {} // [C,N] or [nnz]
) -> std::tuple<torch::Tensor, torch::Tensor, torch::Tensor>
{
    torch::NoGradGuard no_grad;
    int C = 0;
    if (packed)
    { 
        auto nnz = means2d.size(0);
        assert(means2d.size(0) == nnz && means2d.size(1) == 2);
        assert(radii.size(0) == nnz);
        assert(depths.size(0) == nnz);
        assert(camera_ids.has_value(), "camera_ids is required if packed is True");
        assert(gaussian_ids.has_value(), "gaussian_ids is required if packed is True");
        assert(n_cameras.has_value(), "n_cameras is required if packed is True");
        C = n_cameras.value();
    }
    else
    {
        auto N = means2d.size(1);
        C = means2d.size(0);
        assert(means2d.size(0) == C && means2d.size(1) ==  N && means2d.size(2) == 2 );
        assert(radii.size(0) == C && radii.size(1) == N);
        assert(depths.size(0) == C && depths.size(1) == N);
    }

    return gsplat::intersect_tile(
        means2d.contiguous(),
        radii.contiguous(),
        depths.contiguous(),
        camera_ids,
        gaussian_ids,
        conics,
        opacities,
        C,
        tile_size,
        tile_width,
        tile_height,
        sort
    );
}

auto isect_offset_encode(
    torch::Tensor  isect_ids,
    int n_cameras,
    int tile_width,
    int tile_height) -> torch::Tensor
{
    torch::NoGradGuard no_grad;
    return gsplat::intersect_offset(isect_ids.contiguous(), n_cameras, tile_width, tile_height); // [C, tile_height, tile_width]
}

auto rasterize_to_pixels(
    torch::Tensor means2d,
    torch::Tensor conics,
    torch::Tensor colors,
    torch::Tensor opacities,
    torch::Tensor backgrounds,
    torch::Tensor radii,
    torch::Tensor depths,
    int imgWidth,
    int imgHeight,
    int tileSize,
    int C,
    bool packed,
    bool absgrad,
    const at::optional<torch::Tensor>& camera_ids,   // [nnz]
    const at::optional<torch::Tensor>& gaussian_ids,
    const at::optional<torch::Tensor>& ray_ts, //
    const at::optional<torch::Tensor>& ray_planes, //
    const at::optional<torch::Tensor>& normals, //
    const at::optional<torch::Tensor>& Ks,
    bool bgeo)-> std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor>
{
    const int tile_width = static_cast<int>(std::ceil(imgWidth / float(tileSize)));
    const int tile_height = static_cast<int>(std::ceil(imgHeight / float(tileSize)));
    auto [tiles_per_gauss, isect_ids, flatten_ids] = isect_tiles(
        means2d,
        radii,
        depths,
        tileSize,
        tile_width,
        tile_height,
        true,
        packed,
        C,
        camera_ids,
        gaussian_ids,
        conics,
        opacities
    );
    torch::Tensor isect_offsets = isect_offset_encode(isect_ids, C, tile_width, tile_height); // [C, tile_height, tile_width]
    if (bgeo) 
    {
        auto ret = RasterizeGaussians_wDepth::apply(
            means2d.contiguous(),
            conics.contiguous(),
            colors.contiguous(),
            opacities.contiguous(),
            backgrounds,
            isect_offsets.contiguous(),
            flatten_ids.contiguous(),
            ray_ts.value(),
            ray_planes.value(),
            normals.value(),
            Ks.value(),
            imgWidth,
            imgHeight,
            tileSize,
            absgrad
        );
        return { ret[0], ret[1], ret[2] / ret[1].clamp_min(1e-10), ret[3], torch::nn::functional::normalize(ret[4],torch::nn::functional::NormalizeFuncOptions().dim(-1))};
    }
    auto ret = RasterizeGaussians::apply(
        means2d.contiguous(),
        conics.contiguous(),
        colors.contiguous(),
        opacities.contiguous(),
        backgrounds,
        isect_offsets.contiguous(),
        flatten_ids.contiguous(),
        imgWidth,
        imgHeight,
        tileSize,
        absgrad
    );
    torch::Tensor none;
    return {ret[0], ret[1], none, none, none};
}

auto rasterize_splats_buffers_fwd(
    torch::Tensor means2d,
    torch::Tensor conics,
    torch::Tensor colors,
    torch::Tensor opacities,
    torch::Tensor backgrounds,
    torch::Tensor radii,
    torch::Tensor depths,
    int imgWidth,
    int imgHeight,
    int tileSize,
    int C,
    bool packed,
    bool absgrad,
    const at::optional<torch::Tensor>& camera_ids,   // [nnz]
    const at::optional<torch::Tensor>& gaussian_ids,
    const bool splat_count)-> std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor>
{
    const int tile_width = static_cast<int>(std::ceil(imgWidth / float(tileSize)));
    const int tile_height = static_cast<int>(std::ceil(imgHeight / float(tileSize)));
    auto [tiles_per_gauss, isect_ids, flatten_ids] = isect_tiles(
        means2d,
        radii,
        depths,
        tileSize,
        tile_width,
        tile_height,
        true,
        packed,
        C,
        camera_ids,
        gaussian_ids,
        {},
        {}
    );
    torch::Tensor isect_offsets = isect_offset_encode(isect_ids, C, tile_width, tile_height); // [C, tile_height, tile_width]
    C = isect_offsets.size(0);

    uint32_t channels = colors.size(-1);
    if (channels > 513 or channels == 0)
        throw std::runtime_error(std::format("Unsupported number of color channels {}", channels));
    int padded_channels;
    const std::vector<int> n_channels = { 1,2,3, 4,5,8, 9,16, 17, 32,33,64, 65, 128,129,256,257,512,513};
    if( std::find(n_channels.begin(), n_channels.end(), channels) == n_channels.end())
    {
        padded_channels = (1 << std::bit_width(channels - 1u) ) - channels;
        auto shapes = colors.sizes().slice(0, colors.sizes().size() - 1).vec();
        shapes.push_back(padded_channels);
        colors = torch::cat({colors, torch::zeros(shapes, colors.options())}, - 1);
        if (backgrounds.defined())
        {
            auto background_shapes = backgrounds.sizes().slice(0, backgrounds.sizes().size() - 1).vec();
            background_shapes.push_back(padded_channels);
			backgrounds = torch::cat({backgrounds, torch::zeros(background_shapes, backgrounds.options())}, -1);
        }
    }
    else
    {
		padded_channels = 0;
	}
    auto [rgb,renderAlpha,touchPixels,splatT] = gsplat::rasterize_3dsplats_count_fwd(
            means2d,
            conics,
            colors,
            opacities,
            backgrounds,
            {},
            imgWidth,
            imgHeight,
            tileSize,
            isect_offsets,
            flatten_ids,
            splat_count
        );
    return { rgb,renderAlpha,touchPixels,splatT };
}

torch::Tensor RasterizeGaussians::means2dGrad;

auto RasterizeGaussians::forward(AutogradContext *ctx,
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
                        bool absgrad)->tensor_list
 {
    
    const int C = isect_offsets.size(0);

    uint32_t channels = colors.size(-1);
    if (channels > 513 or channels == 0)
        throw std::runtime_error(std::format("Unsupported number of color channels {}", channels));
    int padded_channels;
    const std::vector<int> n_channels = { 1,2,3, 4,5,8, 9,16, 17, 32,33,64, 65, 128,129,256,257,512,513};
    if( std::find(n_channels.begin(), n_channels.end(), channels) == n_channels.end())
    {
        padded_channels = (1 << std::bit_width(channels - 1u) ) - channels;
        auto shapes = colors.sizes().slice(0, colors.sizes().size() - 1).vec();
        shapes.push_back(padded_channels);
        colors = torch::cat({colors, torch::zeros(shapes, colors.options())}, - 1);
        if (backgrounds.defined())
        {
            auto background_shapes = backgrounds.sizes().slice(0, backgrounds.sizes().size() - 1).vec();
            background_shapes.push_back(padded_channels);
			backgrounds = torch::cat({backgrounds, torch::zeros(background_shapes, backgrounds.options())}, -1);
        }
    }
    else
    {
		padded_channels = 0;
	}
    auto tile_height = isect_offsets.size(1);
    auto tile_width = isect_offsets.size(2);
    assert(tile_height * tileSize >= imgHeight, std::format("Assert Failed: {} * {} >= {}", tile_height, tileSize, imgHeight));
    assert(tile_width * tileSize >= imgWidth, std::format("Assert Failed: {} * {} >= {}", tile_width, tileSize, imgWidth));

    //auto bucket_offsets = gsplat::extract_bucket_indices(isect_offsets, flatten_ids.size(0), tile_width, tile_height);
    auto [render_colors, render_alphas, lastIds] = gsplat::rasterize_to_pixels_3dgs_fwd(
        means2d,
        conics,
        colors,
        opacities,
        backgrounds,
        {},
        imgWidth,
        imgHeight,
        tileSize,
        isect_offsets,
        flatten_ids
    );
    // auto [render_colors, render_alphas, tile_max_n_contributions, tile_n_contributions, bucket_tile_index, bucket_color_transmittance] = gsplat::rasterize_to_pixels_bucket_3dgs_fwd(
    //     isect_offsets,
    //     bucket_offsets,
    //     flatten_ids,
    //     means2d,
    //     conics,
    //     opacities,
    //     colors,
    //     imgWidth,
    //     imgHeight
    // );
    ctx->save_for_backward({ means2d,
            conics,
            colors,
            opacities,
            backgrounds,
            isect_offsets,
            flatten_ids,
            render_colors,
            render_alphas,
            lastIds});
    ctx->saved_data["imgWidth"] = imgWidth;
    ctx->saved_data["imgHeight"] = imgHeight;
    ctx->saved_data["tileSize"] = tileSize;
    ctx->saved_data["absgrad"] = absgrad;
    if (padded_channels > 0) {
        auto sizes = render_colors.sizes().vec();
        sizes.back() -= padded_channels;
        render_colors = torch::narrow(render_colors, /*dim=*/-1, /*start=*/0, /*length=*/sizes.back());
    }
    return { render_colors, render_alphas};
}

tensor_list RasterizeGaussians::backward(AutogradContext *ctx, tensor_list grad_outputs) {
    torch::Tensor v_render_colors = grad_outputs[0];
    torch::Tensor v_render_alphas = grad_outputs[1];
    int imgHeight = ctx->saved_data["imgHeight"].toInt();
    int imgWidth = ctx->saved_data["imgWidth"].toInt();
    const auto tileSize = ctx->saved_data["tileSize"].toInt();
    const auto absgrad = ctx->saved_data["absgrad"].toBool();

    variable_list saved = ctx->get_saved_variables();
    torch::Tensor means2d = saved[0];
    torch::Tensor conics = saved[1];
    torch::Tensor colors = saved[2];
    torch::Tensor opacities = saved[3];
    torch::Tensor backgrounds = saved[4];
    torch::Tensor isect_offsets = saved[5];
    torch::Tensor flatten_ids = saved[6];
    torch::Tensor render_colors = saved[7];
    torch::Tensor render_alphas = saved[8];
    torch::Tensor last_ids = saved[9];
    auto t = gsplat::rasterize_to_pixels_3dgs_bwd(means2d,
                                           conics,
                                           colors,
                                           opacities,
                                           backgrounds,
                                           {},
                                           imgWidth,
                                           imgHeight,
                                           tileSize,
                                           isect_offsets,
                                           flatten_ids,
                                           render_alphas,
                                           last_ids, 
                                           v_render_colors.contiguous(),
                                           v_render_alphas.contiguous(),
                                           absgrad);
//     torch::Tensor bucket_offsets = saved[9];
//     torch::Tensor tile_max_n_contributions = saved[10];
//     torch::Tensor tile_n_contributions = saved[11];
//     torch::Tensor bucket_tile_index = saved[12];
//     torch::Tensor bucket_color_transmittance = saved[13];
//    auto t = gsplat::rasterize_to_pixels_bucket_3dgs_bwd(
//         means2d,
//         conics,
//         colors,
//         opacities,
//         backgrounds,
//         {},
//         imgWidth,
//         imgHeight,
//         isect_offsets,
//         bucket_offsets,
//         flatten_ids,
//         render_colors,
//         render_alphas,
//         tile_max_n_contributions,
//         tile_n_contributions,
//         bucket_tile_index,
//         bucket_color_transmittance,
//         v_render_colors,
//         v_render_alphas,
//         absgrad
//     );
    torch::Tensor v_means2d_abs = std::get<0>(t);
    torch::Tensor v_means2d = std::get<1>(t);
    torch::Tensor v_conics = std::get<2>(t);
    torch::Tensor v_colors = std::get<3>(t);
    torch::Tensor v_opacities = std::get<4>(t);
    if (absgrad) {
        means2dGrad = v_means2d_abs;
    }else{
        means2dGrad = v_means2d;
    }
    torch::Tensor v_backgrounds;
    if (ctx->needs_input_grad(4)) {
        v_backgrounds = (v_render_colors * (1.0 - render_alphas)).sum({1,2});
    }

    torch::Tensor none;
    return { v_means2d,
            v_conics,
            v_colors,
            v_opacities,
            v_backgrounds,
            none,
            none,
            none,
            none,
            none,
            none
    };
}

auto RasterizeGaussians_wDepth::forward(AutogradContext* ctx,
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
    bool absgrad)->tensor_list
{

    const int C = isect_offsets.size(0);

    uint32_t channels = colors.size(-1);
    if (channels > 513 or channels == 0)
        throw std::runtime_error(std::format("Unsupported number of color channels {}", channels));
    int padded_channels;
    const std::vector<int> n_channels = { 1,2,3, 4,5,8, 9,16, 17, 32,33,64, 65, 128,129,256,257,512,513 };
    if (std::find(n_channels.begin(), n_channels.end(), channels) == n_channels.end())
    {
        padded_channels = (1 << std::bit_width(channels - 1u)) - channels;
        auto shapes = colors.sizes().slice(0, colors.sizes().size() - 1).vec();
        shapes.push_back(padded_channels);
        colors = torch::cat({ colors, torch::zeros(shapes, colors.options()) }, -1);
        if (backgrounds.defined())
        {
            auto background_shapes = backgrounds.sizes().slice(0, backgrounds.sizes().size() - 1).vec();
            background_shapes.push_back(padded_channels);
            backgrounds = torch::cat({ backgrounds, torch::zeros(background_shapes, backgrounds.options()) }, -1);
        }
    }
    else
    {
        padded_channels = 0;
    }
    auto tile_height = isect_offsets.size(1);
    auto tile_width = isect_offsets.size(2);
    assert(tile_height * tileSize >= imgHeight, std::format("Assert Failed: {} * {} >= {}", tile_height, tileSize, imgHeight));
    assert(tile_width * tileSize >= imgWidth, std::format("Assert Failed: {} * {} >= {}", tile_width, tileSize, imgWidth));

    auto [renderColors, renderAlphas, lastIds, expectedDepth, medianDepth, renderNormals,medianIds] = gsplat::rasterize_to_pixels_3dgs_w_depth_fwd(
        means2d,
        conics,
        colors,
        opacities,
        ray_ts,
        ray_planes,
        normals,
        backgrounds,
        {},
        imgWidth,
        imgHeight,
        tileSize,
        Ks,
        isect_offsets,
        flatten_ids
    );
    ctx->save_for_backward({ means2d,
            conics,
            colors,
            opacities,
            backgrounds,
            isect_offsets,
            flatten_ids,
            renderAlphas,
            lastIds,
            ray_ts,
            ray_planes,
            normals,
            Ks,
            medianIds});
    ctx->saved_data["imgWidth"] = imgWidth;
    ctx->saved_data["imgHeight"] = imgHeight;
    ctx->saved_data["tileSize"] = tileSize;
    ctx->saved_data["absgrad"] = absgrad;
    if (padded_channels > 0) {
        auto sizes = renderColors.sizes().vec();
        sizes.back() -= padded_channels;
        renderColors = torch::narrow(renderColors, /*dim=*/-1, /*start=*/0, /*length=*/sizes.back());
    }
    return { renderColors, renderAlphas, expectedDepth, medianDepth, renderNormals};
}

tensor_list RasterizeGaussians_wDepth::backward(AutogradContext* ctx, tensor_list grad_outputs) {
    torch::Tensor v_render_colors = grad_outputs[0];
    torch::Tensor v_render_alphas = grad_outputs[1];
    torch::Tensor v_expected_depth = grad_outputs[2];
    torch::Tensor v_median_depth = grad_outputs[3];
    torch::Tensor v_expected_normals = grad_outputs[4];

    int imgHeight = ctx->saved_data["imgHeight"].toInt();
    int imgWidth = ctx->saved_data["imgWidth"].toInt();
    const auto tileSize = ctx->saved_data["tileSize"].toInt();
    const auto absgrad = ctx->saved_data["absgrad"].toBool();

    variable_list saved = ctx->get_saved_variables();
    torch::Tensor means2d = saved[0];
    torch::Tensor conics = saved[1];
    torch::Tensor colors = saved[2];
    torch::Tensor opacities = saved[3];
    torch::Tensor backgrounds = saved[4];
    torch::Tensor isectOffsets = saved[5];
    torch::Tensor flattenIds = saved[6];
    torch::Tensor renderAlphas = saved[7];
    torch::Tensor lastIds = saved[8];
    torch::Tensor ray_ts = saved[9];
    torch::Tensor ray_planes = saved[10];
    torch::Tensor normals = saved[11];
    torch::Tensor Ks = saved[12];
    torch::Tensor medianIds = saved[13];

    auto t = gsplat::rasterize_to_pixels_3dgs_w_depth_bwd(means2d,
        conics,
        colors,
        opacities,
        ray_ts,
        ray_planes,
        normals,
        backgrounds,
        {},
        imgWidth,
        imgHeight,
        tileSize,
        Ks,
        isectOffsets,
        flattenIds,
        renderAlphas,
        lastIds,
        medianIds,
        v_render_colors.contiguous(),
        v_render_alphas.contiguous(),
        v_expected_depth.contiguous(),
        v_median_depth.contiguous(),
        v_expected_normals.contiguous(),
        absgrad);

    torch::Tensor v_means2d_abs = std::get<0>(t);
    torch::Tensor v_means2d = std::get<1>(t);
    torch::Tensor v_conics = std::get<2>(t);
    torch::Tensor v_colors = std::get<3>(t);
    torch::Tensor v_opacities = std::get<4>(t);
    torch::Tensor v_ray_ts = std::get<5>(t);
    torch::Tensor v_ray_planes = std::get<6>(t);
    torch::Tensor v_normals = std::get<7>(t);
    if (absgrad) {
        RasterizeGaussians::means2dGrad = v_means2d_abs;
    }else{
        RasterizeGaussians::means2dGrad = v_means2d;
    }
    torch::Tensor v_backgrounds;
    if (ctx->needs_input_grad(4)) {
        v_backgrounds = (v_render_colors * (1.0 - renderAlphas)).sum({ 1,2 });
    }

    torch::Tensor none;
    return { v_means2d,
            v_conics,
            v_colors,
            v_opacities,
            v_backgrounds,
            none,
            none,
            v_ray_ts,
            v_ray_planes,
            v_normals,
            none, //Ks
            none, //width
            none,//height,
            none,//tilesize
            none //absgrad
    };
}

#endif
