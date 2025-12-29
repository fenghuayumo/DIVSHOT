#include "model/model.hpp"
#include "gaussian_trainer_scene.hpp"
#include "video_frame_extractor.hpp"
#include <chrono>
#include <random>
#include <opencv2/opencv.hpp>
#if defined(USE_CUDA)
#include <c10/cuda/CUDACachingAllocator.h>
#include <c10/cuda/CUDAAllocatorConfig.h>
#elif defined(USE_HIP)
#include <c10/hip/HIPCachingAllocator.h>
#endif
#include <image_utils.h>
#include <algorithm>
#include <input_data.hpp>
#include <utils.hpp>
#include <cv_utils.hpp>
#include <utility/thread_pool.h>
#include <utility/file_utils.h>
#include <core/plugin.h>
#include <data_loader.hpp>
#include <colmap_utils.hpp>
#include <tensor_math.hpp>
#include <json/json.hpp>

float maskGenProgress = 0.0f;

float cal_psnr(const torch::Tensor& gt,const torch::Tensor& pred)
{
    auto mse = torch::mean((gt - pred).pow(2));
    return 10 * torch::log10(1.0f / mse).item<float>();
}

int gstrain_init()
{
    //c10::cuda::CUDACachingAllocator::setAllocatorSettings("expandable_segments:True");
    auto path = diverse::parentDirectory(diverse::getExecutablePath());
    // diverse::add_dll_directory(path + "/../libtorch/lib");
    // diverse::add_dll_directory(path + "/../TensorRT-10.12.0.36/lib");
    //system("")
    return  gsplat_init();
}
std::string camera_to_string(int  model)
{
    switch (model)
    {
    case 0:
        return "PINHOLE";
    case 1:
        return "SIMPLE_PINHOLE";
    case 2:
        return "FULL_OPENCV";
    case 3:
        return "OPENCV_FISHEYE";
    default:
        break;
    }
    return "SIMPLE_PINHOLE";
}
void clear_gpu_cache()
{   
#ifdef USE_HIP
        c10::hip::HIPCachingAllocator::emptyCache();
#elif defined(USE_CUDA)
        c10::cuda::CUDACachingAllocator::emptyCache();
#endif
}

#if defined(USE_CUDA)

std::pair<bool, std::string> checkCUDADriverVersion(int targetMajor, int targetMinor) {
    int driverVersion = 0;
    cudaError_t status = cudaDriverGetVersion(&driverVersion);

    if (status != cudaSuccess) {
        return { false, "获取CUDA驱动版本失败: " + std::string(cudaGetErrorString(status)) };
    }

    // 解析版本号：驱动版本号的格式是 major * 1000 + minor * 10
    int currentMajor = driverVersion / 1000;
    int currentMinor = (driverVersion % 1000) / 10;

    std::string versionInfo = "当前CUDA驱动版本: " + std::to_string(currentMajor) + "." +
        std::to_string(currentMinor) + " (" + std::to_string(driverVersion) + ")";

    // 比较版本
    bool isSufficient = false;
    if (currentMajor > targetMajor) {
        isSufficient = true;
    }
    else if (currentMajor == targetMajor && currentMinor >= targetMinor) {
        isSufficient = true;
    }

    std::string resultInfo = versionInfo + " - need version: " + std::to_string(targetMajor) + "." + std::to_string(targetMinor) + " - " +(isSufficient ? "Ok" : "Error");
    return { isSufficient, resultInfo };
}

std::pair<bool, std::string> isCUDADriverAbove12_8() {
    return checkCUDADriverVersion(12, 8);
}

int get_compute_capability(int& major,int& minor)
{
    cudaDeviceProp prop;
    cudaError_t status = cudaGetDeviceProperties(&prop, 0);

    if (status != cudaSuccess) {
        std::cerr << "cudaGetDeviceProperties() failed: " << cudaGetErrorString(status) << std::endl;
        return 0;
    }
    major = prop.major;
    minor = prop.minor;
    return 1;
}
#endif

bool is_device_support_gstrain()
{
#if defined(USE_CUDA) || defined(USE_HIP)
    int major = 0,minor = 0;
    if(!get_compute_capability(major,minor))
        return false;
    return major >= 7;
#elif defined(USE_MPS)
    return true;
#else
    return false;
#endif
}

bool is_driver_support()
{
#if defined(USE_CUDA) || defined(USE_HIP)
    auto p = isCUDADriverAbove12_8();
    if(p.first) return true;
    std::cerr << p.second;
    return false;
#endif
    return true;
}
int gstrain_destroy()
{
    clear_gpu_cache();
    return 1;
}

int get_camera_pos_type_from_file(const std::string& file_path) {
    auto ext = std::filesystem::path(file_path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    if(ext == ".xml"){
       return (int)(EDatasetType::MetaShape);
    }else if(ext == ".csv"){
        return (int)(EDatasetType::RealityCapture);
    }else if(ext == ".bin" || ext == ".txt"){
        auto fname = std::filesystem::path(file_path).filename();
        if ( fname  == "cameras.bin" || fname == "cameras.txt")
            return (int)(EDatasetType::Colmap);
        else
            return -1;
    }else if(ext == ".json"){
        using json = nlohmann::json;
        std::ifstream f(file_path);
        json j = json::parse(f);
        f.close();
        if(j.contains("shots")) return (int)(EDatasetType::OpenSfm);
        // else if (j.contains("camera_angle_x")) return (int)(EDatasetType::Blender);
        return (int)(EDatasetType::NerfStudio);
    }
    return -1;
}

struct Video2Image
{
    bool get_image_from_video(
        const std::string video_path, 
        const int fps, 
        const std::string data_source, 
        int max_width, 
        int max_height,
        int max_images,
        int videoStrategy);

    float get_progress();

    std::atomic<int>   n_handled = 0;
    int   img_count = 0;
};
Video2Image video2img;

bool Video2Image::get_image_from_video(
    const std::string video_path, 
    const int fps, 
    const std::string data_source, 
    int max_width, 
    int max_height,
    int max_images,
    int videoStrategy)
{
    n_handled = 0;
    auto save_path = data_source + "/images";
    
    // Clean up old data
    if (std::filesystem::exists(save_path)) {
        std::filesystem::remove_all(save_path);
        if (std::filesystem::exists(data_source + "/sparse/0/cameras.bin") &&
            std::filesystem::exists(data_source + "/sparse/0/images.bin") &&
            std::filesystem::exists(data_source + "/sparse/0/points3D.bin"))
        {
            std::filesystem::remove_all(data_source + "/sparse/0/");
        }
    }
    
    // First, open video to get actual frame count and fps
    cv::VideoCapture temp_cap(video_path);
    if (!temp_cap.isOpened()) {
        std::cerr << "Failed to open video: " << video_path << std::endl;
        return false;
    }
    
    int total_frames = static_cast<int>(temp_cap.get(cv::CAP_PROP_FRAME_COUNT));
    double video_fps = temp_cap.get(cv::CAP_PROP_FPS);
    temp_cap.release();
    
    // Calculate target frames based on video duration and target fps
    // Video duration (seconds) = total_frames / video_fps
    // Target frames = duration × target_fps
    double duration_sec = total_frames / std::max(1.0, video_fps);
    int calculated_frames = static_cast<int>(duration_sec * fps);
    
    // Apply constraints:
    // 1. Don't exceed max_images (user specified upper limit)
    // 2. Cap at 300 for Hybrid strategy performance (O(K²) complexity)
    // 3. Minimum 50 frames for reasonable coverage
    int target_frames = std::min({calculated_frames, max_images});
    target_frames = std::max(target_frames, 50);
    
    std::cout << std::format("Video info: {} frames @ {:.1f} fps ({:.1f} seconds)\n", 
                            total_frames, video_fps, duration_sec);
    std::cout << std::format("Target: {} fps --> {} frames (capped by max_images={})\n", 
                            fps, target_frames, max_images);
    
    // Configure video frame extractor
    diverse::VideoExtractionConfig config;
    config.target_fps = fps;
    config.max_width = max_width;
    config.max_height = max_height;
    config.max_frames = target_frames;
    config.strategy = static_cast<diverse::FrameSelectionStrategy>(videoStrategy);
    config.diversity_threshold = 0.2f;   // Higher = more different views required
    config.target_candidates = 0;        // Auto: collect max_frames * 3 candidates
    
    std::cout << "Video Strategy: " << static_cast<int>(config.strategy) << std::endl;
    
    // Quality thresholds - Filter out blurry/poor quality frames
    // Note: Set min_sharpness = 0 to disable quality filtering completely
    // Typical sharpness values vary widely by video:
    //   High quality/sharp video: 100-500+
    //   Normal quality: 30-100
    //   Low quality/compressed: 5-30
    //   Very blurry/low-res: < 5
    config.min_sharpness = 5.0f;         // Very low threshold - accept most frames
    config.min_brightness = 10.0f;       // Filter only extremely dark frames
    config.max_brightness = 245.0f;      // Filter only extremely bright frames
    
    // Diversity settings (only used in Hybrid/Diversity strategies)
    config.histogram_bins = 64;          // Histogram resolution for similarity comparison
    
    // Performance settings
    config.num_worker_threads = std::min(8, static_cast<int>(std::thread::hardware_concurrency()));
    config.jpeg_quality = 95;
    config.enable_optimization = true;
    
    // Create extractor
    diverse::VideoFrameExtractor extractor(config);
    
    // Progress callback
    auto progress_callback = [this](int current, int total, const std::string& status) {
        n_handled = current;
        img_count = total;
        if (current % 10 == 0 || current == total) {
            std::cout << std::format("[{}] Progress: {}/{} - {}\n", 
                                    status, current, total, 
                                    current * 100 / std::max(1, total)) << std::flush;
        }
    };
    
    // Extract frames
    bool success = extractor.extract(video_path, save_path, progress_callback);
    
    if (success) {
        const auto& stats = extractor.getStatistics();
        img_count = stats.extracted_frames;
        n_handled = stats.extracted_frames;
        
        std::cout << "\n=== Video Extraction Complete ===\n";
        std::cout << std::format("  Total frames in video: {}\n", stats.total_frames);
        std::cout << std::format("  Extracted frames: {}\n", stats.extracted_frames);
        std::cout << std::format("  Skipped (blurry): {}\n", stats.skipped_blurry);
        std::cout << std::format("  Skipped (brightness): {}\n", stats.skipped_dark);
        std::cout << std::format("  Skipped (similarity): {}\n", stats.skipped_similarity);
        std::cout << std::format("  Average sharpness: {:.2f}\n", stats.avg_sharpness);
        std::cout << std::format("  Processing time: {:.2f} seconds\n", stats.processing_time_sec);
        std::cout << "=================================\n" << std::endl;
    } else {
        std::cerr << "Video extraction failed!" << std::endl;
    }
    
    return success;
}

float Video2Image::get_progress()
{
    return n_handled / std::max<float>(img_count,1);
}
bool has_mask(const std::string& images_path)
{
    std::vector<cv::String> fileNames;
    auto folderPath = (std::filesystem::path(images_path).parent_path() / "masks").string();
    std::vector<std::string> extensions = { ".jpg", ".jpeg", ".png", ".bmp", ".tiff" };
    int total = 0;
    if(std::filesystem::exists(folderPath))
    {
        for (const auto& ext : extensions) {
            std::vector<cv::String> files;
            cv::glob(folderPath + "/*" + ext, files);
            total += files.size();
        }
    }
    bool ret = total > 0;
    if(ret) return true;
    //读取images_path 文件夹下的一张图片，判断是否有alpha通道
    auto png_imgs = std::filesystem::path(images_path) / "*.png";
    std::vector<cv::String> png_files;
    cv::glob(png_imgs.string(), png_files);
    if(png_files.size() > 0)
    {
        auto image = cv::imread(png_files[0], cv::IMREAD_UNCHANGED);
        if(image.channels() == 4) return true;
    }
    return false;
}

int check_image_size(const std::string& images_path,const GaussianTrainConfig& trainConfig)
{
    auto    data_source = std::filesystem::path(images_path).parent_path().string();
    int file_count = 0;
    if (!std::filesystem::exists(images_path))
        return file_count;
    std::vector<std::filesystem::path> image_paths;
    for (const auto& entry : std::filesystem::directory_iterator(images_path)) {
        if (entry.is_regular_file()) {
            file_count++;
            image_paths.push_back(entry.path());
        }
        if( file_count >= trainConfig.maxImageCount)
            break;
    }
    if(file_count == 0) return 0;
    // cv::Mat cImg1 = imreadRGB(image_paths[0].string(), cv::IMREAD_UNCHANGED);
    // if (cImg1.rows > trainConfig.maxImageHeight || cImg1.cols > trainConfig.maxImageWidth){
    //     diverse::parallel_for<size_t>(0, file_count, [&](size_t idx){
    //         cv::Mat cImg = imreadRGB(image_paths[idx].string(), cv::IMREAD_UNCHANGED);
    //         if (cImg.rows > trainConfig.maxImageHeight || cImg.cols > trainConfig.maxImageWidth)
    //         {
    //             float downscaleFactor1 = static_cast<float>(cImg.rows) / static_cast<float>(trainConfig.maxImageHeight);
    //             float downscaleFactor2 = static_cast<float>(cImg.cols) / static_cast<float>(trainConfig.maxImageWidth);
    //             float downscaleFactor = std::max(downscaleFactor1, downscaleFactor2);
    //             if (downscaleFactor > 1.0f){
    //                 float f = 1.0f / downscaleFactor;
    //                 cv::resize(cImg, cImg, cv::Size(), f, f, cv::INTER_AREA);
    //             }
    //             imwriteRGB(image_paths[idx].string(), cImg);
    //         }
    //     });
    // }
    return file_count;
}
int GaussianTrainerScene::getCameraPosFromImage(const std::string& images_path, bool is_video)
{
    auto    data_source = std::filesystem::path(images_path).parent_path().string();
    int file_count = 0;
    if (!std::filesystem::exists(images_path))
        return file_count;
    std::vector<std::filesystem::path> image_paths;
    for (const auto& entry : std::filesystem::directory_iterator(images_path)) {
        if (entry.is_regular_file()) {
            file_count++;
        }
    }
    if (std::filesystem::exists(data_source + "/dvs_cameras.json") && curIteration > 0)
        return file_count;
    if ( (std::filesystem::exists(data_source + "/sparse/cameras.bin") && 
        std::filesystem::exists(data_source + "/sparse/images.bin") &&
        std::filesystem::exists(data_source + "/sparse/points3D.bin")) ||
        (std::filesystem::exists(data_source + "/sparse/0/cameras.bin") && 
        std::filesystem::exists(data_source + "/sparse/0/images.bin") && 
        std::filesystem::exists(data_source + "/sparse/0/points3D.bin")) ||
        (std::filesystem::exists(data_source + "/sparse/cameras.txt") && 
        std::filesystem::exists(data_source + "/sparse/images.txt") && 
        std::filesystem::exists(data_source + "/sparse/points3D.txt")) || 
        (std::filesystem::exists(data_source + "/sparse/0/cameras.txt") && 
        std::filesystem::exists(data_source + "/sparse/0/images.txt") && 
        std::filesystem::exists(data_source + "/sparse/0/points3D.txt")))
    {
        return file_count;
    }
    if(std::filesystem::exists(data_source + "/transforms.xml") || std::filesystem::exists(data_source + "/transforms.csv") || std::filesystem::exists(data_source + "/transforms.json") || std::filesystem::exists(data_source + "/reconstruction.json"))
        return file_count;
    if(std::filesystem::exists(trainConfig.cameraPosePath)) return file_count;

    if (std::filesystem::exists(data_source + "/sparse/") ) 
    {
        std::filesystem::remove_all(data_source + "/sparse/");
    }
    if(!is_device_support_gstrain())
        throw std::runtime_error("current gpu device compute capability doesn't support train");
    if(!is_driver_support())
        throw std::runtime_error("current gpu driver doesn't support train, please update latest gpu driver");
    return -1;
}

GaussianTrainerScene::GaussianTrainerScene() 
    : istraining(false)
{  
    trainConfig.datasetType = -1;
    curIteration = -1;
}

GaussianTrainerScene::GaussianTrainerScene(const GaussianTrainConfig& train_config, int loadIteration)
{
    curIteration = loadIteration;
    trainConfig = train_config;
    istraining = false;
}

GaussianTrainerScene::~GaussianTrainerScene()
{
   {
       isTerminated = true;
#ifdef DS_PLATFORM_WINDOWS
       Sleep(200);
#else
       std::this_thread::sleep_for(std::chrono::milliseconds(200));
#endif
   }
   dataLoader.reset();
   gaussian.reset();
   clear_gpu_cache();
}

auto    GaussianTrainerScene::startTrain() -> void
{
    istraining = true;
    if(curTrainStatus >= TrainingStatus::Training){
        setTrainingStatus(TrainingStatus::Training);
    }
}

auto    GaussianTrainerScene::pauseTrain() -> void
{
    istraining = false;
}

void MaskGenOnProgress(int current, int total, const char* currentFile) {
    maskGenProgress = (float)current / total;
    std::cout << "Progress: " << (maskGenProgress * 100.0f) << "% (" << current << "/" << total << ") - " << currentFile << std::endl;
}

void mask_gen(const std::string& images_path,GaussianTrainerScene* scene)
{
    auto path = diverse::parentDirectory(diverse::getExecutablePath());
    diverse::PluginManager plugin_mgr;
    try{
        if(!plugin_mgr.ensure_plugin_loaded("MaskGen"))
            return;
    }catch(...){
        std::cout << "MaskGen dll load error\n";
        return;
    }
    auto plugin = plugin_mgr.get_plugin("MaskGen");
    if (!plugin)
    {
        return;
    }
    scene->setTrainingStatus(TrainingStatus::MaskGen);
    auto engine_path = path + "/../models/mask_general.engine";
    if(!std::filesystem::exists(engine_path))
        return;
    auto InitializeModel = (bool(*)(const char* enginePath, char* errorMessage, int errorMessageSize))plugin->get_symbol("InitializeModel");
    if (!InitializeModel)
    {
        std::cerr << "plugin init failed " << std::endl;
        exit(-1);
    }
    char errorMsg[1024];
    if (!InitializeModel(engine_path.c_str(), errorMsg, sizeof(errorMsg)))
    {
        std::cerr << "initialize model failed" << std::endl;
        return;
    }
    typedef void (*ProgressCallback)(int current, int total, const char* currentFile);
    // Result structure
    struct ProcessResult {
        bool success;
        std::string message;
        int processedCount;
        int totalCount;
    };
    auto ProcessImageFolder = (ProcessResult(*)(const char* inputFolderPath, const char* outputFolderPath, ProgressCallback progressCallback))plugin->get_symbol("ProcessImageFolder");
    if (!ProcessImageFolder)
    {
        std::cerr << "process image folder get symbol failed " << std::endl;
    }
    auto outputPath = (std::filesystem::path(images_path).parent_path() / "masks").string();
    ProcessResult result = ProcessImageFolder(images_path.c_str(), outputPath.c_str(), MaskGenOnProgress);
    if (result.success) {
        std::cout << "Mask generation completed: " << result.message << std::endl;
        std::cout << "Output directory: " << outputPath << std::endl;
    }
    else {
        std::cout << "Failed to generate masks: " << result.message << std::endl;
    }
    typedef void(*voidFunc)();
    auto Cleanup = (voidFunc)plugin->get_symbol("Cleanup");
    if (!Cleanup)
    {
        std::cerr << "cleanup get symbol failed " << std::endl;
    }
    Cleanup();
    maskGenProgress = 0.0f;
}

bool GaussianTrainerScene::loadTrainData(const std::string& fpath)
{
    curTrainStatus = TrainingStatus::Loading_Prepare;

    auto parent_path = std::filesystem::path(fpath).parent_path();
    //trainConfig.sourcePath = parent_path.string();
    std::string imagePath = fpath;
    bool is_video_data = false;
    if (std::filesystem::is_directory(imagePath))
    {
        if (!std::filesystem::exists(imagePath))
        {
            curTrainStatus = TrainingStatus::Loading_Failed;
            return false;
        }
        for (const auto& entry : std::filesystem::directory_iterator(imagePath)) {
            if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
                auto use_f32_data = ".hdr" == ext || ".exr" == ext;
                if (use_f32_data)
                {
                    trainConfig.packLevel &= ~GSPackLevel::PackF32ToU8;
                }
                break;
            }
        }
    }
    else
    {
        auto ext = std::filesystem::path(fpath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        if (ext == ".mp4" || ext == ".m4v" || ext == ".avi" || ext == ".mov")
        {
            is_video_data = true;
            bool video_handle = false;
            try
            {
                auto datasource = std::filesystem::path(fpath).replace_extension("");
                int max_image_width = trainConfig.maxImageWidth;
                int max_image_height = trainConfig.maxImageHeight;
                video_handle = video2img.get_image_from_video(
                                fpath, 
                                trainConfig.videoFps, datasource.string(), 
                                max_image_width, max_image_height,
                                trainConfig.exportMesh ? 120 : trainConfig.maxImageCount,
                                trainConfig.videoStrategy);
                imagePath = datasource.string() + "/images";
            }
            catch (...)
            {
                std::cout << "opencv slice video error!\n";
                video_handle = false;
            }
            if (!video_handle)
            {
                curTrainStatus = TrainingStatus::Loading_Failed;
                throw std::runtime_error("video process error!\n");
                return false;
            }
        }
        else {
            std::cout << "encounter unkhown data type!\n";
            curTrainStatus = TrainingStatus::Loading_Failed;
            throw std::runtime_error("encounter unkhown video data type!\n");
            return false;
        }
    }
    int img_cnt = check_image_size(imagePath,trainConfig);
    if( img_cnt <= 0)
    {
        curTrainStatus = TrainingStatus::Loading_Failed;
        istraining = false;
        return false;
    }
    if(!has_mask(imagePath) && trainConfig.useMask)
    {
        mask_gen(imagePath,this);
    }
    istraining = true;
    img_cnt = getCameraPosFromImage(imagePath, is_video_data);
    if( img_cnt <= 0)
    {
        curTrainStatus = TrainingStatus::Loading_Failed;
        istraining = false;
        return false;
    }
    trainConfig.sourcePath = imagePath;
    trainConfig.visibleAdam = true;
    trainConfig.rankRegularization = false;
    // if( trainConfig.useAbsGrad)
    //     trainConfig.growGrad2d *= 4;
    if( trainConfig.modelPath.empty())
        trainConfig.modelPath = parent_path.string() + "/3dgs.ply";
    return loadTrainData();
}

bool GaussianTrainerScene::loadTrainData()
{
    curTrainStatus = TrainingStatus::Preprocess_TrainingData;
    std::filesystem::path projectRoot = std::filesystem::path(trainConfig.sourcePath).parent_path();
    if (!std::filesystem::exists(projectRoot.string()))
    {
        std::cout << std::format("{} is not a valid dataset path,it must set a valid dataset path", projectRoot.string());
        curTrainStatus = TrainingStatus::Loading_Failed;
        istraining = false;
        throw std::runtime_error(std::format("{} is not a valid dataset path,it must set a valid dataset path", projectRoot.string()));
    }
    if(!dataLoader)
    {
        DataLoaderConfig    dataConfig = DataLoaderConfig{ trainConfig.maxImageWidth,trainConfig.maxImageHeight,trainConfig.batchLoader, trainConfig.batchSize };
        dataConfig.useMask = trainConfig.useMask;
        dataConfig.useDepth = trainConfig.depthLoss;
        dataConfig.useNormal = trainConfig.normalConsistencyLoss;
        dataConfig.datasetType = trainConfig.datasetType;
        // dataConfig.enablePipeline = false;
        dataLoader = std::make_shared<GaussianInputDataLoader>(dataConfig);
        try {
            dataLoader->load(trainConfig.sourcePath,trainConfig.cameraPosePath, trainConfig.pointCloudPath);
        } catch (const std::exception& e) {
            curTrainStatus = TrainingStatus::Loading_Failed;
            istraining = false;
            throw std::runtime_error(e.what());
        }
    }
    if (curIteration > 0)
    {
        //load trained 3d gaussian point to continue trainning
        std::cout << "Loading trained model at iteration " << curIteration << std::endl;
    }

    auto device = get_gsplat_device();
    if (!dataLoader->inputData.points.xyz.defined() || !dataLoader->inputData.points.rgb.defined() || 
        dataLoader->inputData.points.xyz.size(0) == 0 || dataLoader->inputData.points.rgb.size(0) == 0)
        trainConfig.randomInitPoints = true;
    if (trainConfig.randomInitPoints) {
        trainConfig.warmupLength = 1000;
    }
    if (curIteration > 0)
    {
        gaussian = createGaussianModel(
            std::filesystem::path(trainConfig.modelPath),
            dataLoader->inputData,
            getNumCameras(),
            trainConfig,
            device);
    }
    else 
    {
        gaussian = createGaussianModel(
            dataLoader->inputData,
            getNumCameras(),
            trainConfig,
            device);
    }
    auto dxyz = dataLoader->inputData.points.xyz;
    auto init_pts = dxyz.numel() > 0 ? dxyz.cpu() : gaussian->means.cpu();
    auto [min_points,max_points] = pointsBounds(init_pts);
    init_region_box_min = *(glm::vec3*)min_points.to(torch::kFloat32).data_ptr<float>();
    init_region_box_max = *(glm::vec3*)max_points.to(torch::kFloat32).data_ptr<float>();
    curTrainStatus = TrainingStatus::Preprocess_Done;
    if(!trainConfig.bestQuality && pruenIteraions.empty())
    {
        int pruneIter = 0.5 * trainConfig.refineStopIter - 1;
        pruenIteraions.emplace_back(pruneIter);
        pruneIter = 0.75 * trainConfig.refineStopIter - 1;
        pruenIteraions.emplace_back(pruneIter);
        // if(trainConfig.densifyStrategy == (int)SplatDensifyType::SplatMCMC)
        // {
        //     pruneIter = trainConfig.refineStopIter * 1.2 - 1;
        //     pruenIteraions.emplace_back(pruneIter);
        // }
    }
    if(trainConfig.numIters < 10000)
        trainConfig.progressiveTrain = false;

    trainConfig.scaleReg = std::min<float>(0.05, 0.1 / dataLoader->sceneExtent);
    trainConfig.minOpacity = 0.01;
    if (trainConfig.useMask || 
        trainConfig.densifyStrategy == SplatDensifyType::SplatMCMC || 
        trainConfig.densifyStrategy == SplatDensifyType::SplatADCPlus){
        trainConfig.opacityReg = 1e-4f;
    }
    if(!trainConfig.useMask)
    {
        trainConfig.pruneOpacity = 0.04f;
    }
    //if (trainConfig.densifyStrategy == SplatDensifyType::SplatADCPlus)
    //{
    //    trainConfig.opacityReg = 0;
    //    trainConfig.scaleReg = 0;
    //}
    return true;
}

void    GaussianTrainerScene::resetGaussian()
{
    clear_gpu_cache();
    auto device = get_gsplat_device();
    gaussian = createGaussianModel(dataLoader->inputData,
        getNumCameras(),
        trainConfig,
        device);
    trainSetup();
    curIteration = 0;
    trainingElpasedTime = 0;
}

void    GaussianTrainerScene::loadGaussianTraningModel()
{
    std::thread t([this]() {
        loadTrainData();
        trainSetup();
        gaussian->loadOptimizerStates(curIteration);
    });
    t.detach();
}

size_t GaussianTrainerScene::getNumCameras()
{
    if( !dataLoader ) return 0;
    return dataLoader->inputData.cameras.size();
}

void schedulersStepTrainConfig(int step,GaussianTrainConfig& trainConfig)
{
    auto growGrad2dA = 1e-4;
    auto growGrad2dB = 2e-4;
    //linear interpolation between scaleRegA and scaleRegB based on warmupLength and refineStopIter
    auto growGrad2d = growGrad2dA + (growGrad2dB - growGrad2dA) * (step - trainConfig.warmupLength) / static_cast<float>(trainConfig.refineStopIter - trainConfig.warmupLength);
    trainConfig.growGrad2d = growGrad2d;

    auto noiselrA = 1e5;
    auto noiselrB = 1e3;
    trainConfig.noiselr = noiselrA + (noiselrB - noiselrA) * (step / static_cast<float>(trainConfig.numIters));
}

void GaussianTrainerScene::trainStep()
{
    if( curIteration > trainConfig.numIters) return;
    if(curTrainStatus == TrainingStatus::Loading_Failed) return;
    if(isPruning) return;
    ++curIteration;
    curTrainStatus = TrainingStatus::Training;
    
    const int displayStep = 100;
    const int numCameras = getNumCameras();
    auto [camId,cam] = dataLoader->next_pair();
    auto renderMode = trainConfig.modelType == SplatModelType::Splat2D ? GSplatRenderMode::RGBED :  GSplatRenderMode::RGB;
    auto rasterizeMode = trainConfig.mipAntiliased ? GSRasterizeMode::Antialiased : GSRasterizeMode::Classic;
    torch::Tensor mask;
    auto [render_pkg,infos] = gaussian->forward(
        cam, curIteration, 
        renderMode,
        rasterizeMode);
    if(trainConfig.enableFocusRegion)
    {
        auto [min,max] = getFocusRegion();
        torch::Tensor box = torch::tensor({min.x, min.y, min.z, max.x, max.y, max.z}, torch::kFloat32);
        infos["focusBox"] = box;
    }
    auto rgb = render_pkg["colors"];
    auto alpha = render_pkg["alphas"];
    auto render_depths = render_pkg["depths"];
    if( trainConfig.randombkgd)
    {
        auto bkgd = torch::rand({1,3}, rgb.options());
        rgb = rgb + bkgd * ( 1 - alpha);
    }
    const auto downFactor = trainConfig.progressiveTrain ? gaussian->getDownscaleFactor(curIteration) : 1;
    torch::Tensor gt = cam.getImage(downFactor);
    if(trainConfig.useMask)
    {
        mask = cam.getMask(downFactor);
        if(mask.defined())
            rgb = rgb * torch::logical_not(mask);
    }
    torch::Tensor mainLoss = gaussian->mainLoss(rgb, gt,(1 - trainConfig.ssimWeight));
    if(trainConfig.useMask)
    {
        if(mask.defined() && alpha.defined())
        {
            auto lb = (alpha * mask).sum() / (cam.width * cam.height);
            mainLoss += lb;
        }
    }
    float radenormalConsistencyLoss = 0.0;
    if( trainConfig.scaleReg > 0 && curIteration % 10 == 0)
    {
        auto scaleExp = torch::exp(gaussian->scales);
        auto scaleRegValue = trainConfig.scaleReg * scaleExp.mean();
        if (torch::isnan(scaleRegValue).sum().item<float>() == 0.0f)
            mainLoss += scaleRegValue;
        if( trainConfig.rankRegularization && curIteration > 7000 && curIteration < 10000)
        {
            auto scaleExp = torch::exp(gaussian->scales);
            auto s1 = scaleExp.index({torch::indexing::Slice(), 0});
            auto s2 = scaleExp.index({torch::indexing::Slice(), 1});
            auto s3 = scaleExp.index({torch::indexing::Slice(), 2});
            auto sumSquares = s1 * s1 + s2 * s2 + s3 * s3;

            auto s1_normalized = (s1 * s1) / sumSquares;
            auto s2_normalized = (s2 * s2) / sumSquares;
            auto s3_normalized = (s3 * s3) / sumSquares;

            auto h1 = s1_normalized * torch::log(s1_normalized);
            auto h2 = s2_normalized * torch::log(s2_normalized);
            auto h3 = s3_normalized * torch::log(s3_normalized);
            auto h = -(h1 + h2 + h3);

            auto erank = torch::exp(h);
            auto log_h = -torch::log(erank - 1 + trainConfig.lambdaRank);

            auto loss_rank = trainConfig.lambdaErank * torch::max(log_h, torch::tensor(0.0));
            loss_rank = loss_rank.mean();

        //  auto [min_scale,_] = torch::min(scaleExp, 1);
        //  min_scale = torch::clamp(min_scale, 0, 30);
        //  auto scale_loss = 0.01f * torch::abs(min_scale).mean() + loss_rank;
            if( torch::isnan(loss_rank).sum().item<float>() == 0.0f)
            mainLoss += loss_rank;
        }
    }
    const auto rade = trainConfig.normalConsistencyLoss && curIteration > trainConfig.refineStopIter;

    if(trainConfig.opacityReg > 0 && curIteration % 10 == 0){
        mainLoss += trainConfig.opacityReg * torch::sigmoid(gaussian->opacities).mean();
    }
    if (curIteration % numCameras == 0)
        totalLoss = 0;
    totalLoss += mainLoss.item<float>();
    currentLoss = totalLoss / (curIteration % numCameras + 1);
    if (curIteration % displayStep == 0 && trainConfig.verbose) std::cout << std::format("Step {} loss : {}, normalConsistencyLoss: {} \n", curIteration, currentLoss, radenormalConsistencyLoss);
    if(curIteration % 10000 == 0)
    {
        auto psnr = cal_psnr(gt,rgb);
        std::cout << std::format("Step {} PSNR : {}\n", curIteration, psnr);
    }
    gaussian->optimizersZeroGrad();
    mainLoss.backward();
    if (trainConfig.enableBg) {
        auto& grad = gaussian->means.mutable_grad();
        //grad[:self.trainConfig.numSkyPoints] = 0
        auto numSkyPoints = trainConfig.numSkyPoints;
        if (numSkyPoints > 0) {
			grad.index_put_({torch::indexing::Slice(0, numSkyPoints)}, 0);
		}
    }  
    torch::Tensor visibility_mask;
    auto gaussian_id = infos["gaussianIds"];
    auto radii = infos["radii"];
    
    if (trainConfig.visibleAdam && radii.defined()) {
        if (trainConfig.packLevel & GSPackLevel::PackTileID ) {
           visibility_mask = torch::zeros_like(gaussian->opacities, torch::kBool).squeeze(-1);
           visibility_mask.scatter_(0, gaussian_id,1);
        }
        else {
            visibility_mask = (radii > 0).all(-1).any(0);
        }
    }
    gaussian->optimizersStep(visibility_mask);
    gaussian->schedulersStep(curIteration);
    schedulersStepTrainConfig(curIteration,trainConfig);
    
    try{
        gaussian->stepAfterbackward(curIteration, infos);
    }
    catch(const std::exception& e)
    {
        throw std::runtime_error("densify error");
    }

    auto it = std::find(pruenIteraions.begin(), pruenIteraions.end(), curIteration);
    if(pruenIteraions.size() > 0){
        if (it!= pruenIteraions.end()){
            isPruning = true;
            gaussian->prune(dataLoader->inputData.cameras, curIteration);
            isPruning = false;
        }
    }
}

void GaussianTrainerScene::trainSetup()
{
    gaussian->trainSetup(dataLoader->sceneExtent);
    std::cout << "trainSetup sceneExtent: " << dataLoader->sceneExtent  << std::endl;
    istraining = true;
}

auto GaussianTrainerScene::setDensifyStrategy(int type)->void
{
    if(gaussian)
        gaussian->setDensifyStrategy(type);
}

std::vector<float> GaussianTrainerScene::getGaussianPositionCpu()
{
    auto host_xyz = gaussian->means.to(torch::kCPU);
    auto data = host_xyz.contiguous().data_ptr<float>();
    return std::vector<float>(data, data + host_xyz.numel());
}

std::vector<float> GaussianTrainerScene::getGaussianOpcaitiesCpu()
{
    auto host_opacity = gaussian->opacities.to(torch::kCPU);
    auto data= host_opacity.contiguous().data_ptr<float>();

    return std::vector<float>(data, data + host_opacity.numel());
}

std::vector<float> GaussianTrainerScene::getGaussianRotationsCpu()
{
    auto host = gaussian->quats.to(torch::kCPU);
    auto data =  host.contiguous().data_ptr<float>();

    return std::vector<float>(data, data + host.numel());
}

std::vector<float> GaussianTrainerScene::getGaussianScalingsCpu()
{
    auto host = gaussian->scales.to(torch::kCPU);
    auto data =  host.contiguous().data_ptr<float>();
    std::vector<float> scales(data, data + host.numel());
    return scales;
}

//float* GaussianTrainerScene::getGaussianSHsCpu()
//{
//    auto host = gaussian->getSHs().to(torch::kCPU);
//    auto data = host.contiguous().data_ptr<float>();
//    return data;
//}

auto GaussianTrainerScene::getGaussianSH0Cpu() -> std::vector<float>
{
    auto host = gaussian->featuresDc.to(torch::kCPU);
    auto data = host.contiguous().data_ptr<float>();
    std::vector<float> shs_0(data, data + host.numel());
    return shs_0;
}

auto GaussianTrainerScene::getGaussianSHNCpu() -> std::vector<float>
{
    auto host = gaussian->featuresRest.to(torch::kCPU);
    auto data = host.contiguous().data_ptr<float>();
    std::vector<float> shs_n(data, data + host.numel());
    return shs_n;
}

size_t GaussianTrainerScene::getNumGaussians()
{
    return gaussian->means.size(0);
}

glm::vec3 GaussianTrainerScene::getCameraPos(int idx)
{
    return dataLoader->inputData.cameras.at(idx).getCameraPos();
}

glm::quat GaussianTrainerScene::getCameraRotation(int idx)
{
    return dataLoader->inputData.cameras.at(idx).getCameraRotation();
}

glm::mat4 GaussianTrainerScene::getCameraProjection(int idx)
{
    return dataLoader->inputData.cameras.at(idx).getProjMat();
}

void GaussianTrainerScene::setMaxTrainImageExtent(const std::array<int,2>& extent)
{
    trainConfig.maxImageWidth = extent[0];
    trainConfig.maxImageHeight = extent[1];
}

void GaussianTrainerScene::saveGaussianModel() const
{
    // saveCameraDatas();
    gaussian->save(trainConfig.modelPath);
    gaussian->saveOptimizerStates();
}

void GaussianTrainerScene::saveCameraDatas(const std::string& filePath) const
{
    if( !dataLoader ) return;
    dataLoader->inputData.saveCameras(filePath, trainConfig.keepCrs);
    std::cout << "wrote camera datas to file " << filePath << "\n";
}

void GaussianTrainerScene::updateTensorFromGaussianData(
    const std::vector<glm::vec3>& pos,
    const std::vector<glm::vec4>& rot,
    const std::vector<glm::vec3>& scale,
    const std::vector<float>& opacity,
    const std::vector<std::array<float, 3>>& shs_0,
    const std::vector<std::array<float, 45>>& shs_n)
{
    gaussian->updateGaussianAttributes(
        pos,
        rot,
        scale,
        opacity,
        shs_0,
        shs_n
    );
}

TrainingStatus GaussianTrainerScene::getCurrentTrainingStatus()
{
    return curTrainStatus;
}

float GaussianTrainerScene::getProgressOnCurrentPhase() const
{
    if (curTrainStatus == TrainingStatus::Loading_Prepare)
    {
        return video2img.get_progress();
    }
    else if(curTrainStatus == TrainingStatus::MaskGen)
    {
        return maskGenProgress;
    }
    else if(curTrainStatus == TrainingStatus::Preprocess_TrainingData)
    {
        if(!dataLoader) return 0.0f;
        return dataLoader->load_progress();
    }   
    else if(curTrainStatus == TrainingStatus::Training){
        return curIteration / static_cast<float>(trainConfig.numIters);
    }
    return 1.0f;
}

std::string GaussianTrainerScene::getCurrentTrainingPhaseName() const
{
    switch (curTrainStatus)
    {
    case TrainingStatus::Loading_Prepare:
		return "PreprocessData";
    case TrainingStatus::MaskGen:
        return "MaskGen";
    case TrainingStatus::Colmap_FeatureExtract:
		return "FeatureExtraction";
    case TrainingStatus::Colmap_FeatureMatch:
		return "FeatureMatching";
    case TrainingStatus::Colmap_Sfm:
		return "SparseReconstruct";
    case TrainingStatus::Preprocess_TrainingData:
		return "PreProcess";
    case TrainingStatus::Preprocess_Done:
        return "PreprocessDone";
    case TrainingStatus::Loading_Failed:
	    return "LoadingFailed";
    case TrainingStatus::Training:
		return "SplatTraining";
    case TrainingStatus::GS2Mesh:
        return "ExtractMesh";
    case TrainingStatus::Training_Done:
        return "Training Done";
    default:
        break;
    }
    return "Loading Failed";
}

auto GaussianTrainerScene::getPoints3D(int id) const->std::vector<colmap::SparsePoint>
{
    return {};
}

auto GaussianTrainerScene::exportSparsePointCloud(const std::string& filePath)const ->void
{
    if(dataLoader){
        dataLoader->inputData.saveSparsePointSet(filePath);
        std::cout << "wrote camera datas to file " << filePath << "\n";
    }
}

auto GaussianTrainerScene::getSplatImageView(int id)->SplatImageView const
{
    if(dataLoader){
        SplatImageView view;
        auto t = dataLoader->trainCameras.at(id)->getImage(1).cpu();
        u32 h = t.sizes()[0];
        u32 w = t.sizes()[1];
        u32 c = t.sizes()[2];
        if (c == 3) 
        {
            t = torch::cat({ t, torch::ones({ h,w,1 }, t.options()) }, 2);
            c = t.size(2);
        }
        std::vector<u8> data( h * w * c);
        torch::Tensor scaledTensor = (t * 255.0).toType(torch::kU8);
        uint8_t* dataPtr = static_cast<uint8_t*>(scaledTensor.data_ptr());
        std::copy(dataPtr, dataPtr + (w * h * c), data.data());
        auto filePath = dataLoader->trainCameras.at(id)->filePath;
        auto name = std::filesystem::path(filePath).filename().string();
        return SplatImageView{w,h,name,data};
    }
    return SplatImageView{};
}

auto GaussianTrainerScene::getFocusRegion()->std::tuple<glm::vec3, glm::vec3>
{
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), focus_region_position) * glm::mat4_cast(glm::quat(focus_region_rotation)) * glm::scale(glm::mat4(1.0f), focus_region_scale);
    auto min = transform * glm::vec4(init_region_box_min, 1.0f);
    auto max = transform * glm::vec4(init_region_box_max, 1.0f);
    return {min,max};
}

auto GaussianTrainerScene::getFocusRegionTransform()->glm::mat4
{
    return glm::translate(glm::mat4(1.0f), focus_region_position) * glm::mat4_cast(glm::quat(focus_region_rotation)) * glm::scale(glm::mat4(1.0f), focus_region_scale);
}

auto GaussianTrainerScene::updateFocusRegion(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)->void
{
    focus_region_position = position;
    focus_region_rotation = rotation;
    focus_region_scale = scale;
    auto [min,max] = getFocusRegion();
    //get 8 points of the aabb
    std::vector<glm::vec3> points(8);
    points[0] = glm::vec3(min.x, min.y, min.z);
    points[1] = glm::vec3(max.x, min.y, min.z);
    points[2] = glm::vec3(min.x, max.y, min.z);
    points[3] = glm::vec3(max.x, max.y, min.z);
    points[4] = glm::vec3(min.x, min.y, max.z);
    points[5] = glm::vec3(max.x, min.y, max.z);
    points[6] = glm::vec3(min.x, max.y, max.z);
    points[7] = glm::vec3(max.x, max.y, max.z);
    // for(auto cam : dataLoader->trainCameras)
    // {
    //     auto KsMat = cam->getProjectionMatrix(1,0.01,1000);
    //     auto viewMatrix = cam->worldToCam.to(KsMat.device());
    //     auto v_cam = torch::matmul(torch::cat({torch::from_blob(points.data(),{8,3}).to(viewMatrix.device()), torch::ones({8,1}, viewMatrix.device())}, 1), viewMatrix.t()).to(torch::kFloat).unsqueeze(0);
    //     auto clip_v = torch::matmul(v_cam, KsMat.t());
    //     clip_v = clip_v / clip_v.index({torch::indexing::Slice(), torch::indexing::Slice(3,4)});
    //     auto width = cam->width;
    //     auto height = cam->height;
    //     //nvdiffrast a cube to get mask  
    // }
}

auto GaussianTrainerScene::exportMesh(const std::string& filePath)->bool const
{
    throw std::runtime_error("exportMesh is not supported");
    return false;
}

auto GaussianTrainerScene::getEstimateTrainingTime() -> float
{
   static TrainingStatus prev_status = getCurrentTrainingStatus();
   static auto prevTime = std::chrono::high_resolution_clock::now();
   static float prev_progress= getProgressOnCurrentPhase();
   static float avgTime = 0.0f;
   auto cur_status = getCurrentTrainingStatus();
   
   if (cur_status != prev_status) {
       prevTime = std::chrono::high_resolution_clock::now();
       prev_progress = getProgressOnCurrentPhase();
   }
   auto progress = getProgressOnCurrentPhase();
   if (std::abs(progress - prev_progress) >= 1e-3f)
   {
       auto now = std::chrono::high_resolution_clock::now();
       auto timecnt_10 = std::chrono::duration_cast<std::chrono::milliseconds>(now - prevTime).count();
       avgTime = timecnt_10 / 1000.0f / (std::abs(progress - prev_progress) + 1e-4f);
       prevTime = std::chrono::high_resolution_clock::now();
       prev_progress = getProgressOnCurrentPhase();
   }
   prev_status = cur_status;
   return avgTime * (1-progress);
}

auto GaussianTrainerScene::getTrainingElpasedTime() -> float
{
    static auto prevTime = std::chrono::high_resolution_clock::now();
    auto cur_status = getCurrentTrainingStatus();
    if (istraining && cur_status != TrainingStatus::Training_Done)
    {
        trainingElpasedTime += (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - prevTime).count()) / 1000.0f;
    }
    prevTime = std::chrono::high_resolution_clock::now();
    return trainingElpasedTime;
}

namespace diverse
{
    DS_IMPL_PLUGIN(GaussianTrainerScene, "SplatTrainScene")

    extern "C" { \
        bool DS_EXPORT load_train_data(GaussianTrainerScene* scene,const std::string& path) { \
            bool ret = scene->loadTrainData(path);\
            scene->trainSetup(); \
            return ret;     \
        }\
        void DS_EXPORT train_step(GaussianTrainerScene* scene) {\
            return scene->trainStep(); \
        }\
        void DS_EXPORT save_splat_model(GaussianTrainerScene* scene) {\
            scene->saveGaussianModel(); \
        }\
        void DS_EXPORT* create_splat(const GaussianTrainConfig& config,int loadIter) {\
            return new GaussianTrainerScene(config,loadIter); \
        }\
        void DS_EXPORT delete_splat(GaussianTrainerScene* scene){\
            if(scene) delete scene;\
        }\
        void DS_EXPORT export_mesh(GaussianTrainerScene* scene) {\
            auto mesh_path = std::filesystem::path(scene->getTrainConfig().modelPath).replace_extension("").string() + "_mesh.obj";\
            scene->exportMesh(mesh_path);\
        }\
        int DS_EXPORT get_cur_step(GaussianTrainerScene* scene) {\
            return scene->getCurrentIterations();
        }\
    }
}
