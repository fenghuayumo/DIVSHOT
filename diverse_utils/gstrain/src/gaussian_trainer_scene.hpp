#pragma once

#include <core/core.h>
#include <filesystem>
#include <glm/gtc/quaternion.hpp>
#include <gaussian_train_config.hpp>

class GaussianTrainModel;
class CameraData;

enum class TrainingStatus
{
    Loading_Failed = -1,
    Loading_Prepare = 0,
    MaskGen,
    Colmap_FeatureExtract,
    Colmap_FeatureMatch,
    Colmap_Sfm,
    Preprocess_TrainingData,
    Preprocess_Done,
    Training,
    GS2Mesh,
    Training_Done
};
extern "C"
{
    DS_EXPORT int gstrain_init();
    DS_EXPORT int gstrain_destroy();
    DS_EXPORT int get_camera_pos_type_from_file(const std::string& file_path);
    DS_EXPORT bool is_device_support_gstrain();
    DS_EXPORT bool is_driver_support();
}
#ifndef COLMAP_SPARSEPOINT
#define COLMAP_SPARSEPOINT
namespace colmap{

    template<typename T>
    struct vec3 {
        T x, y, z;
    };

    template <typename T>
    struct vec4 {
        T x, y, z, w;
    };

    struct SparsePoint {
        vec3<float> xyz;
        vec3<unsigned char> color;
    };
    struct CameraTrack {
        uint32_t camera_id;
        int model_id;
        size_t width = 0;
        size_t height = 0;
        std::vector<double> params;
    };

    struct ImageTrack {
        uint32_t image_id;
        std::string name;
        uint32_t camera_id;
        vec4<float> rotation;
        vec3<float>  translation;
    };
}
#endif

struct SplatImageView{
    u32 width;
    u32 height;
    std::string name;
    std::vector<u8> data;
};
namespace o3d {
    class TSDF;
}
class DS_EXPORT GaussianTrainerScene
{
public:
    GaussianTrainerScene();
    GaussianTrainerScene(const GaussianTrainConfig& config,  int loadIteration = 0);
    ~GaussianTrainerScene();
    auto    loadTrainData()->bool;
    auto    loadTrainData(const std::string& fpath)->bool;
    auto    getGaussianModel()-> GaussianTrainModel& {return *gaussian;}
    auto    trainStep()->void;
    auto    getCurrentIterations() const ->int {return curIteration;}
    auto    isTrain()-> bool& {return istraining;}
    auto    startTrain()->void;
    auto    pauseTrain()->void;
    auto    trainSetup()->void;
    auto    isPruningSplat()->bool {return isPruning;}
    auto    getGaussianPositionCpu()->std::vector<float>;
    auto    getGaussianOpcaitiesCpu()-> std::vector<float>;
    auto    getGaussianRotationsCpu()-> std::vector<float>;
    auto    getGaussianScalingsCpu()-> std::vector<float>;
    auto    getGaussianSH0Cpu() -> std::vector<float>;
    auto    getGaussianSHNCpu() -> std::vector<float>;
    auto    getNumGaussians()->size_t;
    auto    getNumCameras() -> size_t;
    auto    getCameraPos(int idx = 0)->glm::vec3;
    auto    getCameraRotation(int idx =0)->glm::quat;
    auto    getCameraProjection(int idx= 0)->glm::mat4;
    int&    maxIteriaons() {return trainConfig.numIters;}
    void    setMaxIterations(int iteration) { trainConfig.numIters = iteration;}
    void    setMaxTrainImageExtent(const std::array<int,2>& extent);
    void    setMaxTrainImageCount(const int maxImageCount){ trainConfig.maxImageCount = maxImageCount;}
    void    setModelPath(const std::string& path) { trainConfig.modelPath = path;}
    auto    setDensifyStrategy(int type)->void;
    auto    getTrainConfig()-> GaussianTrainConfig& {return trainConfig;}
    void    saveGaussianModel() const;
    void    saveCameraDatas(const std::string& filePath) const;
    void    updateTensorFromGaussianData(
            const std::vector<glm::vec3>& pos,
            const std::vector<glm::vec4>& rot,
            const std::vector<glm::vec3>& scale,
            const std::vector<float>& opacity,
            const std::vector<std::array<float, 3>>& shs_0,
            const std::vector<std::array<float, 45>>& shs_n
            );
    void    updateFocusRegion(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale);
    auto    getFocusRegion()->std::tuple<glm::vec3, glm::vec3>;
    auto    getFocusRegionTransform()->glm::mat4;
    void    resetGaussian();
    template <typename Archive>
    void save(Archive& archive) const
    {
        const auto camPath = std::filesystem::path(trainConfig.modelPath).parent_path() / "transforms.json";
        saveCameraDatas(camPath.string());
        const auto plyPath = std::filesystem::path(trainConfig.modelPath).parent_path() / "sparse_pc.ply";
        exportSparsePointCloud(plyPath.string());
        saveGaussianModel();
        archive(cereal::make_nvp("currentIteraion", curIteration), cereal::make_nvp("maxIteraion", trainConfig.numIters));
        archive(cereal::make_nvp("trainConfig", trainConfig));
    }

    template <typename Archive>
    void load(Archive& archive)
    {
        archive(cereal::make_nvp("currentIteraion", curIteration), cereal::make_nvp("maxIteraion", trainConfig.numIters));
        archive(cereal::make_nvp("trainConfig", trainConfig));
        loadGaussianTraningModel();
    }
    auto    loadGaussianTraningModel()->void;
    auto    getCurrentTrainingStatus()-> TrainingStatus;
    auto    setTrainingStatus(TrainingStatus phase)->void { curTrainStatus = phase;}
    auto    getProgressOnCurrentPhase() const->float;
    auto    getCurrentTrainingPhaseName() const->std::string;
    auto    getCameraPosFromImage(const std::string& data_source,bool is_video = true)->int;
    auto    getCurrentLoss() const->float{return currentLoss;}

    auto    getPoints3D(int id) const->std::vector<colmap::SparsePoint>;
    auto    exportSparsePointCloud(const std::string& filePath)const->void;
    auto    exportMesh(const std::string& filePath)->bool const;
    auto    getSplatImageView(int id)->SplatImageView const;
    auto    getEstimateTrainingTime()->float;
    auto    getTrainingElpasedTime()->float;
    auto    isTerminate() const->bool {return isTerminated;}
public:
    bool            ShowTrainView = false;
    std::shared_ptr<struct ColmapSparseReconstruct> sparseRecons;
    std::vector<int>    pruenIteraions;
    int                 curIteration = 0;
    std::shared_ptr<class GaussianInputDataLoader>     dataLoader;
    std::shared_ptr<o3d::TSDF>     tsdf;
    glm::vec3         focus_region_position;
    glm::vec3         focus_region_rotation;
    glm::vec3         focus_region_scale = glm::vec3(1);
    glm::vec3         init_region_box_min;
    glm::vec3         init_region_box_max;
protected:
    std::shared_ptr<GaussianTrainModel>   gaussian;
    GaussianTrainConfig trainConfig;
    bool             istraining = false;

    TrainingStatus     curTrainStatus = TrainingStatus::Loading_Prepare;
    float            currentLoss = 0.0;
    float            totalLoss = 0;
    bool             isPruning = false;
    bool             isTerminated = false; 
    float            trainingElpasedTime = 0.0f;
};
