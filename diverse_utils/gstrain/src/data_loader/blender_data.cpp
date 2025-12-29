#include <filesystem>
#include <cstdlib>
#include <json/json.hpp>
#include "blender_data.hpp"
#include "point_io.hpp"
#include "cv_utils.hpp"
#include "tensor_math.hpp"

namespace fs = std::filesystem;

using json = nlohmann::json;
using namespace torch::indexing;

namespace blender{

void to_json(json &j, const Frame &f){
    j = json{ {"file_path", f.filePath }, 
                {"transform_matrix", f.transformMatrix },
                
            };
}

void from_json(const json& j, Frame &f){
    j.at("file_path").get_to(f.filePath);
    j.at("transform_matrix").get_to(f.transformMatrix);
}

void to_json(json &j, const Transforms &t){
    if(t.camera_angle_x > 0){
        j = json{ {"camera_angle_x", t.camera_angle_x }, 
                {"frames", t.frames },
                };
    }
    else{
        j = json{ {"fl_x", t.fl_x }, 
                {"fl_y", t.fl_y },
                {"frames", t.frames },
                };
    }
}

void from_json(const json& j, Transforms &t){
    if(j.contains("camera_angle_x")) j.at("camera_angle_x").get_to(t.camera_angle_x);
    if(j.contains("fl_x")) j.at("fl_x").get_to(t.fl_x);
    if(j.contains("fl_y")) j.at("fl_y").get_to(t.fl_y);
    j.at("frames").get_to(t.frames);

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

InputData inputDataFromBlender(const std::string &imagePath,const std::string& posePath,const std::string& pointCloudPath){
    InputData ret;
    fs::path nsRoot = fs::path(imagePath).parent_path();
    fs::path transformsPath = posePath.empty() ? nsRoot / "transforms_train.json" : fs::path(posePath);
    if (!fs::exists(transformsPath)) throw std::runtime_error(transformsPath.string() + " does not exist");

    Transforms t = readTransforms(transformsPath.string());
    t.plyFilePath = pointCloudPath.empty() ? (nsRoot / "sparse_pc.ply").string() : pointCloudPath;
    if (t.plyFilePath.empty()) throw std::runtime_error("ply_file_path is empty");
    int img_width = -1;
    int img_height = -1;
    bool first_image = true;
    for(auto& f : t.frames)
    {
        auto imgPath = nsRoot /  (f.filePath);
        if(!std::filesystem::exists(imgPath)){
            imgPath = imgPath.replace_extension(".jpg");
            if(!std::filesystem::exists(imgPath)){
                imgPath = imgPath.replace_extension(".png");
            }
        }
        f.filePath = imgPath.string();
        if (first_image) {
            auto img = imreadRGB(f.filePath);
            img_width = img.cols;
            img_height = img.rows;
            first_image = false;
        }
        f.width = img_width;
        f.height = img_height;
        if(t.camera_angle_x > 0){
            auto focal_length = 0.5 * f.width / std::tan(0.5f * t.camera_angle_x);
            f.fx = focal_length;
            f.fy = focal_length;
        }
        else{
            f.fx = t.fl_x;
            f.fy = t.fl_y;
        }
        f.cx = f.width / 2.0f;
        f.cy = f.height / 2.0f;
    }
    torch::Tensor unorientedPoses = posesFromTransforms(t);
    // torch::Tensor poses = unorientedPoses.clone();
    auto r = autoScaleAndCenterPoses(unorientedPoses);
    torch::Tensor poses = std::get<0>(r);
    ret.translation = std::get<1>(r);
    ret.scale = std::get<2>(r);

    // aabbScale = [[-1.0, -1.0, -1.0], [1.0, 1.0, 1.0]]
    for (size_t i = 0; i < t.frames.size(); i++){
        Frame f = t.frames[i];

        ret.cameras.emplace_back(Camera(f.width, f.height, 
                            static_cast<float>(f.fx), static_cast<float>(f.fy), 
                            static_cast<float>(f.cx), static_cast<float>(f.cy), 
                            static_cast<float>(f.k1), static_cast<float>(f.k2), static_cast<float>(f.k3), 
                            static_cast<float>(f.p1), static_cast<float>(f.p2),  
                            Camera::getViewMatTensor(poses[i]), f.filePath));
        ret.cameras.back().extractCameraPosRotation(poses[i]);
    }
    if (!std::filesystem::exists(t.plyFilePath)) {
        ret.points.xyz = torch::empty({0, 3}, torch::kFloat32);
        ret.points.rgb = torch::empty({0, 3}, torch::kU8);
        return ret;
    }
    PointSet* pSet = readPointSet((nsRoot / t.plyFilePath).string());
    torch::Tensor points = pSet->pointsTensor().clone();
    
    ret.points.xyz = (points - ret.translation) * ret.scale;
    ret.points.rgb = pSet->colorsTensor().clone();

    RELEASE_POINTSET(pSet);

    return ret;
}

}