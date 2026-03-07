#include <filesystem>
#include "model.hpp"
#include "light_gaussian.hpp"
#include "quatized_model.hpp"
#include "default.hpp"
#include "mcmc.hpp"
#include "tile_bounds.hpp"
#include "project_gaussians.hpp"
#include "rasterize_gaussians.hpp"
#include "fused_rasterize_gaussians.hpp"
#include <tensor_math.hpp>
#include "gsplat.hpp"
#include <gsplat_io.hpp>
#ifdef USE_HIP
#include <c10/hip/HIPCachingAllocator.h>
#elif defined(USE_CUDA)
#include <c10/cuda/CUDACachingAllocator.h>
#endif
#include <fused_ssim_function.hpp>
#include "selective_adam.hpp"
#include "sky_model.hpp"

namespace fs = std::filesystem;

int numShBases(int degree){
    switch(degree){
        case 0:
            return 1;
        case 1:
            return 4;
        case 2:
            return 9;
        case 3:
            return 16;
        default:
            return 25;
    }
}

torch::Tensor projectionMatrix(float zNear, float zFar, float fovX, float fovY, const torch::Device &device){
    // OpenGL perspective projection matrix
    float t = zNear * std::tan(0.5f * fovY);
    float b = -t;
    float r = zNear * std::tan(0.5f * fovX);
    float l = -r;
    return torch::tensor({
        {2.0f * zNear / (r - l), 0.0f, (r + l) / (r - l), 0.0f},
        {0.0f, 2 * zNear / (t - b), (t + b) / (t - b), 0.0f},
        {0.0f, 0.0f, (zFar + zNear) / (zFar - zNear), -1.0f * zFar * zNear / (zFar - zNear)},
        {0.0f, 0.0f, 1.0f, 0.0f}
    }, device);
}

torch::Tensor psnr(const torch::Tensor& rendered, const torch::Tensor& gt){
    torch::Tensor mse = (rendered - gt).pow(2).mean();
    return (10.f * torch::log10(1.0 / mse));
}

torch::Tensor l1(const torch::Tensor& rendered, const torch::Tensor& gt){
    return torch::abs(gt - rendered).mean();
}
torch::Tensor l1(const torch::Tensor& rendered, const torch::Tensor& gt, const torch::Tensor& mask){
    if( mask.numel() ) {
      return (torch::abs(gt - rendered) * mask).mean();
    }
    return torch::abs(gt - rendered).mean();
}

extern float getSceneExtent(const std::vector<Camera>& cam_infos);
extern float getSceneExtent(const std::vector<Camera>& cam_infos,const torch::Tensor& points);
enum class PruneType
{
    Reduced = 0,
    LightSplat
};

#define USE_FUSED_GS 1

GaussianTrainModel::GaussianTrainModel(
    const std::filesystem::path& filename, 
    const InputData& inputData,
    int numCameras,
    GaussianTrainConfig& config,
    const torch::Device& device) :
    numCameras(numCameras),
    trainConfig(config),
    device(device), ssim(11, 3) {

    configSkyModel(inputData);
    torch::manual_seed(42);
    load(filename.string());
    backgroundColor = torch::tensor({0.0f, 0.0f, 0.0f}, device); // Black
    //backgroundColor = torch::tensor({ 0.6130f, 0.0101f, 0.3984f }, device); // Nerf Studio default
    updateBounds(0.8f);
}

GaussianTrainModel::GaussianTrainModel(
    const InputData& inputData, 
    int numCameras,
    GaussianTrainConfig& config,
    const torch::Device& device) :
    numCameras(numCameras),
    trainConfig(config),
    device(device), ssim(11, 3) {

    auto [ptxyz, ptrgb] = configSkyModel(inputData);
    auto numPoints = ptxyz.size(0);
    torch::manual_seed(42);

    means = ptxyz.to(device).contiguous().requires_grad_();
    scales = (PointsTensor(ptxyz).scales().clamp(1e-3,0.1 * skyDist)).repeat({ 1, 3 }).log().to(device).contiguous().requires_grad_();
    quats = randomQuatTensor(numPoints).to(device).contiguous().requires_grad_();

    int dimSh = numShBases(trainConfig.shDegree);
    torch::Tensor shs = torch::zeros({ numPoints, dimSh, 3 }, torch::TensorOptions().dtype(torch::kFloat32).device(device));

    shs.index({ Slice(), 0, Slice(None, 3) }) = rgb2sh(ptrgb).toType(torch::kFloat32);
    shs.index({ Slice(), Slice(1, None), Slice(3, None) }) = 0.0f;

    featuresDc = shs.index({ Slice(), 0, Slice() }).reshape({ numPoints, 1, 3 }).to(device).contiguous().requires_grad_();
    featuresRest = shs.index({ Slice(), Slice(1, None), Slice() }).to(device).contiguous().requires_grad_();
    opacities = torch::logit(0.1f * torch::ones({ numPoints, 1 })).to(device).contiguous().requires_grad_();

    backgroundColor = torch::tensor({ 0.0f, 0.0f, 0.0f }, device); // Black
    //backgroundColor = torch::tensor({0.6130f, 0.0101f, 0.3984f}, device); // Nerf Studio default

    updateBounds(0.8f);
}

auto GaussianTrainModel::configSkyModel(const InputData& inputData)->std::pair<torch::Tensor, torch::Tensor>
{
    long long numPoints = 0;

    torch::Tensor ptxyz;
    torch::Tensor ptrgb;
    if (trainConfig.randomInitPoints) {
        numPoints = 100000;
        auto sceneExtent = getSceneExtent(inputData.cameras);
        ptxyz = sceneExtent * (torch::rand({ numPoints,3}) * 2 - 1);
        ptrgb = torch::rand({ numPoints,3 });
    }
    else{
        numPoints = inputData.points.xyz.size(0);
        ptxyz = inputData.points.xyz;
        ptrgb = inputData.points.rgb.toType(torch::kFloat64) / 255.0;
    }
    if(trainConfig.enableBg){
        SkyGaussianModel skyModel(trainConfig.numSkyPoints, ptxyz);
        ptxyz = torch::cat({ skyModel.points,ptxyz}, 0);
        ptrgb = torch::cat({ skyModel.rgbs,ptrgb}, 0);
        numPoints = ptxyz.size(0);
        skyDist = skyModel.sky_dist;
        pointsCenter = skyModel.sky_center;
    }
    else {
        // Compute the mean of points3D
        pointsCenter = ptxyz.mean(0).unsqueeze(0);
        // Compute the sky distance
        torch::Tensor distances = (ptxyz - pointsCenter).norm(2, -1);
        skyDist = torch::quantile(distances, 0.97).item<float>() * 10.0f;
    }

    scale = inputData.scale;
    translation = inputData.translation;
    if(!inputData.cameras.empty()){
        std::cout << "use gut rasterize: " << isUseGutRasterize(inputData.cameras[0]) << std::endl;
    }
    return {ptxyz, ptrgb};
}

auto GaussianTrainModel::createPruneStrategy(int prune_type)->void {
   /* if( prune_type == (int)(PruneType::Reduced) ){
        pruneStrategy = std::make_unique<ReducedSplatModel>(this);
        std::cout << "select Reduced Prune Strategy!\n";
    }else {*/
        pruneStrategy = std::make_unique<LightSplatModel>(this);
        std::cout << "select Light Guassian Prune Strategy!\n";
    //}
}
auto GaussianTrainModel::setDensifyStrategy(int type)->void
{
    if(trainConfig.randomInitPoints && !densifyStrategy){
        densifyStrategy = std::make_unique<MCMCDensify>(this);
        trainConfig.densifyStrategy = (int)(SplatDensifyType::SplatMCMC);
        std::cout << "select SplatMCMC Densify Strategy!\n";
    }
    else if (type == (int)(SplatDensifyType::SplatMCMC) ) {
        if (!densifyStrategy) {
            densifyStrategy = std::make_unique<MCMCDensify>(this);
        }
        else if (densifyStrategy && densifyStrategy->type != (int)(SplatDensifyType::SplatMCMC)) {
            densifyStrategy = std::make_unique<MCMCDensify>(this);
        }
        std::cout << "select SplatMCMC Densify Strategy!\n";
    }
    else if (type == (int)(SplatDensifyType::SplatADC)) {
        if( !densifyStrategy){
            densifyStrategy = std::make_unique<DefaultDensify>(this);
        }
        else if(densifyStrategy && densifyStrategy->type != (int)(SplatDensifyType::SplatADC)){
            densifyStrategy = std::make_unique<DefaultDensify>(this);
        }
        std::cout << "select SplatADC Densify Strategy!\n";
    }
    densifyStrategy->resetState();
}

auto GaussianTrainModel::isUseGutRasterize(const Camera& cam)->bool
{
    if(cam.model_type == dvs::CameraModelType::FISHEYE ||
      cam.radial_distortion.defined() ||
      cam.tangential_distortion.defined() )
    {
        return true;
    }
    return false;
}

auto GaussianTrainModel::forward(Camera& cam, int step, GSplatRenderMode renderMode, GSRasterizeMode rasterizeMode)-> 
    std::tuple<std::unordered_map<std::string_view, torch::Tensor>, std::unordered_map<std::string_view, torch::Tensor>> {
    constexpr int tileSize = 16;
    constexpr int channel_chunk = 32;
    constexpr int C = 1;
    const float scaleFactor = trainConfig.progressiveTrain ? getDownscaleFactor(step) : 1;
    const int height = static_cast<int>(static_cast<float>(cam.height) / scaleFactor);
    const int width = static_cast<int>(static_cast<float>(cam.width) / scaleFactor);

    lastHeight = height;
    lastWidth = width;

    auto viewMat = cam.worldToCam;
    auto T = cam.T;
    
    std::unordered_map<std::string_view, torch::Tensor> infos;
    std::unordered_map<std::string_view, torch::Tensor> render_pkg;
    const auto rade = trainConfig.normalConsistencyLoss && step > trainConfig.refineStopIter;
    const auto antialiased = rasterizeMode == GSRasterizeMode::Antialiased;
    const auto abs_grad = trainConfig.useAbsGrad && trainConfig.densifyStrategy == SplatDensifyType::SplatADC;
    torch::Tensor ksMat = cam.getIntrinsicsMatrix(scaleFactor);

    c10::optional<torch::Tensor> cameraIds = {};
    c10::optional<torch::Tensor> gaussianIds = {};
    torch::Tensor conics;
    torch::Tensor depths; // GPU-only
    torch::Tensor compensations; // GPU-only
    torch::Tensor rgb, render_alpha;
    torch::Tensor xys, radii,t_opacity;
    torch::Tensor ray_ts, ray_planes, normals;
    torch::Tensor render_median_depth, render_depth, render_normals;
    auto camera_model = static_cast<gsplat::CameraModelType>(cam.model_type);
    int degreesToUse = std::min<int>(step / trainConfig.shDegreeInterval, trainConfig.shDegree);
     auto background = backgroundColor;
     if (renderMode == GSplatRenderMode::RGBD || renderMode == GSplatRenderMode::RGBED) {
         if (background.defined())
             background = torch::cat({ backgroundColor.unsqueeze(0).repeat({C,1}), torch::zeros({C, 1},background.options()) }, -1).to(device);
     }
     else if (renderMode == GSplatRenderMode::Depth || renderMode == GSplatRenderMode::EDepth) {
         if (background.defined())
             background = torch::zeros({ C,1 }).to(torch::kFloat).to(device);
     }
    // sparse_grad: when true, backward returns sparse COO tensors for gradients
    // This is more efficient as it only computes gradients for visible gaussians
    // Requires packed=true (PackTileID) to have gaussian_ids for sparse indexing
    const bool packed_mode = static_cast<bool>(trainConfig.packLevel & GSPackLevel::PackTileID);
    const bool request_sparse_grad = static_cast<bool>(trainConfig.packLevel & GSPackLevel::PackSparseGrad);
    // sparse_grad can only work when packed mode is enabled (PackTileID provides gaussian_ids)
    const bool use_sparse_grad = packed_mode && request_sparse_grad;
    
    auto fused_settings = FusedRasterizationSettings{
       width,
       height,
       16,         // tile_size
       0.3f,       // eps2d
       0.01f,      // near_plane
       1e10f,      // far_plane
       0.0f,       // radius_clip
       antialiased,
       camera_model,
       packed_mode, // packed: only enabled by PackTileID, not by PackF32ToU8
       rade,       // calc_gof
       use_sparse_grad, // sparse_grad: requires packed mode
       abs_grad    // absgrad
   };
    
   {
#if USE_FUSED_GS > 0
        auto t = FusedRasterizeGaussians::apply(
            means,
            scales,
            quats,
            opacities,
            featuresDc,
            featuresRest,
            viewMat.unsqueeze(0),
            ksMat.unsqueeze(0),
            T.transpose(0, 1).to(device),
            background,
            degreesToUse,
            fused_settings
        );

        rgb = t[0];
        render_alpha = t[1];
        radii = t[2];
        depths = t[3];
        gaussianIds = t[4];
#else
        {
            #if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)
            if( !(trainConfig.packLevel & GSPackLevel::PackTileID) ){
                auto p = ProjectGaussians::apply(means,
                                scales,
                                quats,
                                opacities,
                                viewMat.unsqueeze(0),
                                ksMat.unsqueeze(0),
                                height,
                                width,
                                eps2d,
                                nearPlane,
                                farPlane,
                                radiusClip,
                                antialiased,
                                camera_model,
                                rade);
                radii = p[0];
                xys = p[1];
                depths = p[2];
                conics = p[3];
                t_opacity = p[4];
            }
            else {
                auto p = ProjectGaussiansPacked::apply(means,
                                scales,
                                quats,
                                opacities,
                                viewMat.unsqueeze(0),
                                ksMat.unsqueeze(0),
                                height,
                                width,
                                eps2d,
                                nearPlane,
                                farPlane,
                                radiusClip,
                                antialiased,
                                camera_model,
                                use_sparse_grad,
                                rade);
                cameraIds = p[0];
                gaussianIds = p[1];
                radii = p[2];
                xys = p[3];
                depths = p[4];
                conics = p[5];
                t_opacity = p[6];
            }
            #endif
    }

     torch::Tensor rgbs;
     {
         #if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)
         if( trainConfig.packLevel & GSPackLevel::PackTileID ){
             torch::Tensor viewDirs = means.detach().index_select(0, *gaussianIds) - T.transpose(0, 1).to(device);
             torch::Tensor masks = (radii > 0).all(-1); //[nnz]
             torch::Tensor sh0_coeffs,shN_coeffs;
             if( featuresRest.dim() == 3){
                 sh0_coeffs = featuresDc.index_select(0, gaussianIds.value()); // [nnz, 1, 3]
                 shN_coeffs = featuresRest.index_select(0, gaussianIds.value()); // [nnz, K-1, 3]
             }
             else{
                 sh0_coeffs = featuresDc.index_select(0, gaussianIds.value()); // [nnz, 1, 3]
                 shN_coeffs = featuresRest.index_select(0, gaussianIds.value()); // [nnz, K-1, 3]
             }
 			rgbs = SphericalHarmonics::apply(degreesToUse, viewDirs.contiguous(), sh0_coeffs.contiguous(), shN_coeffs.contiguous(), masks.contiguous());
 		}
 		else{
            torch::Tensor viewDirs = means.detach() - T.transpose(0, 1).to(device);
            torch::Tensor masks = (radii > 0).all(-1).squeeze(0);
 			rgbs = SphericalHarmonics::apply(degreesToUse, viewDirs.contiguous(), featuresDc.contiguous(), featuresRest.contiguous(), masks.contiguous());
            rgbs = rgbs.unsqueeze(0);
 		}
         #endif
     }
    
     rgbs = torch::clamp_min(rgbs + 0.5f, 0.0f);
     if (renderMode == GSplatRenderMode::RGBD || renderMode == GSplatRenderMode::RGBED) {
         rgbs = torch::cat({ rgbs, depths.unsqueeze(-1) }, -1);
     }
     else if (renderMode == GSplatRenderMode::Depth || renderMode == GSplatRenderMode::EDepth) {
         rgbs = depths.unsqueeze(-1);
     }
     {  
         #if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)
         {
             auto t = rasterize_to_pixels(
                 xys,
                 conics,
                 rgbs,
                 t_opacity,
                 background,
                 radii,
                 depths,
                 width,
                 height,
                 tileSize,
                 C,
                 trainConfig.packLevel & GSPackLevel::PackTileID,
                 abs_grad,
                 cameraIds,
                 gaussianIds,
                 ray_ts,
                 ray_planes,
                 normals,
                 ksMat.unsqueeze(0),
                 rade
             );
             rgb = std::get<0>(t);
             render_alpha = std::get<1>(t);
             render_depth = std::get<2>(t);
         }
         #endif
     }
#endif
    }
     rgb = rgb.squeeze(0);
     infos["gaussianIds"] = gaussianIds.has_value() ? gaussianIds.value() : torch::Tensor();
     infos["radii"] = radii;
     infos["viewZ"] = depths;
     if ( (renderMode == GSplatRenderMode::RGBD || renderMode == GSplatRenderMode::RGBED)) {
         auto split_tensors = torch::split(rgb, { 3, 1 }, -1);
         rgb = split_tensors[0];
         if(!render_depth.defined())
             render_depth = split_tensors[1];
     }
     rgb = torch::clamp_max(rgb, 1.0f);
    //  infos["xys"] = xys;
     render_pkg["colors"] = rgb;
     render_pkg["alphas"] = render_alpha.squeeze(0);
     if(render_depth.defined())
         render_pkg["depths"] = render_depth;
    //render_pkg["colors"] = image;
    return {render_pkg,infos};
}

auto GaussianTrainModel::renderSplats(Camera &cam, int step, bool splatCount, GSplatRenderMode renderMode,
    GSRasterizeMode mode)->std::tuple<torch::Tensor,torch::Tensor, torch::Tensor>
{
    torch::NoGradGuard noGrad;
    const float scaleFactor = trainConfig.progressiveTrain ? getDownscaleFactor(step) : 1;
    const int height = static_cast<int>(static_cast<float>(cam.height) / scaleFactor);
    const int width = static_cast<int>(static_cast<float>(cam.width) / scaleFactor);

    auto viewMat = cam.worldToCam;
    auto T = cam.T;

    torch::Tensor ksMat = cam.getIntrinsicsMatrix(scaleFactor);
    auto camera_model = static_cast<gsplat::CameraModelType>(cam.model_type);
    auto [rgb, renderAlpha, radii, touchedPixels, splatT] = gsplat::render_3dsplats_count_fwd(
        means,
        scales,
        quats,
        opacities,
        3,
        featuresDc,
        featuresRest,
        T,
        viewMat.unsqueeze(0),
        ksMat.unsqueeze(0),
        backgroundColor,
        height,
        width,
        0.3f,
        0.01f,
        1e10f,
        0.0f,
        camera_model,
        splatCount
    );
    return {radii, touchedPixels, splatT};
}

auto GaussianTrainModel::evalSplats(
    const Camera &cam, 
    int step,
    GSplatRenderMode renderMode,
    GSRasterizeMode rasterizeMode) ->
    std::unordered_map<std::string_view, torch::Tensor>
{
    torch::NoGradGuard no_grad;
    constexpr int tileSize = 16;
    constexpr int channel_chunk = 32;
    constexpr int C = 1;

    const int height = static_cast<int>(static_cast<float>(cam.height));
    const int width = static_cast<int>(static_cast<float>(cam.width));

    auto viewMat = cam.worldToCam;
    auto T = cam.T;
        
    torch::Tensor ksMat = torch::tensor({ {cam.fx, 0.0f, cam.cx},
                    {0.0f, cam.fy, cam.cy},
                    {0.0f, 0.0f, 1.0f} }, torch::kFloat32).to(device);

    c10::optional<torch::Tensor> cameraIds = {};
    c10::optional<torch::Tensor> gaussianIds = {};
    torch::Tensor conics;
    torch::Tensor depths; // GPU-only
    torch::Tensor compensations; // GPU-only
    torch::Tensor rgb, render_alpha;
    torch::Tensor xys, radii;
    torch::Tensor ray_ts, ray_planes, normals;
    torch::Tensor render_median_depth, render_depth, render_normals;
    std::unordered_map<std::string_view, torch::Tensor> render_pkg;
    const auto antialiased = rasterizeMode == GSRasterizeMode::Antialiased;
    const auto rade = trainConfig.normalConsistencyLoss && step > trainConfig.refineStopIter;
    // Set camera model based on actual camera type
    auto camera_model = static_cast<gsplat::CameraModelType>(cam.model_type);
    
    int degreesToUse = std::min<int>(step / trainConfig.shDegreeInterval, trainConfig.shDegree);

    auto background = backgroundColor;
    if (renderMode == GSplatRenderMode::RGBD || renderMode == GSplatRenderMode::RGBED) {
        if (background.defined())
            background = torch::cat({ backgroundColor.unsqueeze(0).repeat({C,1}), torch::zeros({C, 1},background.options()) }, -1).to(device);
    }
    else if (renderMode == GSplatRenderMode::Depth || renderMode == GSplatRenderMode::EDepth) {
        if (background.defined())
            background = torch::zeros({ C,1 }).to(torch::kFloat).to(device);
    }
    auto fused_settings = FusedRasterizationSettings{
        width,
        height,
        16,         // tile_size
        0.3f,       // eps2d
        0.01f,      // near_plane
        1e10f,      // far_plane
        0.0f,       // radius_clip
        antialiased,
        camera_model,
        static_cast<bool>(trainConfig.packLevel & GSPackLevel::PackTileID),
        rade,       // calc_gof
        false,      // sparse_grad
        false       // absgrad
    };
    {
#if USE_FUSED_GS > 0
        auto t = FusedRasterizeGaussians::apply(
            means,
            scales,
            quats,
            opacities,
            featuresDc,
            featuresRest,
            viewMat.unsqueeze(0),
            ksMat.unsqueeze(0),
            T.transpose(0, 1).to(device),
            background,
            degreesToUse,
            fused_settings
        );
        rgb = t[0];
        render_alpha = t[1];
        radii = t[2];
        depths = t[3];
        gaussianIds = t[4];
        if (rade) {
            render_depth = t[5];
            render_median_depth = t[6];
            render_normals = t[7];
        }
#else
        torch::Tensor t_opacity;
        {
            #if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)
            if( !(trainConfig.packLevel & GSPackLevel::PackTileID) ){
                auto p = ProjectGaussians::apply(means,
                                scales,
                                quats,
                                opacities,
                                viewMat.unsqueeze(0),
                                ksMat.unsqueeze(0),
                                height,
                                width,
                                eps2d,
                                nearPlane,
                                farPlane,
                                radiusClip,
                                antialiased,
                                camera_model,
                                rade);
                radii = p[0];
                xys = p[1];
                depths = p[2];
                conics = p[3];
                t_opacity = p[4];
                if (rade) {
                    const auto l = p.size();
                    ray_ts = p[l-3];
                    ray_planes = p[l-2];
                    normals = p[l-1];
                }
            }
            else {
                const bool sparseGrad = false;//packeLevel & GSPackLevel::PackSparseGrad;
                auto p = ProjectGaussiansPacked::apply(means,
                                scales,
                                quats,
                                opacities,
                                viewMat.unsqueeze(0),
                                ksMat.unsqueeze(0),
                                height,
                                width,
                                eps2d,
                                nearPlane,
                                farPlane,
                                radiusClip,
                                antialiased,
                                camera_model,
                                sparseGrad,
                                rade);
                cameraIds = p[0];
                gaussianIds = p[1];
                radii = p[2];
                xys = p[3];
                depths = p[4];
                conics = p[5];
                t_opacity = p[6];
                if (rade) {
                    const auto l = p.size();
                    ray_ts = p[l - 3];
                    ray_planes = p[l - 2];
                    normals = p[l - 1];
                }
            }
            #else
                throw std::runtime_error("GPU support not built, use --cpu");
            #endif
        }

        torch::Tensor rgbs;
        {
            #if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)
            if( trainConfig.packLevel & GSPackLevel::PackTileID ){
                torch::Tensor viewDirs = means.detach().index_select(0, *gaussianIds) - T.transpose(0, 1).to(device);
                torch::Tensor masks = (radii > 0).all(-1); //[nnz]
                torch::Tensor sh0_coeffs,shN_coeffs;
                if( featuresRest.dim() == 3){
                    sh0_coeffs = featuresDc.index_select(0, gaussianIds.value()); // [nnz, 1, 3]
                    shN_coeffs = featuresRest.index_select(0, gaussianIds.value()); // [nnz, K-1, 3]
                }
                else{
                    sh0_coeffs = featuresDc.index_select(0, gaussianIds.value()); // [nnz, 1, 3]
                    shN_coeffs = featuresRest.index_select(0, gaussianIds.value()); // [nnz, K-1, 3]
                }
                rgbs = SphericalHarmonics::apply(degreesToUse, viewDirs.contiguous(), sh0_coeffs.contiguous(), shN_coeffs.contiguous(), masks.contiguous());
            }
            else{
                torch::Tensor viewDirs = means.detach() - T.transpose(0, 1).to(device);
                torch::Tensor masks = (radii > 0).all(-1).squeeze(0);
                rgbs = SphericalHarmonics::apply(degreesToUse, viewDirs.contiguous(), featuresDc.contiguous(), featuresRest.contiguous(), masks.contiguous());
                rgbs = rgbs.unsqueeze(0);
            }
            #endif
        }

        rgbs = torch::clamp_min(rgbs + 0.5f, 0.0f);
        if (renderMode == GSplatRenderMode::RGBD || renderMode == GSplatRenderMode::RGBED) {
            rgbs = torch::cat({ rgbs, depths.unsqueeze(-1) }, -1);
        }
        else if (renderMode == GSplatRenderMode::Depth || renderMode == GSplatRenderMode::EDepth) {
            rgbs = depths.unsqueeze(-1);
        }
        {  
            #if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)
            {
                auto t = rasterize_to_pixels(
                    xys,
                    conics,
                    rgbs,
                    t_opacity,
                    background,
                    radii,
                    depths,
                    width,
                    height,
                    tileSize,
                    C,
                    trainConfig.packLevel & GSPackLevel::PackTileID,
                    trainConfig.useAbsGrad,
                    cameraIds,
                    gaussianIds,
                    ray_ts,
                    ray_planes,
                    normals,
                    ksMat.unsqueeze(0),
                    rade
                );
                rgb = std::get<0>(t);
                render_alpha = std::get<1>(t);
                render_depth = std::get<2>(t);
                render_median_depth = std::get<3>(t);
                render_normals = std::get<4>(t); 
            }
            #endif
        }
#endif
    }
    rgb = rgb.squeeze(0);
    if ( (renderMode == GSplatRenderMode::RGBD || renderMode == GSplatRenderMode::RGBED)) {
        auto split_tensors = torch::split(rgb, { 3, 1 }, -1);
        rgb = split_tensors[0];
        if(!render_depth.defined())
            render_depth = split_tensors[1];
    }
    rgb = torch::clamp_max(rgb, 1.0f);
    render_pkg["colors"] = rgb;
    render_pkg["alphas"] = render_alpha.squeeze(0);
    if(render_depth.defined())
        render_pkg["depths"] = render_depth;
    if(render_median_depth.defined())
        render_pkg["median_depths"] = render_median_depth;
    if(render_normals.defined()){
        auto inv_viewmats = torch::linalg_inv(viewMat.unsqueeze(0)); // [C, 4, 4]
        auto rotation_matrix = inv_viewmats.index({torch::indexing::Slice(), torch::indexing::Slice(0, 3), torch::indexing::Slice(0, 3)}); // [..., 3, 3]
        // Perform the equivalent of torch.einsum("...ij,...hwj->...hwi", rotation_matrix, render_normals)
        render_normals = torch::einsum("...ij,...hwj->...hwi", {rotation_matrix, render_normals}); // [..., H, W, 3]

        render_pkg["normals"] = render_normals;
    }
    return render_pkg;
}
void GaussianTrainModel::optimizersZeroGrad(){
  meansOpt->zero_grad();
  scalesOpt->zero_grad();
  quatsOpt->zero_grad();
  featuresDcOpt->zero_grad();
  featuresRestOpt->zero_grad();
  opacitiesOpt->zero_grad();
}

void GaussianTrainModel::optimizersStep(torch::Tensor visiblity){
  if (trainConfig.packLevel & GSPackLevel::PackSparseGrad) {
      // Use SparseAdam for sparse gradients (like gsplat simple_trainer.py)
      static_cast<SparseAdam*>(meansOpt.get())->step();
      static_cast<SparseAdam*>(scalesOpt.get())->step();
      static_cast<SparseAdam*>(quatsOpt.get())->step();
      static_cast<SparseAdam*>(featuresDcOpt.get())->step();
      static_cast<SparseAdam*>(featuresRestOpt.get())->step();
      static_cast<SparseAdam*>(opacitiesOpt.get())->step();
  }
  else {
      static_cast<FusedAdam*>(meansOpt.get())->step(visiblity);
      static_cast<FusedAdam*>(scalesOpt.get())->step(visiblity);
      static_cast<FusedAdam*>(quatsOpt.get())->step(visiblity);
      static_cast<FusedAdam*>(featuresDcOpt.get())->step(visiblity);
      static_cast<FusedAdam*>(featuresRestOpt.get())->step(visiblity,!trainConfig.bestQuality,trainConfig.numIters);
      static_cast<FusedAdam*>(opacitiesOpt.get())->step(visiblity);
  }
}

void GaussianTrainModel::schedulersStep(int step){
  meansOptScheduler->step();
  scalesOptScheduler->step();
    // meansOptScheduler->step(step);
}

int GaussianTrainModel::getDownscaleFactor(int step){
    return std::pow(2, (std::max<int>)(trainConfig.numDownscales - step / trainConfig.resolutionSchedule, 0));
}

void GaussianTrainModel::trainSetup(float sceneExtent){
    constexpr auto batch_size = 1;
    const auto sqrt_batch = std::sqrt(batch_size);
    const auto inv_sqrt = 1.0 / std::sqrt(batch_size);
    const auto eps_f = 1e-15 * inv_sqrt;
    const auto beta_f = std::make_tuple(1 - batch_size * (1 - 0.9), 1 - batch_size * (1 - 0.999));
    sceneScale = sceneExtent;
    const auto poslrInit = 0.00016 * sceneExtent;
    const auto scalinglr = 0.007;
    const auto rotationlr = 0.001;
    const auto featurelr = 0.0025;
    const auto featureRestlr = 0.0025 / 20.0f;
    const auto opacitylr = 0.05;
    if(trainConfig.packLevel & GSPackLevel::PackTileID && trainConfig.packLevel & GSPackLevel::PackSparseGrad){
        // Use SparseAdam for sparse gradients (like gsplat simple_trainer.py)
        // SparseAdam handles both sparse and dense gradients with CUDA acceleration
        meansOpt.reset(new SparseAdam({ means }, torch::optim::AdamOptions(poslrInit * sqrt_batch).eps(eps_f).betas(beta_f)));
        scalesOpt.reset(new SparseAdam({ scales }, torch::optim::AdamOptions(scalinglr * sqrt_batch).eps(eps_f).betas(beta_f)));
        quatsOpt.reset(new SparseAdam({ quats }, torch::optim::AdamOptions(rotationlr * sqrt_batch).eps(eps_f).betas(beta_f)));
        featuresDcOpt.reset(new SparseAdam({ featuresDc }, torch::optim::AdamOptions(featurelr * sqrt_batch).eps(eps_f).betas(beta_f)));
        featuresRestOpt.reset(new SparseAdam({ featuresRest }, torch::optim::AdamOptions(featureRestlr * sqrt_batch).eps(eps_f).betas(beta_f)));
        opacitiesOpt.reset(new SparseAdam({ opacities }, torch::optim::AdamOptions(opacitylr * sqrt_batch).eps(eps_f).betas(beta_f)));
    }
    else{
        meansOpt.reset(new FusedAdam({ means }, torch::optim::AdamOptions(poslrInit * sqrt_batch).eps(eps_f).betas(beta_f)));
        scalesOpt.reset(new FusedAdam({ scales }, torch::optim::AdamOptions(scalinglr * sqrt_batch).eps(eps_f).betas(beta_f)));
        quatsOpt.reset(new FusedAdam({ quats }, torch::optim::AdamOptions(rotationlr * sqrt_batch).eps(eps_f).betas(beta_f)));
        featuresDcOpt.reset(new FusedAdam({ featuresDc }, torch::optim::AdamOptions(featurelr * sqrt_batch).eps(eps_f).betas(beta_f)));
        featuresRestOpt.reset(new FusedAdam({ featuresRest }, torch::optim::AdamOptions(featureRestlr * sqrt_batch).eps(eps_f).betas(beta_f)));
        opacitiesOpt.reset(new FusedAdam({ opacities }, torch::optim::AdamOptions(opacitylr * sqrt_batch).eps(eps_f).betas(beta_f)));
    }
    // meansOptScheduler = std::make_unique<OptimScheduler>(meansOpt.get(), trainConfig.poslrFinal * sceneExtent, trainConfig.numIters);
    const double gamma = std::pow(0.01, 1.0 / trainConfig.numIters);
    meansOptScheduler = std::make_unique<ExponentialLR>(*meansOpt, gamma, 0);
    const double gamma_scales = std::pow(0.7, 1.0 / trainConfig.numIters);
    scalesOptScheduler = std::make_unique<ExponentialLR>(*scalesOpt, gamma_scales, 0);
    
    if( !pruneStrategy ){
        createPruneStrategy(trainConfig.pruneStrategy);
    }
    densifyStrategy->resetState();
}

void addToOptimizer(torch::optim::Adam *optimizer, const torch::Tensor &newParam, const torch::Tensor &idcs, int nSamples){
    torch::Tensor param = optimizer->param_groups()[0].params()[0];
#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
    auto pId = param.unsafeGetTensorImpl();
#else
    auto pId = c10::guts::to_string(param.unsafeGetTensorImpl());
#endif
    auto paramState = std::make_unique<AdamCustomParamState>(static_cast<AdamCustomParamState&>(*optimizer->state()[pId]));
    
    std::vector<int64_t> repeats;
    repeats.push_back(nSamples);
    for (long int i = 0; i < paramState->exp_avg().dim() - 1; i++){
        repeats.push_back(1);
    }

    paramState->exp_avg(torch::cat({
        paramState->exp_avg(), 
        torch::zeros_like(paramState->exp_avg().index({idcs.squeeze()})).repeat(repeats)
    }, 0));
    
    paramState->exp_avg_sq(torch::cat({
        paramState->exp_avg_sq(), 
        torch::zeros_like(paramState->exp_avg_sq().index({idcs.squeeze()})).repeat(repeats)
    }, 0));
    if(paramState->step_per_gaussian().defined()){
        paramState->step_per_gaussian(torch::cat({
            paramState->step_per_gaussian(), 
            torch::zeros_like(paramState->step_per_gaussian().index({idcs.squeeze()})).repeat(repeats)
        }, 0));
    }
    optimizer->state().erase(pId);

#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
    auto newPId = newParam.unsafeGetTensorImpl();
#else
    auto newPId = c10::guts::to_string(newParam.unsafeGetTensorImpl());
#endif    
    optimizer->state()[newPId] = std::move(paramState);
    optimizer->param_groups()[0].params()[0] = newParam;
}

void removeFromOptimizer(torch::optim::Adam *optimizer, const torch::Tensor &newParam, const torch::Tensor &deletedMask){
    torch::Tensor param = optimizer->param_groups()[0].params()[0];
#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
    auto pId = param.unsafeGetTensorImpl();
#else
    auto pId = c10::guts::to_string(param.unsafeGetTensorImpl());
#endif
    auto paramState = std::make_unique<AdamCustomParamState>(static_cast<AdamCustomParamState&>(*optimizer->state()[pId]));

    paramState->exp_avg(paramState->exp_avg().index({~deletedMask}));
    paramState->exp_avg_sq(paramState->exp_avg_sq().index({~deletedMask}));
    if(paramState->step_per_gaussian().defined()){
        paramState->step_per_gaussian(paramState->step_per_gaussian().index({~deletedMask}));
    }
    optimizer->state().erase(pId);
#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
    auto newPId = newParam.unsafeGetTensorImpl();
#else
    auto newPId = c10::guts::to_string(newParam.unsafeGetTensorImpl());
#endif
    optimizer->param_groups()[0].params()[0] = newParam;
    optimizer->state()[newPId] = std::move(paramState);
}

void updateParamWithOptimizer(torch::optim::Adam* optimizer, const torch::Tensor& newParam, std::function<torch::Tensor(torch::Tensor&)>&& optimizerFunc)
{
    torch::Tensor param = optimizer->param_groups()[0].params()[0];
#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
    auto pId = param.unsafeGetTensorImpl();
#else
    auto pId = c10::guts::to_string(param.unsafeGetTensorImpl());
#endif
    auto paramState = std::make_unique<AdamCustomParamState>(static_cast<AdamCustomParamState&>(*optimizer->state()[pId]));
    paramState->exp_avg(optimizerFunc(paramState->exp_avg()));
    paramState->exp_avg_sq(optimizerFunc(paramState->exp_avg_sq()));
    if (paramState->step_per_gaussian().defined()) {
        paramState->step_per_gaussian(optimizerFunc(paramState->step_per_gaussian()));
    }
    optimizer->state().erase(pId);

#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
    auto newPId = newParam.unsafeGetTensorImpl();
#else
    auto newPId = c10::guts::to_string(newParam.unsafeGetTensorImpl());
#endif    
    optimizer->state()[newPId] = std::move(paramState);
    optimizer->param_groups()[0].params()[0] = newParam;
}


void GaussianTrainModel::save(const std::string &filename){

    auto antialiased = trainConfig.mipAntiliased && trainConfig.modelType != Splat2D;
    return save_splat_models(filename, means, opacities,scales, quats,featuresDc,featuresRest,antialiased);
}

void GaussianTrainModel::load(const std::string& filepath) {
    load_gsplat_model(filepath,means, opacities, scales, quats, featuresDc, featuresRest, trainConfig.mipAntiliased);
}

void GaussianTrainModel::saveOptimizerStates() {
    trainConfig.poslr = meansOpt->param_groups()[0].options().get_lr();
    trainConfig.scalinglr = scalesOpt->param_groups()[0].options().get_lr();
    trainConfig.rotationlr = quatsOpt->param_groups()[0].options().get_lr();
    trainConfig.featurelr = featuresDcOpt->param_groups()[0].options().get_lr();
    trainConfig.featureRestlr = featuresRestOpt->param_groups()[0].options().get_lr();
    trainConfig.opacitylr = opacitiesOpt->param_groups()[0].options().get_lr();
}

void GaussianTrainModel::loadOptimizerStates(int current_step) {
    auto setOptlr = [](torch::optim::Adam* optimizer, float lr){
        optimizer->param_groups()[0].options().set_lr(lr);
    };
    setOptlr(meansOpt.get(), trainConfig.poslr);
    setOptlr(scalesOpt.get(), trainConfig.scalinglr);
    setOptlr(quatsOpt.get(), trainConfig.rotationlr);
    setOptlr(featuresDcOpt.get(), trainConfig.featurelr);
    setOptlr(featuresRestOpt.get(), trainConfig.featureRestlr);
    setOptlr(opacitiesOpt.get(), trainConfig.opacitylr);
}

torch::Tensor GaussianTrainModel::mainLoss(torch::Tensor &rgb, torch::Tensor &gt, float l1Weight){
#ifdef USE_MPS
    torch::Tensor ssimLoss = 1.0f - ssim.eval(rgb, gt);
#else
    torch::Tensor ssimLoss = 1.0f - fused_ssim(rgb.unsqueeze(0).permute({0, 3, 1, 2}), gt.unsqueeze(0).permute({0, 3, 1, 2}),"valid");
#endif
    torch::Tensor l1Loss = l1(rgb, gt);
    return l1Weight * l1Loss + (1- l1Weight) * ssimLoss;
}

torch::Tensor GaussianTrainModel::getSHs()
{
    int numPoints = means.size(0);
    //std::cout << "featuresDc.size: " << featuresDc.index({ Slice(), None, Slice() }).sizes() << std::endl;
    //std::cout << "featuresRest.size: " << featuresRest.sizes() << std::endl;
    return torch::cat({ featuresDc, featuresRest }, 1);
}

void GaussianTrainModel::updateGaussianAttributes(
    const std::vector<glm::vec3>& pos,
    const std::vector<glm::vec4>& rot,
    const std::vector<glm::vec3>& scs,
    const std::vector<float>& opacity,
    const std::vector<std::array<float, 3>>& shs_0,
    const std::vector<std::array<float, 45>>& shs_n)
{
    auto count = pos.size();
    const auto pointType = torch::TensorOptions().dtype(torch::kFloat32);
    means = torch::from_blob((float*)pos.data(), { static_cast<long>(pos.size()), 3 }, pointType).to(device).set_requires_grad(true);
    scales = torch::from_blob((float*)scs.data(), { static_cast<long>(scs.size()), 3 }, pointType).to(device, true).set_requires_grad(true);
    quats = torch::from_blob((float*)rot.data(), { static_cast<long>(rot.size()), 4 }, pointType).to(device, true).set_requires_grad(true);
    opacities = torch::from_blob((float*)opacity.data(), { static_cast<long>(opacity.size()), 1}, pointType).to(device, true).set_requires_grad(true);
    // features
    //auto features = torch::from_blob((float*)shs.data(), { static_cast<long>(count), numShBases(trainConfig.shDegree), 3}, pointType).to(device);
    featuresDc = torch::from_blob((float*)shs_0.data(), { static_cast<long>(count), 1, 3 }, pointType).contiguous().to(device).set_requires_grad(true);
    featuresRest = torch::from_blob((float*)shs_n.data(), { static_cast<long>(count), 15, 3 }, pointType).contiguous().to(device).set_requires_grad(true);

    trainSetup(sceneScale);
}

void GaussianTrainModel::prunePoints(torch::Tensor& culls)
{
    int numPointsBefore = means.size(0);
    means = means.index({ ~culls }).detach().requires_grad_();
    scales = scales.index({ ~culls }).detach().requires_grad_();
    quats = quats.index({ ~culls }).detach().requires_grad_();
    featuresDc = featuresDc.index({ ~culls }).detach().requires_grad_();
    featuresRest = featuresRest.index({ ~culls }).detach().requires_grad_();
    opacities = opacities.index({ ~culls }).detach().requires_grad_();

    removeFromOptimizer(meansOpt.get(), means, culls);
    removeFromOptimizer(scalesOpt.get(), scales, culls);
    removeFromOptimizer(quatsOpt.get(), quats, culls);
    removeFromOptimizer(featuresDcOpt.get(), featuresDc, culls);
    removeFromOptimizer(featuresRestOpt.get(), featuresRest, culls);
    removeFromOptimizer(opacitiesOpt.get(), opacities, culls);
    if (trainConfig.verbose)
        std::cout << "Culled " << (numPointsBefore - means.size(0)) << " gaussians, remaining " << means.size(0) << std::endl;
}

void GaussianTrainModel::stepBeforebackward(int step)
{
    if (densifyStrategy)
    {
        densifyStrategy->stepBeforebackward(step);
    }
}
void GaussianTrainModel::stepAfterbackward(int step, std::unordered_map<std::string_view, torch::Tensor>& infos)
{
    if (densifyStrategy)
    {
        densifyStrategy->stepAfterbackward(step, infos);
    }
}

void GaussianTrainModel::prune(std::vector<Camera>& cams,int step)
{
    if( pruneStrategy)
    {
        auto num_splats = means.size(0);
        if(num_splats > 100000 && cams.size() > 0 && !isUseGutRasterize(cams[0]))
        {
            pruneStrategy->prune(cams, step);
            densifyStrategy->resetState();
        }
    }
}

void GaussianTrainModel::updateBounds(float percentile)
{
    // auto [min_points,max_points] = pointsBounds(means);
    // boundsMin = std::move(min_points);
    // boundsMax = std::move(max_points);
    auto lower_percentile = (1 - percentile) / 2.0f;
    auto upper_percentile = (1 + percentile) / 2.0f;
    auto q = torch::tensor({lower_percentile, upper_percentile},torch::kFloat32).to(means.device());
    auto q_points = torch::quantile(means, q, 0).cpu();
    auto tboundsMin = q_points.index({0}).data_ptr<float>();
    auto tboundsMax = q_points.index({1}).data_ptr<float>();
    boundsMin = glm::vec3(tboundsMin[0], tboundsMin[1], tboundsMin[2]);
    boundsMax = glm::vec3(tboundsMax[0], tboundsMax[1], tboundsMax[2]);
}
