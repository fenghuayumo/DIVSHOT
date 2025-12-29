#ifndef MODEL_H
#define MODEL_H

#include <iostream>
#include <array>
#include <torch/torch.h>
#include <torch/version.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <kdtree_tensor.hpp>
#include <ssim.hpp>
#include <input_data.hpp>
#include "spherical_harmonics.hpp"
#include "optim_scheduler.hpp"
#include <gaussian_train_config.hpp>

using namespace torch::indexing;
using namespace torch::autograd;

torch::Tensor randomQuatTensor(long long n);
torch::Tensor projectionMatrix(float zNear, float zFar, float fovX, float fovY, const torch::Device &device);
torch::Tensor psnr(const torch::Tensor& rendered, const torch::Tensor& gt);
torch::Tensor l1(const torch::Tensor& rendered, const torch::Tensor& gt);
torch::Tensor l1(const torch::Tensor& rendered, const torch::Tensor& gt, const torch::Tensor& mask);

enum class GSplatRenderMode
{
    RGB = 1,
    Depth = 2,
    EDepth = 3, //epected depth
    RGBD = 4,
    RGBED = 5,
    DN = 6,
    RGBDN = 7,
    Count
};

enum class GSRasterizeMode
{
    Classic = 0,
    Antialiased = 1,
};

class GaussianTrainModel;
class PruneStrategy
{
public:
    PruneStrategy(GaussianTrainModel* model) : gaussian(model) {}
    virtual auto prune(std::vector<Camera>& cam, int step) -> void;
public:
    auto cullShBands(std::vector<Camera>& cam, int step,float threshold, float stdThreshold = 0.0f)->void;
protected:
    GaussianTrainModel* gaussian;
    float   cdistThreshold = 6.0f;
    float   stdThreshold = 0.04f;
};

class DensifyStrategy 
{
public:
    DensifyStrategy(GaussianTrainModel* model) : gaussian(model) {}
public:
    virtual void stepBeforebackward(int step) {}
    virtual void stepAfterbackward(int step, std::unordered_map<std::string_view, torch::Tensor>& infos) {}
    virtual void resetState() {}
public:
    GaussianTrainModel* gaussian;
    int     type;
};

void addToOptimizer(torch::optim::Adam* optimizer, const torch::Tensor& newParam, const torch::Tensor& idcs, int nSamples);
void removeFromOptimizer(torch::optim::Adam* optimizer, const torch::Tensor& newParam, const torch::Tensor& deletedMask);
void updateParamWithOptimizer(torch::optim::Adam* optimizer, const torch::Tensor& newParam, std::function<torch::Tensor(torch::Tensor&)>&& optimizerFunc);

class GaussianTrainModel{

public:
    GaussianTrainModel(
        const InputData& inputData, 
        int numCameras,
        GaussianTrainConfig& config,
        const torch::Device& device);

    GaussianTrainModel(
        const std::filesystem::path& filename, 
        const InputData& inputData,
        int numCameras,
        GaussianTrainConfig& config,
        const torch::Device& device);

    ~GaussianTrainModel()
    {
    }
    auto createPruneStrategy(int type)->void;
    virtual auto forward(Camera& cam, 
                int step, 
                GSplatRenderMode renderMode = GSplatRenderMode::RGB, 
                GSRasterizeMode mode = GSRasterizeMode::Classic)-> 
                std::tuple<std::unordered_map<std::string_view, torch::Tensor>,std::unordered_map<std::string_view, torch::Tensor>>;
        
    virtual auto renderSplats(Camera &cam, 
                    int step,
                    bool splatCount = false, 
                    GSplatRenderMode renderMode = GSplatRenderMode::RGB,
                    GSRasterizeMode mode = GSRasterizeMode::Classic)->
                    std::tuple<torch::Tensor,torch::Tensor, torch::Tensor>;
    virtual auto evalSplats(
                    const Camera &cam, 
                    int step,
                    GSplatRenderMode renderMode = GSplatRenderMode::RGB,
                    GSRasterizeMode mode = GSRasterizeMode::Classic)->
                    std::unordered_map<std::string_view, torch::Tensor>;
    auto configSkyModel(const InputData& inputData)->std::pair<torch::Tensor, torch::Tensor>;
    void setDensifyStrategy(int type);
    void stepBeforebackward(int step);
    void stepAfterbackward(int step,std::unordered_map<std::string_view, torch::Tensor>& infos);
    virtual void prune(std::vector<Camera>& cams,int step);
    void optimizersZeroGrad();
    void optimizersStep(torch::Tensor visiblity);
    void schedulersStep(int step);
    auto getDownscaleFactor(int step)->int;
    void save(const std::string &filename);
    void load(const std::string &filename);
    bool isUseGutRasterize(const Camera& cam);
    
    void saveOptimizerStates();
    void loadOptimizerStates(int current_step = 0);
    torch::Tensor mainLoss(torch::Tensor &rgb, torch::Tensor &gt, float l1Weight);
    torch::Tensor bilateralGridTvLoss();
    torch::Tensor applyBilateralGrid(torch::Tensor x,int idx);
    void trainSetup(float sceneExtent = 1.0f);
    void prunePoints(torch::Tensor& mask);
    void updateGaussianAttributes(
        const std::vector<glm::vec3>& pos,
        const std::vector<glm::vec4>& rot,
        const std::vector<glm::vec3>& scales,
        const std::vector<float>& opacity,
        const std::vector<std::array<float, 3>>& shs_0,
        const std::vector<std::array<float, 45>>& shs_n);
    
    void updateBounds(float percentile = 0.8f);

    torch::Tensor getSHs();
    torch::Tensor means;
    torch::Tensor scales;
    torch::Tensor quats;
    torch::Tensor featuresDc;
    torch::Tensor featuresRest;
    torch::Tensor opacities;

    std::unique_ptr<torch::optim::Adam> meansOpt;
    std::unique_ptr<torch::optim::Adam> scalesOpt;
    std::unique_ptr<torch::optim::Adam> quatsOpt;
    std::unique_ptr<torch::optim::Adam> featuresDcOpt;
    std::unique_ptr<torch::optim::Adam> featuresRestOpt;
    std::unique_ptr<torch::optim::Adam> opacitiesOpt;

    // std::unique_ptr<OptimScheduler> meansOptScheduler;
    std::unique_ptr<ExponentialLR> meansOptScheduler;
    std::unique_ptr<ExponentialLR> scalesOptScheduler;
    int lastHeight; // set in forward()
    int lastWidth; // set in forward()

    torch::Tensor backgroundColor;
    torch::Device device;
    SSIM ssim;

    float eps2d = 0.3;
    float nearPlane = 0.01f;
    float farPlane = 1e10;
    float radiusClip = 0.0f;
    int numCameras;
    GaussianTrainConfig& trainConfig;

    float scale;
    torch::Tensor translation;
    std::unique_ptr<class PruneStrategy> pruneStrategy;
    std::unique_ptr<class DensifyStrategy>  densifyStrategy;
    float   sceneScale = 1.0f;
    torch::Tensor   pointsCenter;
    float           skyDist = 0.0;
    glm::vec3   boundsMin;
    glm::vec3   boundsMax;
};


std::shared_ptr<GaussianTrainModel> createGaussianModel(
        const InputData& inputData, 
        int numCameras,
        GaussianTrainConfig& config,
        const torch::Device& device);
std::shared_ptr<GaussianTrainModel> createGaussianModel(
        const std::filesystem::path& filename, 
        const InputData& inputData,
        int numCameras,
        GaussianTrainConfig& config,
        const torch::Device& device);

int numShBases(int degree);

#endif