#ifndef DATA_LOADER_UTILS
#define DATA_LOADER_UTILS

#include "input_data.hpp"
#include "utils.hpp"
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <optional>

struct DataLoaderConfig
{
    int maxWidth = 4096; int maxHeight = 4096;
    bool batchLoader = false;
    uint32_t batchSize = 128;
    bool useMask = false;
    bool useNormal = false;
    bool useDepth = false;
    int  datasetType = -1;
    
    // Two-stage pipeline configuration (Brush-style)
    bool enablePipeline = true;          // Enable 2-stage pipeline
    size_t pipelineProducers = 0;         // Number of producer threads (0 = auto-detect)
    size_t imageQueueSize = 32;           // Stage 1 queue capacity
    size_t batchQueueSize = 2;            // Stage 2 queue capacity
    size_t cacheMaxSizeMB = 24 * 1024;     // Cache size limit (6GB default)
};

// ============================================================================
// Bounded Queue - similar to Rust's mpsc::channel(capacity)
// ============================================================================
template<typename T>
class BoundedQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_producer_;
    std::condition_variable cv_consumer_;
    size_t max_size_;
    std::atomic<bool> closed_{false};

public:
    explicit BoundedQueue(size_t max_size) : max_size_(max_size) {}

    // Send data (blocks if full) - producer interface
    bool send(T&& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_producer_.wait(lock, [this] {
            return queue_.size() < max_size_ || closed_;
        });
        if (closed_) return false;
        queue_.push(std::move(value));
        cv_consumer_.notify_one();
        return true;
    }

    // Receive data (blocks if empty) - consumer interface
    std::optional<T> recv() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_consumer_.wait(lock, [this] {
            return !queue_.empty() || closed_;
        });
        if (queue_.empty()) return std::nullopt;
        T value = std::move(queue_.front());
        queue_.pop();
        cv_producer_.notify_one();
        return value;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_producer_.notify_all();
        cv_consumer_.notify_all();
    }

    bool is_closed() const { return closed_; }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
};

// ============================================================================
// Image Cache - LRU-style cache with capacity limit
// ============================================================================
class ImageCache {
private:
    std::vector<std::shared_ptr<Camera>> cache_;
    mutable std::mutex mutex_;
    size_t max_size_mb_;
    size_t current_size_mb_{0};

public:
    ImageCache(size_t max_size_mb, size_t num_images) 
        : max_size_mb_(max_size_mb), cache_(num_images) {}

    std::shared_ptr<Camera> try_get(size_t index) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_[index];
    }

    void insert(size_t index, std::shared_ptr<Camera> cam) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cache_[index] != nullptr) return;
        
        // Estimate size in MB (rough estimate)
        size_t cam_size_mb = (cam->width * cam->height * 4) / (1024 * 1024);
        if (current_size_mb_ + cam_size_mb < max_size_mb_) {
            cache_[index] = cam;
            current_size_mb_ += cam_size_mb;
        }
    }

    std::pair<size_t, size_t> stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t cached_count = 0;
        for (const auto& item : cache_) {
            if (item != nullptr) cached_count++;
        }
        return {cached_count, current_size_mb_};
    }
};

// ============================================================================
// Loaded Camera Batch - data passed through pipeline
// ============================================================================
struct LoadedCameraBatch {
    size_t camera_index;
    Camera* camera_ptr;
    bool is_cached;
};

class GaussianInputDataLoader
{
public:
    GaussianInputDataLoader(const DataLoaderConfig& config);
    ~GaussianInputDataLoader();
    
    void load(const std::string& imagePath,const std::string& posePath="",const std::string& pointPath="",int validateNum = 0);
    void load(const std::string& imagePath,
        std::vector<colmap::SparsePoint>&& points, 
        std::vector<colmap::CameraTrack>&& cameras, 
        std::vector<colmap::ImageTrack>&& imgfs,
        const std::string& posePath="",
        const std::string& pointPath="",
        int validateNum = 0);
    Camera& next();
    std::pair<size_t,Camera&> next_pair();
    
    float load_progress();
    std::atomic<int> n_loaded = 0;
    InputData inputData;
    std::vector<Camera*>    trainCameras;
    std::vector<Camera*>    validateCameras;
    std::unique_ptr<InfiniteRandomIterator<size_t>> camsIter;
    float   sceneExtent = 1.0f;
    DataLoaderConfig    config;

private:
    // Two-stage pipeline components
    std::shared_ptr<BoundedQueue<LoadedCameraBatch>> image_queue_;
    std::shared_ptr<BoundedQueue<LoadedCameraBatch>> batch_queue_;
    std::shared_ptr<ImageCache> cache_;
    std::vector<std::thread> producer_threads_;
    std::thread converter_thread_;
    std::atomic<bool> pipeline_running_{false};
    std::atomic<size_t> total_loads_{0};
    std::atomic<size_t> cache_hits_{0};
    
    // Pipeline methods
    void start_pipeline();
    void stop_pipeline();
    void producer_worker(size_t worker_id);
    void converter_worker();
    bool load_camera_lazy(size_t cam_idx);  // Thread-safe loading, returns true if this call actually loaded
    
    // Per-camera mutex for thread-safe lazy loading
    std::unique_ptr<std::mutex[]> camera_mutexes_;
};
    
#endif