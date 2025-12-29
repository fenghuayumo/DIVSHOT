#include "data_loader.hpp"
#include <utility/thread_pool.h>
#include <format>

float adjustSceneExtent(const torch::Tensor& points, const torch::Tensor& cam_center) {
    auto dist = torch::linalg_norm(points - cam_center, 2, { -1 }, true, torch::kFloat32);
    auto mean_dist = torch::mean(dist).item<float>();
    return mean_dist;
}

float getSceneExtent(const std::vector<Camera>& cam_infos,const torch::Tensor& points)
{
    std::unordered_map<std::string, float> nerfNorm;
    auto getCenterDiag = [](const std::vector<glm::vec3>& cam_centers,const torch::Tensor& points) {
        glm::vec3 avg_cam_center(0);
        for (auto cam : cam_centers)
            avg_cam_center += cam;
        avg_cam_center /= cam_centers.size();
        float diagonal = 0.0;
        if(points.size(0) > 0){
            auto dist = torch::linalg_norm(points - torch::tensor({avg_cam_center.x,avg_cam_center.y,avg_cam_center.z}), 2, { -1 }, true, torch::kFloat32);
            diagonal = torch::mean(dist).item<float>();
        }else{
            for (auto cam : cam_centers)
            {
                auto dist = glm::length(cam - avg_cam_center);
                diagonal = std::max(diagonal, dist);
            }
        }       
        return diagonal;
    };

    std::vector<glm::vec3> cam_centers(cam_infos.size());
    diverse::parallel_for<size_t>(0, cam_infos.size(), [&](size_t i) {
        const auto& cam = cam_infos[i];
        cam_centers[i] = cam.getCameraPos();
    });
    auto diag = getCenterDiag(cam_centers,points);
    auto radius = diag;
    return radius;
}

float getSceneExtent(const std::vector<Camera>& cam_infos){
    return getSceneExtent(cam_infos,torch::Tensor());
}

GaussianInputDataLoader::GaussianInputDataLoader(const DataLoaderConfig& config)
    : config(config)    
{
}

GaussianInputDataLoader::~GaussianInputDataLoader() {
    if (config.enablePipeline) {
        stop_pipeline();
    }
}

void GaussianInputDataLoader::load(const std::string& imagePath,
            const std::string& posePath,
            const std::string& pointPath, 
            int validateNum)
{
    try {
        inputData = inputDataFromX(imagePath, posePath, pointPath, (EDatasetType)(config.datasetType));
    } catch (const std::exception& e) {
        std::cerr << "Error loading input data: " << e.what() << std::endl;
        throw std::runtime_error(e.what());
    }
    
    // If pipeline is disabled, load all images synchronously (original behavior)
    if (!config.enablePipeline) {
        diverse::parallel_for<size_t>(0, inputData.cameras.size(), [this](size_t idx) {
            auto& cam = inputData.cameras[idx];
            cam.loadImage(config.maxWidth, 
                        config.maxHeight, 
                        config.useMask, 
                        config.useDepth, 
                        config.useNormal);
            ++n_loaded;
        }); 
        std::cout << std::format("Loaded {} images \n", n_loaded.load());
    } else {
        // Pipeline mode: lazy loading
        std::cout << std::format("Pipeline mode enabled: {} cameras will be loaded on-demand\n", 
                                inputData.cameras.size());
    }

    sceneExtent = getSceneExtent(inputData.cameras,inputData.points.xyz);
    auto [cam0,cam1] = inputData.getCameras(validateNum > 0, validateNum);
    trainCameras = std::move(cam0);
    validateCameras = std::move(cam1);
    std::vector< size_t > camIndices(trainCameras.size());
    std::iota(camIndices.begin(), camIndices.end(), 0);
    camsIter = std::make_unique<InfiniteRandomIterator<size_t>>(camIndices);
    
    // Start pipeline if enabled
    if (config.enablePipeline) {
        start_pipeline();
    }
}

void GaussianInputDataLoader::load(const std::string& imagePath,
    std::vector<colmap::SparsePoint>&& points,
    std::vector<colmap::CameraTrack>&& cameras,
    std::vector<colmap::ImageTrack>&& imgfs,
    const std::string& posePath,
    const std::string& pointPath,
    int validateNum)
{
    inputData = inputDataFromColmapCameraPoints(imagePath, std::move(cameras), std::move(points), std::move(imgfs));
    
    // If pipeline is disabled, load all images synchronously (original behavior)
    if (!config.enablePipeline) {
        diverse::parallel_for<size_t>(0, inputData.cameras.size(), [this](size_t idx) {
            auto& cam = inputData.cameras[idx];
            cam.loadImage(config.maxWidth, 
                        config.maxHeight, 
                        config.useMask, 
                        config.useDepth, 
                        config.useNormal);
            ++n_loaded;
        });
        std::cout << std::format("Loaded {} images \n", n_loaded.load());
    } else {
        // Pipeline mode: lazy loading
        std::cout << std::format("Pipeline mode enabled: {} cameras will be loaded on-demand\n", 
                                inputData.cameras.size());
    }

    sceneExtent = getSceneExtent(inputData.cameras,inputData.points.xyz);
    auto [cam0,cam1] = inputData.getCameras(validateNum > 0, validateNum);
    trainCameras = std::move(cam0);
    validateCameras = std::move(cam1);
    std::vector< size_t > camIndices(trainCameras.size());
    std::iota(camIndices.begin(), camIndices.end(), 0);
    camsIter = std::make_unique<InfiniteRandomIterator<size_t>>(camIndices);
    
    // Start pipeline if enabled
    if (config.enablePipeline) {
        start_pipeline();
    }
}

Camera& GaussianInputDataLoader::next()
{
    // in the data_loader.cpp next() method
    if (config.enablePipeline && pipeline_running_) {
        auto batch_opt = batch_queue_->recv();
        if (!batch_opt) {
            // ensure the camera is loaded
            auto idx = camsIter->next();
            // Thread-safe lazy loading
            load_camera_lazy(idx);
            return *(trainCameras[idx]);
        }
        return *(batch_opt->camera_ptr);
    }
    else {
        auto idx = camsIter->next();
        return *(trainCameras[idx]);
    }
}

std::pair<size_t, Camera&> GaussianInputDataLoader::next_pair()
{
    if (config.enablePipeline && pipeline_running_) {
        // Pipeline mode: get from batch queue
        auto batch_opt = batch_queue_->recv();
        if (!batch_opt) {
            auto idx = camsIter->next();
            // Thread-safe lazy loading
            load_camera_lazy(idx);
            return {idx, *(trainCameras[idx])};
        }
        return {batch_opt->camera_index, *(batch_opt->camera_ptr)};
    } else {
        // Original mode: direct access
        auto idx = camsIter->next();
        return {idx, *(trainCameras[idx])};
    }
}

float GaussianInputDataLoader::load_progress() {
    float progress = n_loaded.load() / std::max<float>(inputData.cameras.size(), 1);
    return progress;
}

// ============================================================================
// Two-Stage Pipeline Implementation
// ============================================================================

void GaussianInputDataLoader::start_pipeline() {
    if (pipeline_running_) return;
    
    // Auto-detect number of producers
    size_t num_producers = config.pipelineProducers;
    if (num_producers == 0) {
        num_producers = std::min(
            static_cast<size_t>(std::thread::hardware_concurrency()),
            static_cast<size_t>(32)
        );
    }
    
    std::cout << std::format("Starting two-stage pipeline data loader\n");
    std::cout << std::format("Producer threads: {}\n", num_producers);
    std::cout << std::format("Image queue capacity: {}\n", config.imageQueueSize);
    std::cout << std::format("Batch queue capacity: {}\n", config.batchQueueSize);
    std::cout << std::format("Cache size: {} MB\n", config.cacheMaxSizeMB);
    
    // Create queues
    image_queue_ = std::make_shared<BoundedQueue<LoadedCameraBatch>>(config.imageQueueSize);
    batch_queue_ = std::make_shared<BoundedQueue<LoadedCameraBatch>>(config.batchQueueSize);
    
    // Create cache
    cache_ = std::make_shared<ImageCache>(config.cacheMaxSizeMB, inputData.cameras.size());
    
    // Create per-camera mutexes for thread-safe lazy loading
    camera_mutexes_ = std::make_unique<std::mutex[]>(trainCameras.size());
    
    pipeline_running_ = true;
    
    // Start producer threads (Stage 1: load images)
    for (size_t i = 0; i < num_producers; ++i) {
        producer_threads_.emplace_back([this, i]() {
            this->producer_worker(i);
        });
    }
    
    // Start converter thread (Stage 2: image → tensor)
    converter_thread_ = std::thread([this]() {
        this->converter_worker();
    });
    
    std::cout << "  Pipeline started\n\n";
}

void GaussianInputDataLoader::stop_pipeline() {
    if (!pipeline_running_) return;
    
    std::cout << "\n  Stopping pipeline...\n";
    pipeline_running_ = false;
    
    // Close queues
    if (image_queue_) image_queue_->close();
    if (batch_queue_) batch_queue_->close();
    
    // Wait for all producer threads
    for (auto& thread : producer_threads_) {
        if (thread.joinable()) thread.join();
    }
    
    // Wait for converter thread
    if (converter_thread_.joinable()) {
        converter_thread_.join();
    }
    
    // Print statistics
    if (cache_) {
        auto [cached_count, cache_size_mb] = cache_->stats();
        std::cout << "\n Pipeline statistics\n";
        std::cout << std::format("Total loads: {}\n", total_loads_.load());
        std::cout << std::format("Cache hits: {}\n", cache_hits_.load());
        if (total_loads_ + cache_hits_ > 0) {
            std::cout << std::format("Cache hit rate: {:.1f}%\n",
                100.0 * cache_hits_ / (total_loads_ + cache_hits_));
        }
        std::cout << std::format("Cached cameras: {}/{}\n", 
            cached_count, inputData.cameras.size());
        std::cout << std::format("Cache usage: {} MB\n", cache_size_mb);
    }
    
    std::cout << "Pipeline stopped\n";
}

void GaussianInputDataLoader::producer_worker(size_t worker_id) {
    // Independent RNG for each worker (different seeds)
    std::mt19937 rng(42 + worker_id);
    std::vector<size_t> shuffled_indices;
    
    while (pipeline_running_) {
        // Generate shuffled indices (similar to Rust's shuffle)
        if (shuffled_indices.empty()) {
            shuffled_indices.resize(trainCameras.size());
            std::iota(shuffled_indices.begin(), shuffled_indices.end(), 0);
            std::shuffle(shuffled_indices.begin(), shuffled_indices.end(), rng);
        }
        
        size_t cam_idx = shuffled_indices.back();
        shuffled_indices.pop_back();
        
        Camera* cam_ptr = trainCameras[cam_idx];
        
        // 🔍 Try to get from cache
        auto cached = cache_->try_get(cam_idx);
        bool is_cached = false;
        
        if (cached) {
            // ✅ Cache hit - use cached camera
            cam_ptr = cached.get();
            is_cached = true;
            cache_hits_++;
        } else {
            // ❌ Cache miss - load image lazily (thread-safe)
            try {
                bool did_load = load_camera_lazy(cam_idx);
                if (!cam_ptr->is_loaded_) {
                    // loadImage failed, skip this camera
                    continue;
                }
                if (did_load) {
                    total_loads_++;  // Only count if we actually performed the load
                }
            } catch (const std::exception& e) {
                // log error but continue processing other cameras
                std::cerr << "Failed to load camera " << cam_idx << ": " << e.what() << std::endl;
                continue;
            }
        }
        
        // 📤 Send to Stage 1 queue
        LoadedCameraBatch batch;
        batch.camera_index = cam_idx;
        batch.camera_ptr = cam_ptr;
        batch.is_cached = is_cached;
        
        if (!image_queue_->send(std::move(batch))) {
            break;  // Queue closed
        }
    }
}

void GaussianInputDataLoader::converter_worker() {
    while (pipeline_running_) {
        // 📥 Receive from Stage 1 queue
        auto batch_opt = image_queue_->recv();
        if (!batch_opt) break;  // Queue closed
        
        auto& batch = *batch_opt;
        
        // 🔄 Stage 2: Preprocessing / Transfer to GPU
        // In the current implementation, images are already loaded
        // This stage can be used for additional preprocessing:
        // - Data augmentation
        // - GPU transfer (if not already done)
        // - Format conversion
        // - Normalization
        // For now, we just pass through
        
        // 📤 Send to Stage 2 queue
        if (!batch_queue_->send(std::move(batch))) {
            break;
        }
    }
}

bool GaussianInputDataLoader::load_camera_lazy(size_t cam_idx) {
    // Thread-safe lazy loading with per-camera mutex
    // Returns true if this call actually performed the loading (first loader wins)
    Camera& cam = *trainCameras[cam_idx];
    
    // Quick check without lock (optimization for already-loaded case)
    // Note: Reading bool without lock may see stale value, but:
    // 1. If we see true (even stale), we correctly skip
    // 2. If we see false, we'll acquire lock and double-check
    // 3. Worst case is unnecessary lock acquisition, not data race
    if (cam.is_loaded_) {
        return false;  // Already loaded by another thread
    }
    
    // Acquire per-camera mutex for thread-safe loading
    std::lock_guard<std::mutex> lock(camera_mutexes_[cam_idx]);
    
    // Double-check after acquiring lock (another thread might have loaded it)
    // After mutex lock, we're guaranteed to see the latest value (happens-before)
    if (cam.is_loaded_) {
        return false;  // Another thread loaded it while we were waiting for lock
    }
    
    // Actually load the image - we are the first and only loader
    cam.loadImage(config.maxWidth, 
                config.maxHeight, 
                config.useMask, 
                config.useDepth, 
                config.useNormal);
    ++n_loaded;
    return true;  // We performed the actual loading
}
