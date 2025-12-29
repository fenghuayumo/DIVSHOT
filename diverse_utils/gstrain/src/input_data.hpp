#ifndef INPUTDATA_H
#define INPUTDATA_H

#include <iostream>
#include <string>
#include <fstream>
#include <unordered_map>
#include <opencv2/calib3d.hpp>
#include <torch/torch.h>
#include <glm/glm.hpp>
#include "colmap_data.hpp"
namespace dvs {
    enum CameraModelType {
        PINHOLE = 0,
        ORTHO = 1,
        FISHEYE = 2,
        FTHETA = 3
    };
}
enum class EImageDataType {
    Byte,
    Half,
    Float,
};

enum class EDatasetType{
    None = -1,
    NerfStudio = 0,
    Colmap,
    OpenSfm,
    RealityCapture,
    MetaShape,
    Blender,
};

struct RayBundle
{
    torch::Tensor origins;
    torch::Tensor directions;
    torch::Tensor radiis;
    torch::Tensor rayCos;
};

// Forward declaration removed - will use cache in data_loader

struct Camera{
    int id = -1;
    int width = 0;
    int height = 0;
    float fx = 0;
    float fy = 0;
    float cx = 0;
    float cy = 0;
    float k1 = 0;
    float k2 = 0;
    float k3 = 0;
    float k4 = 0;
    float k5 = 0;
    float k6 = 0;
    float p1 = 0;
    float p2 = 0;
    uint8_t model_type = 0;
    torch::Tensor worldToCam;
    std::string filePath = "";
    
    // Flag for lazy loading in pipeline mode
    // Note: Not using atomic since image_cpu.empty() check provides actual protection
    bool is_loaded_ = false;

    Camera(){};
    Camera(int width, int height, float fx, float fy, float cx, float cy, 
        float k1, float k2, float k3, float p1, float p2,
        const torch::Tensor &worldToCam, const std::string &filePath);
    
    static torch::Tensor getViewMatTensor(const torch::Tensor& camToWorld);

    torch::Tensor getIntrinsicsMatrix(int downscaleFactor = 1);
    torch::Tensor getCamToWorldFromViewMat();
    torch::Tensor getProjectionMatrix(int downFactor=1,float near=0.01f,float farp=1000.0f);
    bool hasDistortionParameters();
    std::vector<float> undistortionParameters();
    
    // Stage 2: Convert CPU data (cv::Mat) to GPU Tensor
    // These methods perform the conversion on-demand, reducing memory peaks
    torch::Tensor getImage(int downscaleFactor);
    torch::Tensor getNormal(int downscaleFactor);
    torch::Tensor getDepth(int downscaleFactor);
    torch::Tensor getMask(int downscaleFactor);
    
    RayBundle     getCameraRay(int downscaleFactor,bool c2w = true,bool cone_trace = false);
    RayBundle     getRandomCameraRay(int downscaleFactor,int numRays = 8192,bool normalize_ray = true);

    // Stage 1: Load image data to CPU memory (cv::Mat)
    // Lightweight storage compared to torch::Tensor
    void loadImage(int maxWidth, int maxHeight, 
                   bool useMask = false,
                   bool useDepth = false,
                   bool useNormal = false);

    void extractCameraPosRotation(const torch::Tensor& camToWorld);
    glm::vec3 getCameraPos() const;
    glm::mat3 getCameraRotation() const;
    glm::quat getRotationQuat() const;
    glm::mat4 getProjMat() const;

    // Camera parameters
    torch::Tensor K;
    torch::Tensor T;
    torch::Tensor radial_distortion;
    torch::Tensor tangential_distortion;
    glm::vec3     cameraPos;
    glm::mat3     rotation;
    
    // Stage 1: CPU storage (lightweight cv::Mat)
    // These hold the raw image data loaded from disk
    cv::Mat       image_cpu;      // RGB/RGBA image in CPU memory
    cv::Mat       normal_cpu;     // Normal map in CPU memory
    cv::Mat       depth_cpu;      // Depth map in CPU memory
    cv::Mat       mask_cpu;       // Mask in CPU memory
    
    // Pyramid cache (GPU tensors cached at different scales)
    std::unordered_map<int, torch::Tensor> intricsPyramids;
    std::unordered_map<int, torch::Tensor> imagePyramids;   // GPU
    std::unordered_map<int, torch::Tensor> maskPyramids;
    std::unordered_map<int, torch::Tensor> depthPyramids;   // GPU
    std::unordered_map<int, torch::Tensor> normalPyramids;
    
    torch::Tensor  pointDepth;
    torch::Tensor  point2D;
    RayBundle      cameraRays;
    std::unordered_map<int, RayBundle>  rayBoundles;
    int             prevDownscaleFactor = -1;
};

struct Points{
    torch::Tensor xyz;
    torch::Tensor rgb;
};
struct InputData{
    std::vector<Camera> cameras;
    float scale;
    torch::Tensor translation;
    Points points;
    std::tuple<std::vector<Camera*>, std::vector<Camera*>> getCameras(bool validate, int randCount = 5);

    void saveCameras(const std::string &filename, bool keepCrs);
    void saveSparsePointSet(const std::string &filename);
};
InputData inputDataFromX(const std::string & imagPath,const std::string&  cameraPosePath="", const std::string& pointPath="",EDatasetType dataType = EDatasetType::None);

InputData inputDataFromColmapCameraPoints(const std::string& imagPath,std::vector<
    colmap::CameraTrack>&& cameras, std::vector<colmap::SparsePoint>&& points,std::vector<colmap::ImageTrack>&& imgs);

torch::Device get_gsplat_device();
int gsplat_init();
#endif