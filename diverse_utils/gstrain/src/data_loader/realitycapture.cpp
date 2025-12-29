#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <future>
#include <thread>
#include <json/json.hpp>
#include <torch/torch.h>
#include "point_io.hpp"
#include "cv_utils.hpp"
#include "tensor_math.hpp"
#include "nerfstudio.hpp"
#include <rapidcsv/rapidcsv.h>

namespace fs = std::filesystem;

using json = nlohmann::json;
using namespace torch::indexing;

// Helper struct for async point cloud loading
struct PointCloudData {
    torch::Tensor xyz;
    torch::Tensor rgb;
    bool loaded = false;
};

namespace realitycapture{
    
    glm::mat3 getRotationMatrix(float yaw, float pitch, float roll) {
        auto s_yaw = glm::sin(glm::radians(yaw));
        auto c_yaw = glm::cos(glm::radians(yaw));
        auto s_pitch = glm::sin(glm::radians(pitch));
        auto c_pitch = glm::cos(glm::radians(pitch));
        auto s_roll = glm::sin(glm::radians(roll));
        auto c_roll = glm::cos(glm::radians(roll));
        glm::mat3 rot_x(
            1.0f, 0.0f,    0.0f,
            0.0f, c_pitch, -s_pitch,
            0.0f, s_pitch, c_pitch
        );
        rot_x = glm::transpose(rot_x);
        
        glm::mat3 rot_y(
            c_roll,  0.0f, s_roll,
            0.0f,    1.0f, 0.0f,
            -s_roll, 0.0f, c_roll
        );
        rot_y = glm::transpose(rot_y);
        
        glm::mat3 rot_z(
            c_yaw, -s_yaw, 0.0f,
            s_yaw,  c_yaw, 0.0f,
            0.0f,   0.0f,  1.0f
        );
        rot_z = glm::transpose(rot_z);

        return rot_z * rot_x * rot_y;
    }

    InputData inputDataFromRealityCapture(const std::string & imagePath,const std::string& posePath,const std::string& pointCloudPath){
        InputData ret;
        fs::path nsRoot = fs::path(imagePath).parent_path();
        fs::path transformsPath = posePath.empty() ? nsRoot / "transforms.csv" : fs::path(posePath);
        std::cout << "loading realitycapture data from " << nsRoot.string() << std::endl;
        if (!fs::exists(transformsPath)) throw std::runtime_error(transformsPath.string() + " does not exist");

        ns::Transforms  t;
        try {
            rapidcsv::Document doc(transformsPath.string());
            std::unordered_map<std::string, std::vector<float>> name_cameras;
            for (auto row = 0; row < doc.GetRowCount(); row++) {
                auto rows = doc.GetRow<std::string>(row);
                std::string rowName = rows[0];
                if (rowName.find("#name") != std::string::npos) 
                    continue;
                //name_cameras[rowName] = doc.GetRow<float>(row);
                for (int col = 1; col < 16; col++)
                    name_cameras[rowName].push_back(std::atof(rows[col].c_str()));
            }
            auto imgPath = imagePath + "/";
            //t.frames.resize(name_cameras.size());
            t.cameraModel = "OPENCV";
            
            // Optimization: Only read first image to get dimensions, assuming all images have the same size
            int img_width = -1;
            int img_height = -1;
            bool first_image = true;
            
            for(const auto& c : name_cameras){
                // Only read image dimensions on first iteration
                if (first_image) {
                    auto img = imreadRGB(imgPath + c.first);
                    img_width = img.cols;
                    img_height = img.rows;
                    first_image = false;
                }

                ns::Frame frame;
                frame.width = img_width;
                frame.height = img_height;
                auto scale = std::max(frame.width, frame.height);
                frame.filePath = imgPath + c.first;
                frame.fx = c.second[6] * scale / 36.0f;
                frame.fy = c.second[6] * scale / 36.0f;
                frame.cx = c.second[7] * scale + frame.width / 2.0f;
                frame.cy = c.second[8] * scale + frame.height / 2.0f;
                frame.k1 = c.second[9];
                frame.k2 = c.second[10];
                frame.k3 = c.second[11];
                frame.k4 = c.second[12];
                frame.p1 =  c.second[13];
                frame.p2 = c.second[14];

                auto rot = getRotationMatrix(-c.second[3],c.second[4],c.second[5]);
                glm::mat4 transform(1.0f);
                transform = rot;
                transform[3] = glm::vec4(c.second[0],c.second[1],c.second[2],1);
                frame.transformMatrix.resize(4);
                for (auto i = 0; i < 4; i++) {
                    frame.transformMatrix[i].resize(4);
                    for (auto j = 0; j < 4; j++) {
                        frame.transformMatrix[i][j] = transform[j][i];
                    }
                }
                t.frames.push_back(frame);
            }
        }
        catch(...){
            throw std::runtime_error("parse realitycapture camerapos file error, please check whether the file format is correct");
        }
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

        for (size_t i = 0; i < t.frames.size(); i++){
            ns::Frame f = t.frames[i];

            ret.cameras.emplace_back(Camera(f.width, f.height, 
                                static_cast<float>(f.fx), static_cast<float>(f.fy), 
                                static_cast<float>(f.cx), static_cast<float>(f.cy), 
                                static_cast<float>(f.k1), static_cast<float>(f.k2), static_cast<float>(f.k3), 
                                static_cast<float>(f.p1), static_cast<float>(f.p2),  
                                Camera::getViewMatTensor(poses[i]), (nsRoot / f.filePath).string()));
                                  
            // Set distortion parameters and camera model type
            Camera& cam = ret.cameras.back();
            cam.model_type = dvs::CameraModelType::PINHOLE;
            
            // OpenCV Pinhole model uses 6 radial coefficients:
            // icD = (1 + k1*r² + k2*r⁴ + k3*r⁶) / (1 + k4*r² + k5*r⁴ + k6*r⁶)
            // RealityCapture k1,k2,k3 map to numerator (OpenCV k1,k2,k3)
            // Denominator coefficients (k4,k5,k6) are set to 0
            if ( std::abs(f.k1) >= 0.02 || std::abs(f.k2) >= 0.02 || std::abs(f.k3) >= 0.02 || std::abs(f.k4) >= 0.01) {
                cam.radial_distortion = torch::tensor({
                    static_cast<float>(f.k1), 
                    static_cast<float>(f.k2), 
                    static_cast<float>(f.k3),
                    static_cast<float>(f.k4), 0.0f, 0.0f  // denominator coefficients (k4, k5, k6)
                }, torch::kFloat32);
            }
            if (std::abs(f.p1) >= 0.001 || std::abs(f.p2) >= 0.001) {
                cam.tangential_distortion = torch::tensor({static_cast<float>(f.p1), static_cast<float>(f.p2)}, torch::kFloat32);
            }
            ret.cameras.back().extractCameraPosRotation(poses[i]);
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