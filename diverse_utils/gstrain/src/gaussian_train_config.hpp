#pragma once
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

enum GSPackLevel
{
    NoPack = 0,
    PackF32ToU8 = 1, //pack color and normal
    PackTileID = 2, //pack tile id
    PackSparseGrad = 4, //pack sparse
};

enum SplatModelType
{
    Splat3D = 0,
    Splat2D = 1,
};

enum SplatDensifyType {
    SplatADC = 0,
    SplatMCMC = 1,
    SplatADCPlus = 2,
};
enum SfmCameraModel
{
    SimplePinhole = 0,
    Radial = 1,
    Fisheye = 2,
};

struct GaussianTrainConfig
{
    std::string sourcePath;
    std::string modelPath;
    std::string cameraPosePath;
    std::string pointCloudPath;

    int         datasetType = -1;
    int         maxImageWidth = 4096;
    int         maxImageHeight = 4096;
    int         maxImageCount = 2048;
    int         modelType = 0;
    float       poslr = 0.00016;
    float       rotationlr = 0.001;
    float       scalinglr = 0.007;
    float       featurelr = 0.0025;
    float       featureRestlr = 0.0025 / 20.0f;
    float       opacitylr = 0.05;

    float   downScaleFactor = 1.0f;
    //Number of iterations to run
    int     numIters = 30000;
    //Number of images downscales to use. After being scaled by [downscale-factor], images are initially scaled by a further (2^[num-downscales]) and the scale is increased every [resolution-schedule]
    int     numDownscales = 2;
    int     resolutionSchedule = 3000;
    //Maximum spherical harmonics degree (must be > 0)
    int     shDegree = 3;
    //Increase the number of spherical harmonics degree after these many steps (will not exceed [sh-degree])
    int     shDegreeInterval = 1000;
    //Weight to apply to the structural similarity loss. Set to zero to use least absolute deviation (L1) loss only
    float   ssimWeight = 0.2;
    // Refine GSs every this steps. Default is 100.
    int     refineEvery = 100;
    // Start refining GSs after this iteration. Default is 500.
    int     warmupLength = 500;
    //  Reset opacities every this steps. Default is 3000.
    int     resetAlphaEvery = 3000;
    // Stop refining GSs after this iteration. Default is 15_000.
    int     refineStopIter = 15000;
    // Stop refining GSs based on 2d scale after this
    int     refineScale2dStopIter = 4000;
    //GSs with image plane gradient above this value will be split/duplicated. Default is 0.0002.
    float   growGrad2d = 0.0002;
    //GSs with 3d scale (normalized by scene_scale) below this value will be duplicated. Above will be split. Default is 0.01.
    float   growScale3d = 0.01;
    //GSs with 2d scale (normalized by image resolution) above this value will be split. Default is 0.05.
    float   growScale2d = 0.05;
    // GSs with 3d scale (normalized by scene_scale) above this  value will be pruned. Default is 0.1.
    float   pruneScale3d = 0.2;
    //  GSs with 2d scale (normalized by image resolution) above this value will be pruned. Default is 0.15.
    float   pruneScale2d = 0.15;
    //GSs with opacity below this value will be pruned. Default is 0.005.
    float   pruneOpacity = 0.005;
    //Retain the project input's coordinate reference system
    bool    keepCrs = false;
    //Whether to use revised opacity heuristic from https://arxiv.org/abs/2404.06109 (experimental). Default is False.
    bool    revisedOpacity = true;
    //whether use absgrad to densify; reference: https://github.com/TY424/AbsGS
    bool    useAbsGrad = true;
    //whether use coarse to fine train, at start we use small resolution image train, we modify image resolution ervey resolutionSchedule step. 
    bool    progressiveTrain = true;
    //grow fraction, the fraction of gs to grow at each densify step.
    float   growFraction = 0.25f;
    //whether scale pixel grad  from https://arxiv.org/pdf/2403.15530
    bool    pixelGradScale = false;
    int     videoFps = 4;
    bool    batchLoader = false;
    uint32_t batchSize = 128;
    //pack levle, 0 means no pack, have 3 flag or op
    uint32_t packLevel = 0;
    bool    useMask = false;
    float    scaleReg = 0.01;
    //use rank regularize form https://arxiv.org/abs/2406.11672
    bool    rankRegularization = true;
    float   lambdaRank = 1e-5;
    float   lambdaErank = 0.01;
    float   opacityReg = 0.01f;
    bool    verbose = false;
    //mcmc config 
    int     capMax = 3000000;
    float   minOpacity = 0.005;
    float   noiselr = 5e5;
    //prune config
    bool    cullSH = false;
    int     pruneInterval = 12000;
    float   v_pow = 0.1;
    float   pruneDecay = 0.8;
    float   depthThreshold = 0.35f;
    //sfm related
    bool    outputSparsePoints = false;
    bool    singleCamera = false;
    int     cameraModel = 0;
    int     mapperType = 0;
    int     quality = 1;//low, medium,high,extreme
    bool    randomInitPoints = false;
    //Use random background for training to discourage transparency
    bool    randombkgd = false;
    //prune method
    int     pruneStrategy = 1; //light gaussian prune 
    //whether to use mip splat from https://arxiv.org/pdf/2311.16493
    bool    mipAntiliased = true;
    //whether to use RaDe to get normal and depth
    bool    normalConsistencyLoss = true;
    float   normalConsistencyLambda = 0.05f;
    //experimental 
    bool    depthLoss = false;
    float   depthLambda = 1e-2f;
    //distortionLoss
    bool    distortionLoss = false;
    float   distortionLambda = 0.01f;
    //whether eval on training 
    bool    evalOnTraining = false;
    //  Use visible adam from Taming 3DGS. (experimental)
    bool    visibleAdam = false;
    int     densifyStrategy = 1;
    bool    enableBg = false;
    bool    useBilateralGrid = false;
    int     bilateralGridX = 16;
    int     bilateralGridY = 16;
    int     bilateralGridW = 8;
    int     numSkyPoints = 100000;
    bool    exportMesh = false;
    int     meshResolution = 256;
    bool    exportPbrMaterial = false;
    bool    enableFocusRegion = false;
    // using homogeneous coordinates, a concept on the projective geometry, 
    // for the 3DGS pipeline remarkably improves the rendering accuracies of distant object
    // refernce to https://arxiv.org/pdf/2503.19232
    bool    useHomCoord = false;
    // use best quality config
    bool    bestQuality = true;
    int     videoStrategy = 0;
    template <typename Archive>
    void save(Archive& archive) const
    {
        archive(cereal::make_nvp("sourcePath", sourcePath), cereal::make_nvp("modelPath", modelPath));
        archive(cereal::make_nvp("progressiveTrain", progressiveTrain), cereal::make_nvp("packLevel", packLevel));
        archive(cereal::make_nvp("poslr", poslr));
        archive(cereal::make_nvp("rotationlr", rotationlr));
        archive(cereal::make_nvp("scalinglr", scalinglr));
        archive(cereal::make_nvp("featurelr", featurelr));
        archive(cereal::make_nvp("featureRestlr", featureRestlr));
        archive(cereal::make_nvp("opacitylr", opacitylr));
        archive(cereal::make_nvp("numIters", numIters));
        archive(cereal::make_nvp("refineEvery", refineEvery));
        archive(cereal::make_nvp("warmupLength", warmupLength));
        archive(cereal::make_nvp("resetAlphaEvery", resetAlphaEvery));
        archive(cereal::make_nvp("refineStopIter", refineStopIter));
        archive(cereal::make_nvp("videoFps", videoFps));
        archive(cereal::make_nvp("useMask", useMask));
        archive(cereal::make_nvp("densifyStrategy", densifyStrategy));
        archive(cereal::make_nvp("enableBg", enableBg));
        archive(cereal::make_nvp("modelType", modelType));
        archive(cereal::make_nvp("enableFocusRegion", enableFocusRegion));
        archive(cereal::make_nvp("pruneStrategy", pruneStrategy));
        archive(cereal::make_nvp("mipAntiliased", mipAntiliased));
        archive(cereal::make_nvp("useBilateralGrid", useBilateralGrid));
        archive(cereal::make_nvp("exportMesh", exportMesh));
        archive(cereal::make_nvp("meshResolution", meshResolution));
        archive(cereal::make_nvp("useHomCoord", useHomCoord));
        archive(cereal::make_nvp("bestQuality", bestQuality));
        archive(cereal::make_nvp("videoStrategy", videoStrategy));
    }
    template <typename Archive>
    void load(Archive& archive)
    {
        archive(cereal::make_nvp("sourcePath", sourcePath), cereal::make_nvp("modelPath", modelPath));
        archive(cereal::make_nvp("progressiveTrain", progressiveTrain), cereal::make_nvp("packLevel", packLevel));
        archive(cereal::make_nvp("poslr", poslr));
        archive(cereal::make_nvp("rotationlr", rotationlr));
        archive(cereal::make_nvp("scalinglr", scalinglr));
        archive(cereal::make_nvp("featurelr", featurelr));
        archive(cereal::make_nvp("featureRestlr", featureRestlr));
        archive(cereal::make_nvp("opacitylr", opacitylr));
        archive(cereal::make_nvp("numIters", numIters));
        archive(cereal::make_nvp("refineEvery", refineEvery));
        archive(cereal::make_nvp("warmupLength", warmupLength));
        archive(cereal::make_nvp("resetAlphaEvery", resetAlphaEvery));
        archive(cereal::make_nvp("refineStopIter", refineStopIter));
        archive(cereal::make_nvp("videoFps", videoFps));
        archive(cereal::make_nvp("useMask", useMask));
        archive(cereal::make_nvp("densifyStrategy", densifyStrategy));
        archive(cereal::make_nvp("enableBg", enableBg));
        archive(cereal::make_nvp("modelType", modelType));
        archive(cereal::make_nvp("enableFocusRegion", enableFocusRegion));
        archive(cereal::make_nvp("pruneStrategy", pruneStrategy));
        archive(cereal::make_nvp("mipAntiliased", mipAntiliased));
        archive(cereal::make_nvp("useBilateralGrid", useBilateralGrid));
        archive(cereal::make_nvp("exportMesh", exportMesh));
        archive(cereal::make_nvp("meshResolution", meshResolution));
        archive(cereal::make_nvp("useHomCoord", useHomCoord));
        archive(cereal::make_nvp("bestQuality", bestQuality));
        archive(cereal::make_nvp("videoStrategy", videoStrategy));
    }
};

struct MeshOptimConfig {
public:
    std::string sourcePath;
    std::string modelPath;
    std::string cameraPosePath;

    bool    useMask = false;
    int     numIters = 30000;
    float   lambda_mask = 0.1f;
    float   lambda_kd = 0.005f;
    float   lambda_ks = 0.0025f;
    
    float   lambda_rgb = 1.0f;
    float   lambda_rgb_brdf = 0.02f;
    float   lambda_brdf_diffuse = 0.0015f;
    float   lambda_brdf_specular = 0.000025f;

    float   lambda_nrm = 0.00025f;
    float   lambda_extra_kd = 0.0f;
    float   lambda_lpips = 0.0f;
    float   lambda_normal = 0.0f;
    float   lambda_edgelen = 0.0f;
    float   lambda_largestep = 10.0f;
    float   pos_gradient_boost = 1.0f;
    float   lambda_img_normal = 0.005f;
    float   lambda_offsets = 0.1f;

    bool    use_hdr = false;
    bool    use_restir = true;
    int     spp = 32;
    int     env_h = 256;
    int     env_w = 256;
    float   exposure = 0.0f;
    float   albedo_scale_x = 1.0f;
    float   albedo_scale_y = 1.0f;
    float   albedo_scale_z = 1.0f;

    std::string color_space = "srgb";
    bool    refine = true;
    float   refine_decimate_ratio = 0.1f;
    float   refine_size = 0.01f;
    float   refine_remesh_size = 0.02f;
};