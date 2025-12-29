#include <filesystem>
#include <cstdlib>
#include <future>
#include <thread>
#include <json/json.hpp>
#include "nerfstudio.hpp"
#include "point_io.hpp"
#include "cv_utils.hpp"
#include "tensor_math.hpp"

namespace fs = std::filesystem;

using json = nlohmann::json;
using namespace torch::indexing;

// Helper struct for async point cloud loading
struct PointCloudData {
    torch::Tensor xyz;
    torch::Tensor rgb;
    bool loaded = false;
};

namespace ns{

void to_json(json &j, const Frame &f){
    j = json{ {"file_path", f.filePath }, 
                {"w", f.width }, 
                {"h", f.height },
                {"fl_x", f.fx },
                {"fl_y", f.fy },
                {"cx", f.cx },
                {"cy", f.cy },
                {"k1", f.k1 },
                {"k2", f.k2 },
                {"p1", f.p1 },
                {"p2", f.p2 },
                {"k3", f.k3 },
                {"k4", f.k4 },
                {"k5", f.k5 },
                {"k6", f.k6 },
                {"transform_matrix", f.transformMatrix },
                
            };
}

void from_json(const json& j, Frame &f){
    j.at("file_path").get_to(f.filePath);
    j.at("transform_matrix").get_to(f.transformMatrix);
    if (j.contains("w")) j.at("w").get_to(f.width);
    if (j.contains("h")) j.at("h").get_to(f.height);
    if (j.contains("fl_x")) j.at("fl_x").get_to(f.fx);
    if (j.contains("fl_y")) j.at("fl_y").get_to(f.fy);
    if (j.contains("fx")) j.at("fx").get_to(f.fx);
    if (j.contains("fy")) j.at("fy").get_to(f.fy);
    if (j.contains("cx")) j.at("cx").get_to(f.cx);
    if (j.contains("cy")) j.at("cy").get_to(f.cy);
    if (j.contains("k1")) j.at("k1").get_to(f.k1);
    if (j.contains("k2")) j.at("k2").get_to(f.k2);
    if (j.contains("p1")) j.at("p1").get_to(f.p1);
    if (j.contains("p2")) j.at("p2").get_to(f.p2);
    if (j.contains("k3")) j.at("k3").get_to(f.k3);
    if (j.contains("k4")) j.at("k4").get_to(f.k4);
    if (j.contains("k5")) j.at("k5").get_to(f.k5);
    if (j.contains("k6")) j.at("k6").get_to(f.k6);
    
}

void to_json(json &j, const Transforms &t){
    j = json{  
                {"frames", t.frames },
                {"ply_file_path", t.plyFilePath },
            };
}

void from_json(const json& j, Transforms &t){
    // j.at("camera_model").get_to(t.cameraModel);
    j.at("frames").get_to(t.frames);
    if (j.contains("ply_file_path")) j.at("ply_file_path").get_to(t.plyFilePath);

    // Globals
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
    // float camera_angle_x = -1;
    float camera_angle_y = -1;
    if (j.contains("camera_angle_x")) j.at("camera_angle_x").get_to(t.camera_angle_x);
    if (j.contains("camera_angle_y")) j.at("camera_angle_y").get_to(camera_angle_y);
    if (j.contains("w")) j.at("w").get_to(width);
    if (j.contains("h")) j.at("h").get_to(height);
    if (j.contains("fl_x")) j.at("fl_x").get_to(fx);
    if (j.contains("fl_y")) j.at("fl_y").get_to(fy);
    if (j.contains("fx")) j.at("fx").get_to(fx);
    if (j.contains("fy")) j.at("fy").get_to(fy);
    if (j.contains("cx")) j.at("cx").get_to(cx);
    if (j.contains("cy")) j.at("cy").get_to(cy);
    if (j.contains("k1")) j.at("k1").get_to(k1);
    if (j.contains("k2")) j.at("k2").get_to(k2);
    if (j.contains("p1")) j.at("p1").get_to(p1);
    if (j.contains("p2")) j.at("p2").get_to(p2);
    if (j.contains("k3")) j.at("k3").get_to(k3);
    if (j.contains("k4")) j.at("k4").get_to(k4);
    if (j.contains("k5")) j.at("k5").get_to(k5);
    if (j.contains("k6")) j.at("k6").get_to(k6);
    if (j.contains("camera_model")) j.at("camera_model").get_to(t.cameraModel);
    // Assign per-frame intrinsics if missing
    for (Frame &f : t.frames){
        if (!f.width && width) f.width = width;
        if (!f.height && height) f.height = height;
        if (!f.fx && fx) f.fx = fx;
        if (!f.fy && fy) f.fy = fy;
        if (!f.cx && cx) f.cx = cx;
        if (!f.cy && cy) f.cy = cy;
        if (!f.k1 && k1) f.k1 = k1;
        if (!f.k2 && k2) f.k2 = k2;
        if (!f.p1 && p1) f.p1 = p1;
        if (!f.p2 && p2) f.p2 = p2;
        if (!f.k3 && k3) f.k3 = k3;
        if (!f.k4 && k4) f.k4 = k4;
        if (!f.k5 && k5) f.k5 = k5;
        if (!f.k6 && k6) f.k6 = k6;
        if(!f.cx ) f.cx = f.width / 2.0f;
        if(!f.cy ) f.cy = f.height / 2.0f;
        if(!f.fx && t.camera_angle_x > 0){
            auto focal_length = 0.5 * f.width / std::tan(0.5f * t.camera_angle_x);
            f.fx = focal_length;
            f.fy = focal_length;
        }
        else if(!f.fx && camera_angle_y > 0){
            auto focal_length = 0.5 * f.height / std::tan(0.5f * camera_angle_y);
            f.fx = focal_length;
            f.fy = focal_length;
        }
    }

    std::sort(t.frames.begin(), t.frames.end(), 
        [](Frame const &a, Frame const &b) {
            return a.filePath < b.filePath; 
        });
}    

Transforms readTransforms(const std::string &filename){
    std::ifstream f(filename);
    json data = json::parse(f);
    f.close();
    return data.template get<Transforms>();
}

torch::Tensor posesFromTransforms(const Transforms &t){
    torch::Tensor poses = torch::zeros({static_cast<long int>(t.frames.size()), 4, 4}, torch::kFloat32);
    for (size_t c = 0; c < t.frames.size(); c++){
        for (size_t i = 0; i < 4; i++){
            for (size_t j = 0; j < 4; j++){
                poses[c][i][j] = t.frames[c].transformMatrix[i][j];
            }
        }
    }
    return poses;
}

InputData inputDataFromNerfStudio(const std::string &imagePath,const std::string &posePath,const std::string &pointCloudPath){
    InputData ret;
    // fs::path nsRoot = fs::path(imagePath).parent_path();
    fs::path nsRoot = posePath.empty() ? fs::path(imagePath).parent_path() : fs::path(posePath).parent_path();
    fs::path transformsPath = posePath.empty() ? nsRoot / "transforms.json" : fs::path(posePath);
    std::cout << "loading nerfstudio data from " << nsRoot.string() << std::endl;
    if (!fs::exists(transformsPath)) {
        transformsPath = nsRoot / "dvs_cameras.json";
        if(!fs::exists(transformsPath)){
            transformsPath = nsRoot.parent_path() / "dvs_cameras.json";
            if (!fs::exists(transformsPath)) {
                throw std::runtime_error(transformsPath.string() + " does not exist");
            }
            else {
                nsRoot = nsRoot.parent_path();
            }
        }
        else{
            nsRoot = nsRoot.parent_path();
        }
    }

    Transforms t = readTransforms(transformsPath.string());
    t.plyFilePath = pointCloudPath.empty() ? (nsRoot / "sparse_pc.ply").string() : pointCloudPath;
    if (t.plyFilePath.empty()) throw std::runtime_error("ply_file_path is empty");

    // ========================================================================
    // Start ASYNC point cloud loading in parallel (Task 2)
    // ========================================================================
    std::future<PointCloudData> pointCloudFuture = std::async(std::launch::async, [&nsRoot, &t]() {
        PointCloudData result;
        if (std::filesystem::exists(t.plyFilePath)) {
            try {
                PointSet* pSet = readPointSet((nsRoot / t.plyFilePath).string());
                if (pSet) {
                    result.xyz = pSet->pointsTensor().clone();
                    result.rgb = pSet->colorsTensor().clone();
                    result.loaded = true;
                    RELEASE_POINTSET(pSet);
                }
            } catch (...) {
                result.loaded = false;
            }
        }
        return result;
    });

    // ========================================================================
    // Continue with camera loading (Task 1) - runs in parallel with Task 2
    // ========================================================================
    torch::Tensor unorientedPoses = posesFromTransforms(t);

    auto r = autoScaleAndCenterPoses(unorientedPoses);
    torch::Tensor poses = std::get<0>(r);
    ret.translation = std::get<1>(r);
    ret.scale = std::get<2>(r);

    // aabbScale = [[-1.0, -1.0, -1.0], [1.0, 1.0, 1.0]]
    int img_width = -1;
    int img_height = -1;
    bool first_image = true;
    for (size_t i = 0; i < t.frames.size(); i++){
        Frame f = t.frames[i];
        auto imgPath = nsRoot / (f.filePath);
        if (!std::filesystem::exists(imgPath)) {
            imgPath = imgPath.replace_extension(".jpg");
            if (!std::filesystem::exists(imgPath)) {
                imgPath = imgPath.replace_extension(".png");
            }
        }
        f.filePath = imgPath.string();
        if(f.width == 0 || f.height == 0){
            if (first_image) {
                auto img = imreadRGB(f.filePath);
                img_width = img.cols;
                img_height = img.rows;
                first_image = false;
            }
            f.width = img_width;
            f.height = img_height;
            if(!f.fx && t.camera_angle_x > 0){
                auto focal_length = 0.5 * f.width / std::tan(0.5f * t.camera_angle_x);
                f.fx = focal_length;
                f.fy = focal_length;
            }
            f.cx = f.width / 2.0f;
            f.cy = f.height / 2.0f;
        }

        auto worldToCam = Camera::getViewMatTensor(poses[i]);
        ret.cameras.emplace_back(Camera(f.width, f.height, 
                            static_cast<float>(f.fx), static_cast<float>(f.fy), 
                            static_cast<float>(f.cx), static_cast<float>(f.cy), 
                            static_cast<float>(f.k1), static_cast<float>(f.k2), static_cast<float>(f.k3), 
                            static_cast<float>(f.p1), static_cast<float>(f.p2),  
                            worldToCam, f.filePath));
        
        // Set additional distortion parameters and camera model type
        Camera& cam = ret.cameras.back();
        cam.k4 = static_cast<float>(f.k4);
        cam.k5 = static_cast<float>(f.k5);
        cam.k6 = static_cast<float>(f.k6);
        
        // Parse camera model string to enum
        if (t.cameraModel == "ORTHO") {
            cam.model_type = static_cast<uint8_t>(dvs::CameraModelType::ORTHO);
        } else if (t.cameraModel == "FISHEYE") {
            cam.model_type = static_cast<uint8_t>(dvs::CameraModelType::FISHEYE);
        } else if (t.cameraModel == "FTHETA") {
            cam.model_type = static_cast<uint8_t>(dvs::CameraModelType::FTHETA);
        } else {
            cam.model_type = static_cast<uint8_t>(dvs::CameraModelType::PINHOLE);
        }
        
        cam.extractCameraPosRotation(poses[i]);
    }
    
    // ========================================================================
    // Wait for async point cloud loading to complete (Task 2)
    // ========================================================================
    PointCloudData pointCloudData = pointCloudFuture.get();
    if (pointCloudData.loaded) {
        ret.points.xyz = (pointCloudData.xyz - ret.translation) * ret.scale;
        ret.points.rgb = pointCloudData.rgb;
    } else {
        ret.points.xyz = torch::empty({0, 3}, torch::kFloat32);
        ret.points.rgb = torch::empty({0, 3}, torch::kU8);
    }

    return ret;
}

}