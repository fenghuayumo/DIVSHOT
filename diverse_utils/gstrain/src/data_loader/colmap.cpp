#include <filesystem>
#include <future>
#include <thread>
#include "colmap.hpp"
#include "point_io.hpp"
#include "tensor_math.hpp"
#include <utility/file_utils.h>
#include <utility/string_utils.h>
namespace fs = std::filesystem;
using namespace torch::indexing;

// Helper struct for async point cloud loading
struct PointCloudData {
    torch::Tensor xyz;
    torch::Tensor rgb;
    bool loaded = false;
};

namespace cm{

    auto str2CameraModel(const std::string& cam_id)-> CameraModel
    {
        if (cam_id == "SIMPLE_PINHOLE") {
            return CameraModel::SimplePinhole;
        }
        else if (cam_id == "PINHOLE") {
            return CameraModel::Pinhole;
        }
        else if (cam_id == "SIMPLE_RADIAL") {
            return CameraModel::SimpleRadial;
        }
        else if (cam_id == "OPENCV") {
            return CameraModel::OpenCV;
        }
        else if (cam_id == "OPENCV_FISHEYE") {
            return CameraModel::OpenCVFisheye;
        }
        else if (cam_id == "FULL_OPENCV") {
            return CameraModel::FullOpenCV;
        }
        else if (cam_id == "RadialFisheye") {
            return CameraModel::RadialFisheye;
        }
        else if (cam_id == "ThinPrismFisheye") {
            return CameraModel::ThinPrismFisheye;
        }else if (cam_id == "Radial"){
            return CameraModel::Radial;
        }
        return CameraModel::SimplePinhole;
    }
InputData inputDataFromColmapText(const std::string& imagePath, const std::string& posePath, const std::string& pointCloudPath) {
    InputData ret;
    auto projectRoot = fs::path(imagePath).parent_path();
    fs::path cmRoot(projectRoot);

    if (!fs::exists(cmRoot / "cameras.txt") && fs::exists(cmRoot / "sparse" / "0" / "cameras.txt")) {
        cmRoot = cmRoot / "sparse" / "0";
    }
    else if (fs::exists(cmRoot / "sparse" / "cameras.txt"))
        cmRoot = cmRoot / "sparse";
    fs::path camerasPath = posePath.empty() ? cmRoot / "cameras.txt" : fs::path(posePath);
    fs::path imagesPath = cmRoot / "images.txt";
    fs::path pointsPath = pointCloudPath.empty() ? cmRoot / "points3D.txt" : fs::path(pointCloudPath);

    if (!fs::exists(camerasPath)) throw std::runtime_error(camerasPath.string() + " does not exist");
    if (!fs::exists(imagesPath)) throw std::runtime_error(imagesPath.string() + " does not exist");
    
    // ========================================================================
    // Start ASYNC point cloud loading in parallel (Task 2)
    // ========================================================================
    std::future<PointCloudData> pointCloudFuture = std::async(std::launch::async, [pointsPath]() {
        PointCloudData result;
        if (fs::exists(pointsPath)) {
            try {
                PointSet* pSet = readPointSet(pointsPath.string());
                if (pSet) {
                    result.xyz = pSet->pointsTensor().clone();
                    result.rgb = pSet->colorsTensor().clone();
                    result.loaded = true;
                    RELEASE_POINTSET(pSet);
                }
            } catch (...) {
                // Point cloud loading failed, continue without it
                result.loaded = false;
            }
        }
        return result;
    });
    
    // ========================================================================
    // Continue with camera loading (Task 1) - runs in parallel with Task 2
    // ========================================================================
    auto lines = diverse::read_text_file(camerasPath);
    std::unordered_map<uint32_t, Camera*> camMap;
    std::vector<Camera> cameras(lines.size());
    for (auto i = 0;i<cameras.size();i++) {
        const auto& line = lines[i];
        const auto tokens = diverse::stringutility::split_string(line, ' ');
        if (tokens.size() < 4) {
            throw std::runtime_error("Invalid format in cameras.txt: " + line);
        }
        
        Camera* cam = &cameras[i];
        cam->id = std::stoul(tokens[0]);
        CameraModel model = str2CameraModel(tokens[1]); // model ID
        cam->width = std::stoi(tokens[2]);
        cam->height = std::stoi(tokens[3]);
        // Read parameters
        std::vector<double> raw_params;
        for (uint64_t j = 4; j < tokens.size(); ++j) {
            raw_params.push_back(std::stod(tokens[j]));
        }

        if (model == SimplePinhole) {
            cam->fx = raw_params[0];
            cam->fy = cam->fx;
            cam->cx = raw_params[1];
            cam->cy = raw_params[2];
            cam->model_type = dvs::CameraModelType::PINHOLE;
        }
        else if (model == Pinhole) {
            cam->fx = raw_params[0];
            cam->fy = raw_params[1];
            cam->cx = raw_params[2];
            cam->cy = raw_params[3];
            cam->model_type = dvs::CameraModelType::PINHOLE;
        }
        else if (model == SimpleRadial) {
            cam->fx = raw_params[0];
            cam->fy = cam->fx;
            cam->cx = raw_params[1];
            cam->cy = raw_params[2];
            cam->k1 = raw_params[3];
            cam->model_type = dvs::CameraModelType::PINHOLE;
            // OpenCV Pinhole needs 6 radial coeffs: k1,k2,k3 (numerator), k4,k5,k6 (denominator)
            cam->radial_distortion = torch::tensor({cam->k1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, torch::kFloat32);
        }
        else if (model == Radial){
            //f, cx, cy, k1, k2
            cam->fx = raw_params[0];
            cam->fy = cam->fx;
            cam->cx = raw_params[1];
            cam->cy = raw_params[2];
            cam->k1 = raw_params[3];
            cam->k2 = raw_params[4];
            // OpenCV Pinhole needs 6 radial coeffs: k1,k2,k3 (numerator), k4,k5,k6 (denominator)
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, 0.0f, 0.0f, 0.0f, 0.0f}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::PINHOLE;
        }
        else if (model == OpenCV) {
            cam->fx = raw_params[0];
            cam->fy = raw_params[1];
            cam->cx = raw_params[2];
            cam->cy = raw_params[3];
            cam->k1 = raw_params[4];
            cam->k2 = raw_params[5];
            cam->p1 = raw_params[6];
            cam->p2 = raw_params[7];
            // OpenCV Pinhole needs 6 radial coeffs: k1,k2,k3 (numerator), k4,k5,k6 (denominator)
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, 0.0f, 0.0f, 0.0f, 0.0f}, torch::kFloat32);
            cam->tangential_distortion = torch::tensor({cam->p1, cam->p2}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::PINHOLE;
        }
        else if (model == FullOpenCV) {
            // fx, fy, cx, cy, k1, k2, p1, p2, k3, k4, k5, k6
            cam->fx = raw_params[0];
            cam->fy = raw_params[1];
            cam->cx = raw_params[2];
            cam->cy = raw_params[3];
            cam->k1 = raw_params[4];
            cam->k2 = raw_params[5];
            cam->p1 = raw_params[6];
            cam->p2 = raw_params[7];
            cam->k3 = raw_params[8];
            cam->k4 = raw_params[9];
            cam->k5 = raw_params[10];
            cam->k6 = raw_params[11];
            // OpenCV Pinhole needs 6 radial coeffs: k1,k2,k3 (numerator), k4,k5,k6 (denominator)
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, cam->k3, cam->k4, cam->k5, cam->k6}, torch::kFloat32);
            cam->tangential_distortion = torch::tensor({cam->p1, cam->p2}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::PINHOLE;
        }
        else if (model == OpenCVFisheye){
            cam->fx = raw_params[0];
            cam->fy = raw_params[1];
            cam->cx = raw_params[2];
            cam->cy = raw_params[3];
            cam->k1 = raw_params[4];
            cam->k2 = raw_params[5];
            cam->k3 = raw_params[6];
            cam->k4 = raw_params[7];
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, cam->k3, cam->k4}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::FISHEYE;
        }
        else if (model == RadialFisheye){
             // f, cx, cy, k1, k2
            cam->fx = raw_params[0];
            cam->fy = cam->fx;
            cam->cx = raw_params[1];
            cam->cy = raw_params[2];
            cam->k1 = raw_params[3];
            cam->k2 = raw_params[4];
            // OpenCV Fisheye needs 4 radial coeffs: k1, k2, k3, k4
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, 0.0f, 0.0f}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::FISHEYE;
        }
        else {
            throw std::runtime_error("Unsupported camera model: " + std::to_string(model));
        }
        camMap[cam->id] = cam;
    }
    auto img_lines = diverse::read_text_file(imagesPath);
    uint64_t numImages = img_lines.size() / 2;
    torch::Tensor unorientedPoses = torch::zeros({ static_cast<long int>(numImages), 4, 4 }, torch::kFloat32);
    for (uint64_t i = 0; i < numImages; ++i) {
        const auto& line = img_lines[i * 2];
        const auto tokens = diverse::stringutility::split_string(line, ' ');
        if (tokens.size() != 10) {
            throw std::runtime_error("Invalid format in images.txt line " + std::to_string(i * 2 + 1));
        }
        torch::Tensor qVec = torch::tensor({
             std::stof(tokens[1]), std::stof(tokens[2]),
             std::stof(tokens[3]), std::stof(tokens[4])
            }, torch::kFloat32);
        torch::Tensor R = quatToRotMat(qVec);
        torch::Tensor T = torch::tensor({
            { std::stof(tokens[5]) },
            { std::stof(tokens[6]) },
            { std::stof(tokens[7]) }
            }, torch::kFloat32);

        torch::Tensor Rinv = R.transpose(0, 1);
        torch::Tensor Tinv = torch::matmul(-Rinv, T);

        uint32_t camId = std::stoul(tokens[8]);

        Camera& cam = *camMap[camId];

        // TODO: should "images" be an option?
        cam.filePath = (fs::path(imagePath) / tokens[9]).string();

        unorientedPoses[i].index_put_({ Slice(None, 3), Slice(None, 3) }, Rinv);
        unorientedPoses[i].index_put_({ Slice(None, 3), Slice(3, 4) }, Tinv);
        unorientedPoses[i][3][3] = 1.0f;

        // Convert COLMAP's camera CRS (OpenCV) to OpenGL
        unorientedPoses[i].index_put_({ Slice(0, 3), Slice(1,3) }, unorientedPoses[i].index({ Slice(0, 3), Slice(1,3) }) * -1.0f);
        ret.cameras.push_back(cam);
    }
    auto r = autoScaleAndCenterPoses(unorientedPoses);
    torch::Tensor poses = std::get<0>(r);
    ret.translation = std::get<1>(r);
    ret.scale = std::get<2>(r);

    for (size_t i = 0; i < ret.cameras.size(); i++) {
        ret.cameras[i].worldToCam = Camera::getViewMatTensor(poses[i]);
        ret.cameras[i].extractCameraPosRotation(poses[i]);
    }
    // ========================================================================
    // Wait for async point cloud loading to complete (Task 2)
    // ========================================================================
    PointCloudData pointCloudData = pointCloudFuture.get();
    if (pointCloudData.loaded) {
        ret.points.xyz = (pointCloudData.xyz - ret.translation) * ret.scale;
        ret.points.rgb = pointCloudData.rgb;
    }
    
    return ret;
}

InputData inputDataFromColmap(const std::string &imagePath, const std::string& posePath, const std::string& pointCloudPath){
    InputData ret;
    auto projectRoot = fs::path(imagePath).parent_path();
    fs::path cmRoot(projectRoot);
    std::cout << "loading colmap data from " << cmRoot.string() << std::endl;
    if (!fs::exists(cmRoot / "cameras.bin") && fs::exists(cmRoot / "sparse" / "0" / "cameras.bin")){
        cmRoot = cmRoot / "sparse" / "0";
    }else if(fs::exists(cmRoot / "sparse" / "cameras.bin"))
        cmRoot = cmRoot / "sparse";
    else if(fs::exists(cmRoot / "sparse" / "0" / "cameras.txt") || fs::exists(cmRoot / "sparse" / "cameras.txt"))
        return inputDataFromColmapText(imagePath, posePath, pointCloudPath);
    fs::path camerasPath = posePath.empty() ? cmRoot / "cameras.bin" : fs::path(posePath);
    fs::path imagesPath = cmRoot / "images.bin";
    fs::path pointsPath = pointCloudPath.empty() ? cmRoot / "points3D.bin" : fs::path(pointCloudPath);
    
    if (!fs::exists(camerasPath)) throw std::runtime_error(camerasPath.string() + " does not exist");
    if (!fs::exists(imagesPath)) throw std::runtime_error(imagesPath.string() + " does not exist");
    
    // ========================================================================
    // Start ASYNC point cloud loading in parallel (Task 2)
    // ========================================================================
    std::future<PointCloudData> pointCloudFuture = std::async(std::launch::async, [pointsPath]() {
        PointCloudData result;
        if (fs::exists(pointsPath)) {
            try {
                PointSet* pSet = readPointSet(pointsPath.string());
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
    std::ifstream camf(camerasPath.string(), std::ios::binary);
    if (!camf.is_open()) throw std::runtime_error("Cannot open " + camerasPath.string());
    std::ifstream imgf(imagesPath.string(), std::ios::binary);
    if (!imgf.is_open()) throw std::runtime_error("Cannot open " + imagesPath.string());
    
    size_t numCameras = readBinary<uint64_t>(camf);
    std::vector<Camera> cameras(numCameras);

    std::unordered_map<uint32_t, Camera *> camMap;
    
    for (size_t i = 0; i < numCameras; i++) {
        Camera *cam = &cameras[i];

        cam->id = readBinary<uint32_t>(camf);

        CameraModel model = static_cast<CameraModel>(readBinary<int>(camf)); // model ID
        cam->width = readBinary<uint64_t>(camf);
        cam->height = readBinary<uint64_t>(camf);
        
        if (model == SimplePinhole){
            cam->fx = readBinary<double>(camf);
            cam->fy = cam->fx;
            cam->cx = readBinary<double>(camf);
            cam->cy = readBinary<double>(camf);
        }else if (model == Pinhole){
            cam->fx = readBinary<double>(camf);
            cam->fy = readBinary<double>(camf);
            cam->cx = readBinary<double>(camf);
            cam->cy = readBinary<double>(camf);
        }else if (model == SimpleRadial){
            cam->fx = readBinary<double>(camf);
            cam->fy = cam->fx;
            cam->cx = readBinary<double>(camf);
            cam->cy = readBinary<double>(camf);
            cam->k1 = readBinary<double>(camf);
            cam->model_type = dvs::CameraModelType::PINHOLE;
            // OpenCV Pinhole needs 6 radial coeffs
            cam->radial_distortion = torch::tensor({cam->k1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, torch::kFloat32);
        }else if (model == Radial){
            cam->fx = readBinary<double>(camf);
            cam->fy = cam->fx;
            cam->cx = readBinary<double>(camf);
            cam->cy = readBinary<double>(camf);
            cam->k1 = readBinary<double>(camf);
            cam->k2 = readBinary<double>(camf);
            cam->model_type = dvs::CameraModelType::PINHOLE;
            // OpenCV Pinhole needs 6 radial coeffs
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, 0.0f, 0.0f, 0.0f, 0.0f}, torch::kFloat32);
        }
        else if (model == OpenCV){
            cam->fx = readBinary<double>(camf);
            cam->fy = readBinary<double>(camf);
            cam->cx = readBinary<double>(camf);
            cam->cy = readBinary<double>(camf);
            cam->k1 = readBinary<double>(camf);
            cam->k2 = readBinary<double>(camf);
            cam->p1 = readBinary<double>(camf);
            cam->p2 = readBinary<double>(camf);
            cam->model_type = dvs::CameraModelType::PINHOLE;
            // OpenCV Pinhole needs 6 radial coeffs
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, 0.0f, 0.0f, 0.0f, 0.0f}, torch::kFloat32);
            cam->tangential_distortion = torch::tensor({cam->p1, cam->p2}, torch::kFloat32);
        }
        else if (model == FullOpenCV) {
            // fx, fy, cx, cy, k1, k2, p1, p2, k3, k4, k5, k6
            cam->fx = readBinary<double>(camf);
            cam->fy = readBinary<double>(camf);
            cam->cx = readBinary<double>(camf);
            cam->cy = readBinary<double>(camf);
            cam->k1 = readBinary<double>(camf);
            cam->k2 = readBinary<double>(camf);
            cam->p1 = readBinary<double>(camf);
            cam->p2 = readBinary<double>(camf);
            cam->k3 = readBinary<double>(camf);
            cam->k4 = readBinary<double>(camf);
            cam->k5 = readBinary<double>(camf);
            cam->k6 = readBinary<double>(camf);
            cam->model_type = dvs::CameraModelType::PINHOLE;
            // OpenCV Pinhole needs 6 radial coeffs: k1,k2,k3 (numerator), k4,k5,k6 (denominator)
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, cam->k3, cam->k4, cam->k5, cam->k6}, torch::kFloat32);
            cam->tangential_distortion = torch::tensor({cam->p1, cam->p2}, torch::kFloat32);
        }
        else if (model == OpenCVFisheye){
            cam->fx = readBinary<double>(camf);
            cam->fy = readBinary<double>(camf);
            cam->cx = readBinary<double>(camf);
            cam->cy = readBinary<double>(camf);
            cam->k1 = readBinary<double>(camf);
            cam->k2 = readBinary<double>(camf);
            cam->k3 = readBinary<double>(camf);
            cam->k4 = readBinary<double>(camf);
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, cam->k3, cam->k4}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::FISHEYE;
        }
        else if (model == RadialFisheye){
             // f, cx, cy, k1, k2
            cam->fx = readBinary<double>(camf);
            cam->fy = cam->fx;
            cam->cx = readBinary<double>(camf);
            cam->cy = readBinary<double>(camf);
            cam->k1 = readBinary<double>(camf);
            cam->k2 = readBinary<double>(camf);
            // OpenCV Fisheye needs 4 radial coeffs: k1, k2, k3, k4
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, 0.0f, 0.0f}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::FISHEYE;
        }
        else{
            throw std::runtime_error("Unsupported camera model: " + std::to_string(model));
        }
        camMap[cam->id] = cam;
    }

    camf.close();


    size_t numImages = readBinary<uint64_t>(imgf);
    torch::Tensor unorientedPoses = torch::zeros({static_cast<long int>(numImages), 4, 4}, torch::kFloat32);

    for (size_t i = 0; i < numImages; i++){
        readBinary<uint32_t>(imgf); // imageId
        
        torch::Tensor qVec = torch::tensor({
            readBinary<double>(imgf),
            readBinary<double>(imgf),
            readBinary<double>(imgf),
            readBinary<double>(imgf)
        }, torch::kFloat32);
        torch::Tensor R = quatToRotMat(qVec);
        torch::Tensor T = torch::tensor({
            { readBinary<double>(imgf) },
            { readBinary<double>(imgf) },
            { readBinary<double>(imgf) }
        }, torch::kFloat32);

        torch::Tensor Rinv = R.transpose(0, 1);
        torch::Tensor Tinv = torch::matmul(-Rinv, T);

        uint32_t camId = readBinary<uint32_t>(imgf);

        Camera& cam = *camMap[camId];

        char ch = '\0';
        std::string filePath = "";
        while(true){
            imgf.read(&ch, 1);
            if (ch == '\0') break;
            filePath += ch;
        }

        // TODO: should "images" be an option?
        cam.filePath = (fs::path(imagePath) / filePath).string();

        unorientedPoses[i].index_put_({Slice(None, 3), Slice(None, 3)}, Rinv);
        unorientedPoses[i].index_put_({Slice(None, 3), Slice(3, 4)}, Tinv);
        unorientedPoses[i][3][3] = 1.0f;

        // Convert COLMAP's camera CRS (OpenCV) to OpenGL
        unorientedPoses[i].index_put_({Slice(0, 3), Slice(1,3)}, unorientedPoses[i].index({Slice(0, 3), Slice(1,3)}) * -1.0f);

        size_t numPoints2D = readBinary<uint64_t>(imgf);
        for (size_t j = 0; j < numPoints2D; j++){
            readBinary<double>(imgf); // x
            readBinary<double>(imgf); // y
            readBinary<uint64_t>(imgf); // point3D ID
        }

        ret.cameras.push_back(cam);
    }

    imgf.close();

    auto r = autoScaleAndCenterPoses(unorientedPoses);
    torch::Tensor poses = std::get<0>(r);
    ret.translation = std::get<1>(r);
    ret.scale = std::get<2>(r);

    for (size_t i = 0; i < ret.cameras.size(); i++){
        ret.cameras[i].worldToCam = Camera::getViewMatTensor(poses[i]);
        ret.cameras[i].extractCameraPosRotation(poses[i]);
    }
    // ========================================================================
    // Wait for async point cloud loading to complete (Task 2)
    // ========================================================================
    PointCloudData pointCloudData = pointCloudFuture.get();
    if (pointCloudData.loaded) {
        ret.points.xyz = (pointCloudData.xyz - ret.translation) * ret.scale;
        ret.points.rgb = pointCloudData.rgb;
    }

    return ret;
}

}

InputData inputDataFromColmapCameraPoints(const std::string& imagePath, std::vector<
    colmap::CameraTrack>&& cameraTracks, 
    std::vector<colmap::SparsePoint>&& cpoints, 
    std::vector<colmap::ImageTrack>&& imgfs) {
    using namespace cm;
    InputData ret;

    const auto numCameras = cameraTracks.size();
    std::vector<Camera> cameras(numCameras);

    std::unordered_map<uint32_t, Camera*> camMap;
    
    for (auto i = 0; i < numCameras; i++) {
        auto cameraTrack = cameraTracks[i];
        Camera* cam = &cameras[i];
        cam->id = cameraTrack.camera_id;

        cm::CameraModel model = static_cast<cm::CameraModel>(cameraTrack.model_id); // model ID
        cam->width = cameraTrack.width;
        cam->height = cameraTrack.height;

        if (model == SimplePinhole) {
            cam->fx = cameraTrack.params[0];
            cam->fy = cam->fx;
            cam->cx = cameraTrack.params[1];
            cam->cy = cameraTrack.params[2];
            cam->model_type = dvs::CameraModelType::PINHOLE;
        }
        else if (model == Pinhole) {
            cam->fx = cameraTrack.params[0];
            cam->fy = cameraTrack.params[1];
            cam->cx = cameraTrack.params[2];
            cam->cy = cameraTrack.params[3];
            cam->model_type = dvs::CameraModelType::PINHOLE;
        }
        else if (model == SimpleRadial) {
            cam->fx = cameraTrack.params[0];
            cam->fy = cam->fx;
            cam->cx = cameraTrack.params[1];
            cam->cy = cameraTrack.params[2];
            cam->k1 = cameraTrack.params[3];
            // OpenCV Pinhole needs 6 radial coeffs
            cam->radial_distortion = torch::tensor({cam->k1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::PINHOLE;
        }
        else if (model == Radial) {
            // f, cx, cy, k1, k2
            cam->fx = cameraTrack.params[0];
            cam->fy = cam->fx;
            cam->cx = cameraTrack.params[1];
            cam->cy = cameraTrack.params[2];
            cam->k1 = cameraTrack.params[3];
            cam->k2 = cameraTrack.params[4];
            // OpenCV Pinhole needs 6 radial coeffs
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, 0.0f, 0.0f, 0.0f, 0.0f}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::PINHOLE;
        }
        else if (model == OpenCV) {
            cam->fx = cameraTrack.params[0];
            cam->fy = cameraTrack.params[1];
            cam->cx = cameraTrack.params[2];
            cam->cy = cameraTrack.params[3];
            cam->k1 = cameraTrack.params[4];
            cam->k2 = cameraTrack.params[5];
            cam->p1 = cameraTrack.params[6];
            cam->p2 = cameraTrack.params[7];
            // OpenCV Pinhole needs 6 radial coeffs
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, 0.0f, 0.0f, 0.0f, 0.0f}, torch::kFloat32);
            cam->tangential_distortion = torch::tensor({cam->p1, cam->p2}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::PINHOLE;
        }
        else if (model == FullOpenCV) {
            // fx, fy, cx, cy, k1, k2, p1, p2, k3, k4, k5, k6
            cam->fx = cameraTrack.params[0];
            cam->fy = cameraTrack.params[1];
            cam->cx = cameraTrack.params[2];
            cam->cy = cameraTrack.params[3];
            cam->k1 = cameraTrack.params[4];
            cam->k2 = cameraTrack.params[5];
            cam->p1 = cameraTrack.params[6];
            cam->p2 = cameraTrack.params[7];
            cam->k3 = cameraTrack.params[8];
            cam->k4 = cameraTrack.params[9];
            cam->k5 = cameraTrack.params[10];
            cam->k6 = cameraTrack.params[11];
            cam->model_type = dvs::CameraModelType::PINHOLE;
            // OpenCV Pinhole needs 6 radial coeffs: k1,k2,k3 (numerator), k4,k5,k6 (denominator)
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, cam->k3, cam->k4, cam->k5, cam->k6}, torch::kFloat32);
            cam->tangential_distortion = torch::tensor({cam->p1, cam->p2}, torch::kFloat32);
        }
        else if (model == OpenCVFisheye) {
            cam->fx = cameraTrack.params[0];
            cam->fy = cameraTrack.params[1];
            cam->cx = cameraTrack.params[2];
            cam->cy = cameraTrack.params[3];
            cam->k1 = cameraTrack.params[4];
            cam->k2 = cameraTrack.params[5];
            cam->k3 = cameraTrack.params[6];
            cam->k4 = cameraTrack.params[7];
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, cam->k3, cam->k4}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::FISHEYE;
        }
        else if (model == RadialFisheye) {
            // f, cx, cy, k1, k2
            cam->fx = cameraTrack.params[0];
            cam->fy = cam->fx;
            cam->cx = cameraTrack.params[1];
            cam->cy = cameraTrack.params[2];
            cam->k1 = cameraTrack.params[3];
            cam->k2 = cameraTrack.params[4];
            // OpenCV Fisheye needs 4 radial coeffs: k1, k2, k3, k4
            cam->radial_distortion = torch::tensor({cam->k1, cam->k2, 0.0f, 0.0f}, torch::kFloat32);
            cam->model_type = dvs::CameraModelType::FISHEYE;
        }
        else {
            throw std::runtime_error("Unsupported camera model: " + std::to_string(model));
        }
        camMap[cam->id] = cam;
    }

    size_t numImages = imgfs.size();
    torch::Tensor unorientedPoses = torch::zeros({ static_cast<long int>(numImages), 4, 4 }, torch::kFloat32);
    ret.cameras.resize(numImages);

    for (auto i = 0; i < numImages; i++) {
        uint32_t camId = imgfs[i].camera_id;

        Camera& cam = *camMap[camId];
        cam.filePath = (fs::path(imagePath) / imgfs[i].name).string();

        torch::Tensor qVec = torch::tensor({
            imgfs[i].rotation.w,
            imgfs[i].rotation.x,
            imgfs[i].rotation.y,
            imgfs[i].rotation.z,
            }, torch::kFloat32);
        torch::Tensor R = quatToRotMat(qVec);
        torch::Tensor T = torch::tensor({
            { imgfs[i].translation.x, },
            { imgfs[i].translation.y },
            { imgfs[i].translation.z }
            }, torch::kFloat32);

        torch::Tensor Rinv = R.transpose(0, 1);
        torch::Tensor Tinv = torch::matmul(-Rinv, T);
        unorientedPoses[i].index_put_({ Slice(None, 3), Slice(None, 3) }, Rinv);
        unorientedPoses[i].index_put_({ Slice(None, 3), Slice(3, 4) }, Tinv);
        unorientedPoses[i][3][3] = 1.0f;

        // Convert COLMAP's camera CRS (OpenCV) to OpenGL
        unorientedPoses[i].index_put_({ Slice(0, 3), Slice(1,3) }, unorientedPoses[i].index({ Slice(0, 3), Slice(1,3) }) * -1.0f);
        ret.cameras[i] = cam;
    }

    auto r = autoScaleAndCenterPoses(unorientedPoses);
    torch::Tensor poses = std::get<0>(r);
    ret.translation = std::get<1>(r);
    ret.scale = std::get<2>(r);

    for (size_t i = 0; i < ret.cameras.size(); i++) {
        ret.cameras[i].worldToCam = Camera::getViewMatTensor(poses[i]);
        ret.cameras[i].extractCameraPosRotation(poses[i]);
    }
    //ret.cameras = std::move(cameras);

    const auto numPoints = cpoints.size();
    PointSet* pSet = new PointSet();
    pSet->points.resize(numPoints);
    pSet->colors.resize(numPoints);
    for (auto cp = 0; cp< numPoints; cp++) {
        pSet->points[cp][0] = cpoints[cp].xyz.x;
        pSet->points[cp][1] = cpoints[cp].xyz.y;
        pSet->points[cp][2] = cpoints[cp].xyz.z;

        pSet->colors[cp][0] = cpoints[cp].color.x;
        pSet->colors[cp][1] = cpoints[cp].color.y;
        pSet->colors[cp][2] = cpoints[cp].color.z;
    }
    torch::Tensor points = pSet->pointsTensor().clone();

    ret.points.xyz = (points - ret.translation) * ret.scale;
    ret.points.rgb = pSet->colorsTensor().clone();

    RELEASE_POINTSET(pSet);
    return ret;
}
