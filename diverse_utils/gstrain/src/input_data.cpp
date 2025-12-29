#include <filesystem>
#include <json/json.hpp>
#include "input_data.hpp"
#include "cv_utils.hpp"
#include "utils.hpp"
#include "point_io.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/ext.hpp>
#include <algorithm>
#include <random>
namespace fs = std::filesystem;
using namespace torch::indexing;
using json = nlohmann::json;

namespace ns{ InputData inputDataFromNerfStudio(const std::string &projectRoot,const std::string& posePath,const std::string& pointCloudPath); }
namespace cm{ InputData inputDataFromColmap(const std::string &imagePath,const std::string& posePath,const std::string& pointCloudPath); }
namespace metashape {InputData inputDataFromMetaShape(const std::string &imagePath,const std::string& posePath,const std::string& pointCloudPath);}
namespace realitycapture {InputData inputDataFromRealityCapture(const std::string &imagePath,const std::string& posePath,const std::string& pointCloudPath);}
namespace blender {InputData inputDataFromBlender(const std::string &imagePath,const std::string& posePath,const std::string& pointCloudPath);}

#ifdef USE_CUDA
torch::Device device = torch::kCUDA;
#elif defined(USE_MPS)
torch::Device device = torch::kMPS;
#else
torch::Device device = torch::kCPU;
#endif

int gsplat_init()
{
    if (torch::hasCUDA() ) {
        std::cout << "Using CUDA" << std::endl;
        device = torch::kCUDA;
    }
    else if (torch::hasMPS() ) {
        std::cout << "Using MPS" << std::endl;
        device = torch::kMPS;
    }
    else {
        device = torch::kCPU;
        std::cout << "Using CPU" << std::endl;
    }
    return 1;
}

int gsplat_destroy()
{
    return 1;
}

torch::Device get_gsplat_device() 
{
    return device;
}

InputData inputDataFromX(const std::string & imagePath,const std::string&  cameraPosePath, const std::string& pointPath,EDatasetType dataType){
    auto projectRoot = fs::path(imagePath).parent_path().string();
    fs::path root(projectRoot);
    if( dataType != EDatasetType::None){
        if(  dataType == EDatasetType::NerfStudio ){
            return ns::inputDataFromNerfStudio(imagePath,cameraPosePath,pointPath);
        }else if( dataType == EDatasetType::Colmap){
            return cm::inputDataFromColmap(imagePath,cameraPosePath,pointPath);
        }else if( dataType == EDatasetType::RealityCapture){
            return realitycapture::inputDataFromRealityCapture(imagePath,cameraPosePath,pointPath);
        }else if( dataType == EDatasetType::MetaShape){
            return metashape::inputDataFromMetaShape(imagePath,cameraPosePath,pointPath);
        }else if(dataType == EDatasetType::Blender){
            return blender::inputDataFromBlender(imagePath, cameraPosePath, pointPath);
        }
        throw std::runtime_error("Invalid project folder (must be a valid dataset type, now support colmap/nerfstudio/oepnsfm/realitycapture/metashape)");
    }
    if (fs::exists(root / "transforms.json") || fs::exists(root / "dvs_cameras.json")){
        return ns::inputDataFromNerfStudio(imagePath,cameraPosePath,pointPath);
    }else if (fs::exists(root / "sparse") || fs::exists(root / "cameras.bin") || fs::exists(root / "cameras.txt")){
        return cm::inputDataFromColmap(imagePath, cameraPosePath, pointPath);
    }else if( fs::exists(root / "transforms.csv")){
        return realitycapture::inputDataFromRealityCapture(imagePath, cameraPosePath, pointPath);
    }else if (fs::exists(root / "transforms.xml")) {
        return metashape::inputDataFromMetaShape(imagePath, cameraPosePath, pointPath);
    }else{
        throw std::runtime_error("Invalid project folder (must be either a colmap or nerfstudio project folder)");
    }
}

torch::Tensor Camera::getIntrinsicsMatrix(int downscaleFactor){
    if( downscaleFactor <= 1){
        K = K.to(device);
        return K;
    }

    if (intricsPyramids.find(downscaleFactor) != intricsPyramids.end()) {
        return intricsPyramids[downscaleFactor];
    }
    torch::Tensor t = torch::tensor({ {fx / downscaleFactor, 0.0f, cx / downscaleFactor},
                    {0.0f, fy / downscaleFactor, cy / downscaleFactor},
                    {0.0f, 0.0f, 1.0f} }, torch::kFloat32).to(device);
    intricsPyramids[downscaleFactor] = t;
    return t;
}

torch::Tensor Camera::getProjectionMatrix(int downscaleFactor,float near,float far){
    float fx = this->fx / downscaleFactor;
    float fy = this->fy / downscaleFactor;
    float cx = this->cx / downscaleFactor;
    float cy = this->cy / downscaleFactor;
    int height = this->height / downscaleFactor;
    int width = this->width / downscaleFactor;
    float fovX = 2.0 * std::atan(width / (2.0 * fx));
    float fovY = 2.0 * std::atan(height / (2.0 * fy));
    float y = std::tan(fovY / 2.0f);
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    return torch::tensor({
		{1.0f / (aspect * y), 0.0f, 0.0f, 0.0f},
		{0.0f, -1.0f / y, 0.0f, 0.0f},
		{0.0f, 0.0f, -(far + near) / (far - near), -(2.0f * far * near) / (far - near)},
		{0.0f, 0.0f, -1.0f, 0.0f} 
	},torch::kFloat32).to(device);
}

void Camera::loadImage(int maxWidth,
                       int maxHeight, 
                       bool useMask,
                       bool useDepth, 
                       bool useNormal){
    // Stage 1: Load image to CPU memory (cv::Mat) - Lightweight storage
    // This reduces memory peaks during parallel loading
    // Conversion to GPU Tensor happens later in getImage()
    
    // Quick check: if already loaded, skip
    if (is_loaded_) {
        return;
    }
    
    // Double-check with image_cpu (handles race condition gracefully)
    if (!image_cpu.empty()) {
        is_loaded_ = true;
        return;
    }
    maxHeight = std::min(maxHeight,height);
    maxWidth = std::min(maxWidth,width);
    float downscaleFactor = 1.0;
    cv::Mat cImg = imreadRGB(filePath, cv::IMREAD_UNCHANGED);
    if (cImg.rows > maxHeight || cImg.cols > maxWidth) {
        float downscaleFactor1 = static_cast<float>(cImg.rows) / static_cast<float>(maxHeight);
        float downscaleFactor2 = static_cast<float>(cImg.cols) / static_cast<float>(maxWidth);
        downscaleFactor = std::max(downscaleFactor1, downscaleFactor2);
    }
    float scaleFactor = 1.0f / downscaleFactor;
    float rescaleF = 1.0f;
    // If camera intrinsics don't match the image dimensions 
    //if (cImg.rows != height || cImg.cols != width){
    //    rescaleF = static_cast<float>(cImg.rows) / static_cast<float>(height);
    //}
    fx *= scaleFactor * rescaleF;
    fy *= scaleFactor * rescaleF;
    cx *= scaleFactor * rescaleF;
    cy *= scaleFactor * rescaleF;

    if (downscaleFactor > 1.0f){
        float f = 1.0f / downscaleFactor;
        cv::resize(cImg, cImg, cv::Size(), f, f, cv::INTER_AREA);
    }
    width = cImg.cols;
    height = cImg.rows;
    K = torch::tensor({ {fx, 0.0f, cx},
                        {0.0f, fy, cy},
                        {0.0f, 0.0f, 1.0f} }, torch::kFloat32);
    
    // Store image in CPU memory as cv::Mat (lightweight) as uint8
    if (cImg.type() == CV_32FC3 || cImg.type() == CV_32FC4) {
        cImg.convertTo(image_cpu, CV_8U, 255.0);
    } else {
        image_cpu = cImg;
    }
    
    // fx = K[0][0].item<float>();
    // fy = K[1][1].item<float>();
    // cx = K[0][2].item<float>();
    // cy = K[1][2].item<float>();
    // K = K.to(device);
    // T = T.to(device);
    // worldToCam = worldToCam.to(device);
    
    // Load mask image to CPU memory
    if(useMask){
        const auto fileName = std::filesystem::path(filePath).filename();
        const auto maskPath = std::filesystem::path(filePath).parent_path().parent_path() / "masks" / fileName;
        if(std::filesystem::exists(maskPath)){
            cv::Mat maskImg = imreadRGB(maskPath.string(), cv::IMREAD_GRAYSCALE);
            if (downscaleFactor > 1.0f) {
                float f = 1.0f / downscaleFactor;
                cv::resize(maskImg, maskImg, cv::Size(), f, f, cv::INTER_AREA);
            }
            // Invert mask (logical_not)
            mask_cpu = 255 - maskImg;
        }
        else if (!image_cpu.empty() && image_cpu.channels() == 4) {
            // Extract alpha channel as mask
            cv::Mat alphaChannel;
            cv::extractChannel(image_cpu, alphaChannel, 3);
            alphaChannel.convertTo(alphaChannel, CV_8U);
            cv::Mat maskImg(alphaChannel.size(), CV_8U, cv::Scalar(0));
            cv::threshold(alphaChannel, maskImg, 200, 255, cv::THRESH_BINARY_INV);
            mask_cpu = maskImg;
        }
    }
    
    // Load normal image to CPU memory
    if (useNormal) {
        const auto fileName = std::filesystem::path(filePath).stem();
        auto normalPath = std::filesystem::path(filePath).parent_path().parent_path() / "normal" / fileName;
        const auto jpgPath = normalPath.replace_extension(".jpg");
        const auto pngPath = normalPath.replace_extension(".png");
        if (std::filesystem::exists(pngPath) || std::filesystem::exists(jpgPath)) {
            normalPath = std::filesystem::exists(jpgPath) ? jpgPath : pngPath;
            cv::Mat normalImg = imreadRGB(normalPath.string());
            if (downscaleFactor > 1.0f) {
                float f = 1.0f / downscaleFactor;
                cv::resize(normalImg, normalImg, cv::Size(), f, f, cv::INTER_AREA);
            }
            // Store as cv::Mat in CPU memory as uint8
            normal_cpu = normalImg;
        }
    }
    
    // Load depth image to CPU memory
    if (useDepth) {
        const auto fileName = std::filesystem::path(filePath).stem();
        auto depthPath = std::filesystem::path(filePath).parent_path().parent_path() / "depth" / fileName;
        const auto jpgPath = depthPath.replace_extension(".jpg");
        const auto pngPath = depthPath.replace_extension(".png");
        if (std::filesystem::exists(pngPath) || std::filesystem::exists(jpgPath)) {
            depthPath = std::filesystem::exists(jpgPath) ? jpgPath : pngPath;
            cv::Mat depthImg = imreadRGB(depthPath.string());
            if (downscaleFactor > 1.0f) {
                float f = 1.0f / downscaleFactor;
                cv::resize(depthImg, depthImg, cv::Size(), f, f, cv::INTER_AREA);
            }
            // Convert to single channel and store as cv::Mat
            cv::Mat depthGray;
            if (depthImg.channels() > 1) {
                cv::extractChannel(depthImg, depthGray, 0);
            } else {
                depthGray = depthImg;
            }
            depth_cpu = depthGray;
        }
    }
    
    // Apply mask to images in CPU memory if needed
    if (!mask_cpu.empty()) {
        cv::Mat mask_3ch;
        if (image_cpu.channels() == 3) {
            cv::cvtColor(mask_cpu, mask_3ch, cv::COLOR_GRAY2BGR);
        } else if (image_cpu.channels() == 4) {
            cv::cvtColor(mask_cpu, mask_3ch, cv::COLOR_GRAY2BGRA);
        }
        
        if (!mask_3ch.empty()) {
            image_cpu.setTo(cv::Scalar(0, 0, 0, 0), mask_3ch);
        }
        
        if (!depth_cpu.empty()) {
            depth_cpu.setTo(cv::Scalar(0), mask_cpu);
        }
        
        if (!normal_cpu.empty()) {
            if (normal_cpu.channels() == 3) {
                cv::cvtColor(mask_cpu, mask_3ch, cv::COLOR_GRAY2BGR);
            }
            normal_cpu.setTo(cv::Scalar(0, 0, 0), mask_3ch);
        }
    }
    
    // Mark as loaded (thread-safe)
    is_loaded_ = true;
}

torch::Tensor Camera::getMask(int downscaleFactor){
    // Stage 2: Convert CPU mask data to GPU Tensor
    
    if (prevDownscaleFactor > 0 && downscaleFactor != prevDownscaleFactor) {
       maskPyramids.clear();
    }
    
    // Return empty tensor if no mask available
    if (mask_cpu.empty()) {
        return torch::Tensor();
    }
    
    if (downscaleFactor <= 1) {
        torch::Tensor t = maskToTensor(mask_cpu);
        return t.to(device);
    } else {
        if (maskPyramids.find(downscaleFactor) != maskPyramids.end()) {
            return maskPyramids[downscaleFactor].to(device);
        }
        cv::Mat cImg;
        cv::resize(mask_cpu, cImg, cv::Size(mask_cpu.cols / downscaleFactor, mask_cpu.rows / downscaleFactor), 0.0, 0.0, cv::INTER_AREA);
        torch::Tensor t = maskToTensor(cImg);
        maskPyramids[downscaleFactor] = t;
        return t.to(device);
    }
}

torch::Tensor Camera::getImage(int downscaleFactor){
    // Stage 2: Convert CPU data (cv::Mat) to GPU Tensor on-demand
    // This method is called during training, avoiding memory peaks during loading
    
    if(prevDownscaleFactor > 0 && downscaleFactor != prevDownscaleFactor){
        imagePyramids.clear();
    }
    
    // Use cv::Mat as source if available (new pipeline)
    if (!image_cpu.empty()) {
        if (downscaleFactor <= 1) {
            // Direct conversion from cv::Mat to Tensor
            torch::Tensor t = imageToTensor(image_cpu);
            
            return t.to(device);
        } else {
            // Check cache first
            if (imagePyramids.find(downscaleFactor) != imagePyramids.end()) {
                return imagePyramids[downscaleFactor].to(device);
            }

            // Downscale on CPU, then convert to Tensor
            cv::Mat cImg;
            cv::resize(image_cpu, cImg, cv::Size(image_cpu.cols / downscaleFactor, image_cpu.rows / downscaleFactor), 0.0, 0.0, cv::INTER_AREA);
            torch::Tensor t = imageToTensor(cImg);
            
            imagePyramids[downscaleFactor] = t;
            prevDownscaleFactor = downscaleFactor;
            return imagePyramids[downscaleFactor].to(device);
        }
    }
    
    // If no image_cpu available, throw exception instead of returning None Tensor
    // This prevents silent crashes during training loss computation
    throw std::runtime_error("Camera::getImage() - image_cpu is empty! Image not loaded for: " + filePath);
}

torch::Tensor Camera::getDepth(int downscaleFactor) {
    // Stage 2: Convert CPU depth data to GPU Tensor
    
    if (prevDownscaleFactor > 0 && downscaleFactor != prevDownscaleFactor) {
        depthPyramids.clear();
    }
    
    // Use cv::Mat as source if available (new pipeline)
    if (!depth_cpu.empty()) {
        if (downscaleFactor <= 1) {
            torch::Tensor t = imageToTensor(depth_cpu);
            // t = t.index({torch::indexing::Slice(), torch::indexing::Slice(), 0}).unsqueeze(2);
            return t.to(device);
        } else {
            if (depthPyramids.find(downscaleFactor) != depthPyramids.end()) {
                return depthPyramids[downscaleFactor].to(device);
            }
            cv::Mat cImg;
            cv::resize(depth_cpu, cImg, cv::Size(depth_cpu.cols / downscaleFactor, depth_cpu.rows / downscaleFactor), 0.0, 0.0, cv::INTER_AREA);
            torch::Tensor t = imageToTensor(cImg);
            // t = t.index({torch::indexing::Slice(), torch::indexing::Slice(), 0}).unsqueeze(2);
            depthPyramids[downscaleFactor] = t;
            return t.to(device);
        }
    }
    
    // If no depth_cpu available, return empty tensor
    return torch::Tensor();
}

torch::Tensor Camera::getNormal(int downscaleFactor) {
    // Stage 2: Convert CPU normal data to GPU Tensor
    
    if (prevDownscaleFactor > 0 && downscaleFactor != prevDownscaleFactor) {
        normalPyramids.clear();
    }
    
    // Use cv::Mat as source if available (new pipeline)
    if (!normal_cpu.empty()) {
        if (downscaleFactor <= 1) {
            torch::Tensor t = imageToTensor(normal_cpu);
            return t.to(device);
        } else {
            if (normalPyramids.find(downscaleFactor) != normalPyramids.end()) {
                return normalPyramids[downscaleFactor].to(device);
            }
            cv::Mat cImg;
            cv::resize(normal_cpu, cImg, cv::Size(normal_cpu.cols / downscaleFactor, normal_cpu.rows / downscaleFactor), 0.0, 0.0, cv::INTER_AREA);
            torch::Tensor t = imageToTensor(cImg);
            normalPyramids[downscaleFactor] = t;
            return t.to(device);
        }
    }
    
    // If no normal_cpu available, return empty tensor
    return torch::Tensor();
}

RayBundle Camera::getCameraRay(int downscaleFactor,bool transformed_to_w,bool cone_trace)
{
    if (prevDownscaleFactor > 0 && downscaleFactor != prevDownscaleFactor) {
        //clear previous pyramids
        rayBoundles.clear();
    }
    if (downscaleFactor <= 1) {
        if(cameraRays.directions.numel() > 0) return cameraRays;
    }
    if (rayBoundles.find(downscaleFactor) != rayBoundles.end()) {
        return rayBoundles[downscaleFactor];
    }
    float sign_z = -1;
    float fx = this->fx / downscaleFactor;
    float fy = this->fy / downscaleFactor;
    float cx = this->cx / downscaleFactor;
    float cy = this->cy / downscaleFactor;
    int height = this->height / downscaleFactor;
    int width = this->width / downscaleFactor;
    auto xy = torch::meshgrid(
        {torch::arange(width, device),
        torch::arange(height, device)},"xy"
    );
    auto x = xy[0];auto y = xy[1];
    torch::Tensor directions = torch::stack({
      (x - cx + 0.5f) / fx,
      (y - cy + 0.5f) / fy * sign_z,
      torch::full_like(x, sign_z)
    }, -1).to(torch::kFloat32); // [H,W,3]
    torch::Tensor radii,ray_cos;
    if(cone_trace){
        auto dx = torch::linalg_norm((directions.index({ Slice(), Slice(None, -1), Slice() }) -
            directions.index({ Slice(), Slice(1, None), Slice() })),std::optional<c10::Scalar>{}, -1, true,{}).to(torch::kFloat32);
        dx = torch::cat({ dx, dx.index({Slice(), Slice(-2,-1), Slice()})}, 1); // [H,W,1]
        torch::Tensor dy = torch::linalg_norm(
            (directions.index({ Slice(None, -1), Slice(), Slice() }) -
            directions.index({ Slice(1, None), Slice(), Slice() })),
            std::optional<c10::Scalar>{}, -1, true, {}
        ).to(torch::kFloat32); // [H-1,W,1]
        dy = torch::cat({ dy, dy.index({Slice(-2,-1), Slice(), Slice()})}, 0); // [H,W,1]
        torch::Tensor area = dx * dy;
        radii = torch::sqrt(area / 3.1415926f).to(torch::kFloat32);
    }
    
    directions = directions / torch::linalg_norm(directions, std::optional<c10::Scalar>{}, -1, true, {});
    directions = directions.to(torch::kFloat32);
    if(cone_trace){
        ray_cos = torch::matmul(directions, torch::tensor({{0.0f, 0.0f, sign_z}}, device).t()).to(torch::kFloat32);
    }
    auto origin = torch::zeros({1,3}, torch::kFloat32).to(device);
    if(transformed_to_w) {
        auto camToWorld = this->getCamToWorldFromViewMat();
        auto c2w = camToWorld.to(device).repeat({height * width,1,1});
        directions = directions.view({height * width, 3});
        directions = torch::matmul(c2w.index({Slice(None), Slice(None, 3), Slice(None,3)}), directions.unsqueeze(-1)).squeeze(-1);
        origin = camToWorld.index({Slice(None, 3), Slice(-1)}).view({1,3}).to(device);
    }
    RayBundle rayBoundle = {
        torch::zeros({1,3}, torch::kFloat32).to(device),
        directions,
        radii,
        ray_cos
    };
    if(downscaleFactor == 1) cameraRays = rayBoundle;
    else rayBoundles[downscaleFactor] = rayBoundle;
    return rayBoundle;
}

RayBundle Camera::getRandomCameraRay(int downscaleFactor,int numRays, bool normalize_ray)
{
    if (rayBoundles.find(downscaleFactor) != rayBoundles.end()) {
        auto ray = rayBoundles[downscaleFactor];

        auto sample_x = torch::randint(
            0,
            width,
            {numRays,}
        );
        auto sample_y = torch::randint(
            0,
            height,
            {numRays,}
        );
        
        RayBundle sampledRay;
        sampledRay.directions = ray.directions[sample_y][sample_x];
        sampledRay.origins = ray.origins[sample_y][sample_x];
        sampledRay.radiis = ray.radiis[sample_y][sample_x];
        sampledRay.rayCos = ray.rayCos[sample_y][sample_x];
        return sampledRay;
    }
    float sign_z = -1;
    float fx = this->fx / downscaleFactor;
    float fy = this->fy / downscaleFactor;
    float cx = this->cx / downscaleFactor;
    float cy = this->cy / downscaleFactor;
    int height = this->height / downscaleFactor;
    int width = this->width / downscaleFactor;
    auto xy = torch::meshgrid(
        { torch::arange(width, device),
        torch::arange(height, device) }, "xy"
    );
    auto x = xy[0]; auto y = xy[1];
    torch::Tensor directions = torch::stack({
      (x - cx + 0.5f) / fx,
      (y - cy + 0.5f) / fy * sign_z,
      torch::full_like(x, sign_z)
        }, -1).to(torch::kFloat32); // [H,W,3]
    auto dx = torch::linalg_norm((directions.index({ Slice(), Slice(None, -1), Slice() }) -
        directions.index({ Slice(), Slice(1, None), Slice() })), std::optional<c10::Scalar>{}, -1, true, {}).to(torch::kFloat32);
    dx = torch::cat({ dx, dx.index({Slice(), Slice(-2,-1), Slice()}) }, 1); // [H,W,1]
    torch::Tensor dy = torch::linalg_norm(
        (directions.index({ Slice(None, -1), Slice(), Slice() }) -
            directions.index({ Slice(1, None), Slice(), Slice() })),
        std::optional<c10::Scalar>{}, -1, true, {}
    ).to(torch::kFloat32); // [H-1,W,1]
    dy = torch::cat({ dy, dy.index({Slice(-2,-1), Slice(), Slice()}) }, 0); // [H,W,1]
    torch::Tensor area = dx * dy;
    torch::Tensor radii = torch::sqrt(area / 3.1415926f).to(torch::kFloat32);

    if (normalize_ray) {
        directions = directions / torch::linalg_norm(directions, std::optional<c10::Scalar>{}, -1, true, {});
    }
    directions = directions.to(torch::kFloat32);
    RayBundle rayBoundle = {
        torch::zeros({1,3}, torch::kFloat32).to(device),
        directions,
        radii,
        torch::matmul(directions, torch::tensor({{0.0f, 0.0f, sign_z}}, device).t()).to(torch::kFloat32)
    };

    rayBoundles[downscaleFactor] = rayBoundle;
    auto sample_x = torch::randint(
        0,
        width,
        { numRays, }
    );
    auto sample_y = torch::randint(
        0,
        height,
        { numRays, }
    );
    RayBundle sampledRay;
    sampledRay.directions = rayBoundle.directions[sample_y][sample_x];
    sampledRay.origins = rayBoundle.origins[sample_y][sample_x];
    sampledRay.radiis = rayBoundle.radiis[sample_y][sample_x];
    sampledRay.rayCos = rayBoundle.rayCos[sample_y][sample_x];
    return sampledRay;
}

bool Camera::hasDistortionParameters(){
    return k1 != 0.0f || k2 != 0.0f || k3 != 0.0f || p1 != 0.0f || p2 != 0.0f;
}

std::vector<float> Camera::undistortionParameters(){
    std::vector<float> p = { k1, k2, p1, p2, k3, 0.0f, 0.0f, 0.0f };
    return p;
}

std::tuple<std::vector<Camera*>, std::vector<Camera*>> InputData::getCameras(bool validate, int validateNumbers){
    //random validate cameras
    std::vector<Camera*> trainCameras;
    std::vector<Camera*> validateCameras;
    if (validate) {
        std::default_random_engine engine(100);
        std::vector<int> indices(cameras.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), engine);
        for (int i = 0; i < validateNumbers; i++) {
            validateCameras.push_back(&cameras[indices[i]]);
        }
        for (int i = validateNumbers; i < cameras.size(); i++) {
            trainCameras.push_back(&cameras[indices[i]]);
        }
    }
    else {
        for (int i = 0; i < cameras.size(); i++) {
            trainCameras.push_back(&cameras[i]);
        }
    }
    return std::make_tuple(trainCameras,validateCameras);
}


void InputData::saveCameras(const std::string &filename, bool keepCrs){
    json j = json::array();
    float camera_angle_x = 0.5;
    float fl_x = 0.5;
    float fl_y = 0.5;
    for (size_t i = 0; i < cameras.size(); i++){
        Camera &cam = cameras[i];

        json camera = json::object();
        camera["id"] = i;
        auto image_path = fs::path(cam.filePath).parent_path().filename() / fs::path(cam.filePath).filename();
        camera["file_path"] = image_path.string();
        camera["w"] = cam.width;
        camera["h"] = cam.height;
        camera["fx"] = cam.fx;
        camera["fy"] = cam.fy;
        camera["cx"] = cam.cx;
        camera["cy"] = cam.cy;
        camera["k1"] = cam.k1;
        camera["k2"] = cam.k2;
        camera["k3"] = cam.k3;
        camera["k4"] = cam.k4;
        camera["k5"] = cam.k5;
        camera["k6"] = cam.k6;
        camera["p1"] = cam.p1;
        camera["p2"] = cam.p2;

        auto camToWorld = cam.getCamToWorldFromViewMat().cpu();
        torch::Tensor R = camToWorld.index({Slice(None, 3), Slice(None, 3)});
        torch::Tensor T = camToWorld.index({Slice(None, 3), Slice(3,4)}).squeeze();
        
        // Flip z and y
        R = torch::matmul(R, torch::diag(torch::tensor({1.0f, -1.0f, -1.0f})));

        if (keepCrs) T = (T / scale) + translation;

        std::vector<float> position(3);
        std::vector<std::vector<float>> rotation(3, std::vector<float>(3));
        std::vector<std::vector<float>> transform_matrix(4,std::vector<float>(4));
        for (int i = 0; i < 3; i++) {
            position[i] = T[i].item<float>();
            for (int j = 0; j < 3; j++) {
                rotation[i][j] = R[i][j].item<float>();
            }
        }

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                transform_matrix[i][j] = camToWorld[i][j].item<float>();
            }
        }

        camera["position"] = position;
        camera["rotation"] = rotation;
        camera["transform_matrix"] = transform_matrix;
        // camera["camera_angle_x"] = focal2fov(cam.fx, cam.height);
        // camera_angle_x =  focal2fov(cam.fx, cam.height);
        fl_x = cam.fx;
        fl_y = cam.fy;
        j.push_back(camera);
    }
    json obj = json::object();
    // obj["camera_angle_x"] = camera_angle_x;
    // obj["fl_x"] = fl_x;
    // obj["fl_y"] = fl_y;
    
    // Save camera model as string (global)
    if (!cameras.empty()) {
        std::string camera_model_str;
        switch (static_cast<dvs::CameraModelType>(cameras[0].model_type)) {
            case dvs::CameraModelType::PINHOLE: camera_model_str = "PINHOLE"; break;
            case dvs::CameraModelType::ORTHO:   camera_model_str = "ORTHO"; break;
            case dvs::CameraModelType::FISHEYE: camera_model_str = "FISHEYE"; break;
            case dvs::CameraModelType::FTHETA:  camera_model_str = "FTHETA"; break;
            default: camera_model_str = "PINHOLE"; break;
        }
        obj["camera_model"] = camera_model_str;
    }
    
    obj["frames"] = j;

    std::ofstream of(filename);
    of << obj;
    of.close();

    std::cout << "Wrote " << filename << std::endl;
}

PointSet toPointSet(const Points& pt)
{
    PointSet pts;
    pts.points.resize(pt.xyz.size(0));
    pts.colors.resize(pt.rgb.size(0));
    memcpy( pts.points.data(), pt.xyz.cpu().data_ptr<float>(), pt.xyz.size(0) * 3 * sizeof(float));
    memcpy( pts.colors.data(), pt.rgb.cpu().data_ptr<uint8_t>(), pt.rgb.size(0) * 3 * sizeof(uint8_t));
    return pts;
}

void InputData::saveSparsePointSet(const std::string &filename){
    if(points.xyz.numel() > 0 ) {
        auto pt = toPointSet(points);
        savePointSet(pt,filename);
    }
}

Camera::Camera(int width, int height, float fx, float fy, float cx, float cy,
    float k1, float k2, float k3, float p1, float p2,
    const torch::Tensor& worldToCam, const std::string& filePath) :
    width(width), height(height), fx(fx), fy(fy), cx(cx), cy(cy),
    k1(k1), k2(k2), k3(k3), p1(p1), p2(p2),
    worldToCam(worldToCam), filePath(filePath) 
{
}

void Camera::extractCameraPosRotation(const torch::Tensor& camToWorld) {
    T = camToWorld.index({ Slice(None, 3), Slice(3,4) }).clone();
    auto ptr = T.cpu().contiguous().data_ptr<float>();
    cameraPos = { ptr[0], ptr[1], ptr[2] };
    torch::Tensor R = camToWorld.index({ Slice(None, 3), Slice(None, 3) }).clone();
    R.index_put_({ Slice(0, 3), Slice(1,3) }, R.index({ Slice(0, 3), Slice(1,3) }) * -1.0f);
    ptr = R.contiguous().data_ptr<float>();
    rotation = glm::mat3{ 
        ptr[0], ptr[1], ptr[2],
        ptr[3], ptr[4], ptr[5],
        ptr[6], ptr[7], ptr[8]
    };
    rotation = glm::transpose(rotation);

    if(radial_distortion.defined()) radial_distortion = radial_distortion.to(device);
    if(tangential_distortion.defined()) tangential_distortion = tangential_distortion.to(device);
}

glm::vec3 Camera::getCameraPos() const{
    return cameraPos;
}

glm::mat3 Camera::getCameraRotation() const{
   return rotation;
}

glm::quat Camera::getRotationQuat() const
{
    return glm::quat_cast(rotation);
}

glm::mat4 Camera::getProjMat() const{
    auto fovx = focal2fov(fx, width);
    auto fovy = focal2fov(fy, height);
    // const auto reversed = false;
    // if(reversed)
    //     return glm::transpose(getProjectionMatrix(0.1, 0.01, fovx, fovy));
    return glm::transpose(::getProjectionMatrix(0.01, 0.1, fovx, fovy));
}

torch::Tensor Camera::getViewMatTensor(const torch::Tensor& camToWorld)
{
    torch::Tensor R = camToWorld.index({ Slice(None, 3), Slice(None, 3) });
    torch::Tensor T = camToWorld.index({ Slice(None, 3), Slice(3,4) });
    // Flip the z and y axes to align with gsplat conventions
    R = torch::matmul(R, torch::diag(torch::tensor({ 1.0f, -1.0f, -1.0f }, R.device())));
    // worldToCam
    torch::Tensor Rinv = R.transpose(0, 1);
    torch::Tensor Tinv = torch::matmul(-Rinv, T);
    torch::Tensor viewMat = torch::eye(4, device);
    viewMat.index_put_({ Slice(None, 3), Slice(None, 3) }, Rinv);
    viewMat.index_put_({ Slice(None, 3), Slice(3, 4) }, Tinv);
    return viewMat;
}

torch::Tensor Camera::getCamToWorldFromViewMat()
{
    // Extract rotation and translation from view matrix
    torch::Tensor Rinv = worldToCam.index({ Slice(None, 3), Slice(None, 3) });
    torch::Tensor Tinv = worldToCam.index({ Slice(None, 3), Slice(3, 4) });
    
    // Get the inverse rotation (transpose for orthogonal matrices)
    torch::Tensor R = Rinv.transpose(0, 1);
    
    // Get the inverse translation: T = -R * Tinv
    torch::Tensor T = torch::matmul(-R, Tinv);
    
    // Flip the z and y axes back to align with original conventions
    // This is the inverse of the flip in getViewMatTensor()
    R = torch::matmul(R, torch::diag(torch::tensor({ 1.0f, -1.0f, -1.0f }, R.device())));
    
    // Construct camToWorld matrix
    torch::Tensor camToWorld = torch::eye(4, device);
    camToWorld.index_put_({ Slice(None, 3), Slice(None, 3) }, R);
    camToWorld.index_put_({ Slice(None, 3), Slice(3, 4) }, T);
    
    return camToWorld;
}