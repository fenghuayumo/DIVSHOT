#ifndef BLENDER_DATA_H
#define BLENDER_DATA_H

#include <iostream>
#include <string>
#include <fstream>
#include <torch/torch.h>
#include <json/json.hpp>
#include "input_data.hpp"

using json = nlohmann::json;

namespace blender{
    typedef std::vector<std::vector<float>> Mat4;

    struct Frame{
        std::string filePath = "";
        int width = 0;
        int height = 0;
        double fx = 0;
        double fy = 0;
        double cx = 0;
        double cy = 0;
        double k1 = 0;
        double k2 = 0;
        double p1 = 0;
        double p2 = 0;
        double k3 = 0;
        Mat4 transformMatrix;
    };
    void to_json(json &j, const Frame &f);
    void from_json(const json& j, Frame &f);

    struct Transforms{
        float camera_angle_x = -1;
        float fl_x = -1;
        float fl_y = -1;
        std::vector<Frame> frames;
        std::string plyFilePath;
    };
    void to_json(json &j, const Transforms &t);
    void from_json(const json& j, Transforms &t);

    Transforms readTransforms(const std::string &filename);
    torch::Tensor posesFromTransforms(const Transforms &t);

    InputData inputDataFromBlender(const std::string &imagePath,const std::string& posePath,const std::string& pointCloudPath);
}   



#endif