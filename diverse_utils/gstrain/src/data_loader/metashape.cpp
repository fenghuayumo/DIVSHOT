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
#include <pugixml.hpp>


namespace fs = std::filesystem;

using json = nlohmann::json;
using namespace torch::indexing;

// Helper struct for async point cloud loading
struct PointCloudData {
    torch::Tensor xyz;
    torch::Tensor rgb;
    bool loaded = false;
};
namespace metashape {
    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        size_t start = 0;
        size_t end = str.find(delimiter);
        while (end != std::string::npos) {
            tokens.push_back(str.substr(start, end - start));
            start = end + 1;
            end = str.find(delimiter, start);
        }
        tokens.push_back(str.substr(start));
        return tokens;
    }
    InputData inputDataFromMetaShape(const std::string & imagePath,const std::string& posePath,const std::string& pointCloudPath) {
        InputData ret;
        fs::path nsRoot = fs::path(imagePath).parent_path();
        fs::path transformsPath = posePath.empty() ? nsRoot / "transforms.xml" : fs::path(posePath);
        if (!fs::exists(transformsPath)) throw std::runtime_error(transformsPath.string() + " does not exist");
        auto images_path = fs::path(imagePath);
        std::unordered_map<std::string, std::string> imgPaths;
        for (const auto& entry : std::filesystem::directory_iterator(images_path)) 
            if (entry.is_regular_file()) {
                
                imgPaths[entry.path().filename().replace_extension().string()] = (entry.path().string());
            }
        ns::Transforms t;
        try {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(transformsPath.string().c_str());
        if (!result)
            throw std::runtime_error(transformsPath.string() + " parse error");;
        auto root =  doc.root();
        auto child = root.first_child();
        auto chunk = child.child("chunk");
        auto sensors =  chunk.child("sensors");
        if (sensors.empty()) {
            throw std::runtime_error("No sensors found");
        }
        std::vector<pugi::xml_node> calibrated_sensors;
        for (pugi::xml_node sensor : sensors.children("sensor")) {
            if (sensor.attribute("type").as_string() == "spherical" || sensor.child("calibration")) {
                calibrated_sensors.push_back(sensor);
            }
        }
        // Check if any calibrated sensors were found
        if (calibrated_sensors.empty()) {
            throw std::runtime_error("No calibrated sensor found in Metashape XML");
        }
        // Check if all sensors have the same type
        std::vector<std::string> sensor_types;
        for (const auto& sensor : calibrated_sensors) {
            sensor_types.push_back(sensor.attribute("type").as_string());
        }
        if (std::count(sensor_types.begin(), sensor_types.end(), sensor_types[0]) != sensor_types.size()) {
            throw std::runtime_error(
                "All Metashape sensors do not have the same sensor type. "
                "diverse does not support per-frame camera_model types."
                "Only one camera type can be used: frame, fisheye or spherical (perspective, fisheye or equirectangular)"
            );
        }
        if (sensor_types[0] == "frame") {
            t.cameraModel = "OPENCV";
        }
        else if (sensor_types[0] == "fisheye") {
            t.cameraModel = "OPENCV_FISHEYE";
        }
        else
            throw std::runtime_error("Unsupported Metashape sensor type: " + sensor_types[0]);
        std::unordered_map<std::string,ns::Frame> sensor_dict;
        std::unordered_map<std::string, std::string> sensor_type_dict;
        for (auto sensor : calibrated_sensors) {
            auto resolution = sensor.child("resolution");
            assert(resolution.hash_value());
            ns::Frame frame;
            frame.width = resolution.attribute("width").as_int();
            frame.height = resolution.attribute("height").as_int();
            std::string sensorType = sensor.attribute("type").as_string();
            auto calib = sensor.child("calibration");
            if (calib.hash_value()) {
                auto f = calib.child("f");
                assert(f.hash_value());
                frame.fx  = frame.fy = f.text().as_float();
                frame.cx = calib.child("cx").text().as_float() + frame.width / 2.0f;
                frame.cy = calib.child("cy").text().as_float() + frame.height / 2.0f;
                frame.k1 = calib.child("k1").text().as_float();
                frame.k2 = calib.child("k2").text().as_float();
                frame.k3 = calib.child("k3").text().as_float();
                frame.p1 = calib.child("p1").text().as_float();
                frame.p2 = calib.child("p2").text().as_float();
                // For fisheye cameras, k4 may also be present
                if (sensorType == "fisheye") {
                    frame.k4 = calib.child("k4").text().as_float();
                }
            }
            std::string sensorId = sensor.attribute("id").as_string();
            sensor_dict[sensorId] = frame;
            sensor_type_dict[sensorId] = sensorType;
        }
        auto compoents = chunk.child("components");
        std::unordered_map<std::string, glm::mat4> component_dict;
        if (compoents.hash_value()) {
            for(auto component : compoents.children("component")){
                auto transform = component.child("transform");
                if (transform.hash_value()) {
                    auto rotation = transform.child("rotation");
                    std::array<float,9> r;
                    if (rotation.hash_value()) {
                        auto s = split(rotation.text().as_string(),' ');
                        for (auto i = 0; i < 9; i++) {
                            r[i] = std::atof(s[i].c_str());
                        }
                    }
                    std::array<float,3> t;
                    auto translation = transform.child("translation");
                    if (translation.hash_value()) {
                        auto s = split(translation.text().as_string(), ' ');
                        for (auto i = 0; i < 3; i++) {
                            t[i] = std::atof(s[i].c_str());
                        }
                    }
                    auto scale = transform.child("scale");
                    std::array<float, 3> s;
                    if (scale.hash_value()) {
                        auto sa = split(translation.text().as_string(), ' ');
                        for (auto i = 0; i < 3; i++) {
                            s[i] = std::atof(sa[i].c_str());
                        }
                    }
                    auto id = component.attribute("id");
                    glm::mat4 m(1.0f);
                    for (auto i = 0; i < 3; i++) {
                        for (auto j = 0; j < 3; j++) {
                            if (i < 3 && j < 3) {
                                m[i][j] = r[3 * i + j];
                            }
                        }
                    }
                    for (auto i = 0; i < 3; i++) {
                        m[i][3] = t[i] / s[i];
                    }
                    component_dict[id.as_string()] = m;
                }
            }
        }
        auto cameras = chunk.child("cameras");
        std::vector<std::string> frame_sensor_types;
        for (auto camera : cameras.children()) {
            auto label = camera.attribute("label");
            auto sensor_id = camera.attribute("sensor_id");
            std::string sensorIdStr = sensor_id.as_string();
            ns::Frame frame;
            std::string frameSensorType = "frame";
            if (sensor_dict.find(sensorIdStr) != sensor_dict.end()) {
                frame = sensor_dict[sensorIdStr];
                if (sensor_type_dict.find(sensorIdStr) != sensor_type_dict.end()) {
                    frameSensorType = sensor_type_dict[sensorIdStr];
                }
            }
            frame_sensor_types.push_back(frameSensorType);
            frame.filePath = imgPaths[std::string(label.as_string())];
            auto transform = camera.child("transform");
            glm::mat4 trans(1.0f);
            if (transform.hash_value()) {
                auto s = split(transform.text().as_string(), ' ');
                for (auto i = 0; i < 4; i++) {
                    for(int j=0;j<4;j++)
                        trans[i][j] = std::atof(s[i * 4 + j].c_str());
                }
            }
            auto compoent_id = camera.attribute("component_id");
            glm::mat4 transforMatrix = trans;
            if(component_dict.find(compoent_id.as_string()) != component_dict.end())
                transforMatrix = component_dict[compoent_id.as_string()] * trans;
            auto t0 = transforMatrix[0];
            auto t1 = transforMatrix[1];
            auto t2 = transforMatrix[2];
            transforMatrix[0] = t2;
            transforMatrix[1] = t0;
            transforMatrix[2] = t1;
            for (auto i = 0; i < 4; i++) {
                transforMatrix[i][1] *= -1;
                transforMatrix[i][2] *= -1;
            }
            frame.transformMatrix.resize(4);
            for (auto i = 0; i < 4; i++) {
                frame.transformMatrix[i].resize(4);
                for (auto j = 0; j < 4; j++) {
                    frame.transformMatrix[i][j] = transforMatrix[i][j];
                }
            }
     
            t.frames.push_back(frame);
        }
        }
        catch(...){
            throw std::runtime_error("parse metashape camerapose file error, please check whether the file format is correct");
        }
        t.plyFilePath = pointCloudPath.empty() ? (nsRoot / "sparse_pc.ply").string() : pointCloudPath;
        if (t.plyFilePath.empty()) throw std::runtime_error("ply_file_path is empty");

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
            cam.k4 = f.k4;
            if (f.k4 != 0) {
                // Fisheye uses 4 radial coefficients
                cam.model_type = dvs::CameraModelType::FISHEYE;
                cam.radial_distortion = torch::tensor({ static_cast<float>(f.k1), static_cast<float>(f.k2), static_cast<float>(f.k3), static_cast<float>(f.k4) }, torch::kFloat32);
            }
            else {
                // OpenCV Pinhole needs 6 radial coeffs: k1,k2,k3 (numerator), k4,k5,k6 (denominator)
                cam.model_type = dvs::CameraModelType::PINHOLE;
                if (std::abs(f.k1) >= 0.05 || std::abs(f.k2) >= 0.05 || std::abs(f.k3) >= 0.05) {
                    cam.radial_distortion = torch::tensor({ 
                        static_cast<float>(f.k1), static_cast<float>(f.k2), static_cast<float>(f.k3),
                        0.0f, 0.0f, 0.0f  // denominator coefficients
                    }, torch::kFloat32);
                }
                if (f.p1 != 0.0 || f.p2 != 0.0) {
                    cam.tangential_distortion = torch::tensor({ static_cast<float>(f.p1), static_cast<float>(f.p2) }, torch::kFloat32);
                }
            }
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