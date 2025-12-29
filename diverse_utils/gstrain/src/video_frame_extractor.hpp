#pragma once

#include <string>
#include <vector>
#include <functional>
#include <opencv2/opencv.hpp>
#include <atomic>
#include <memory>

namespace diverse {

// Frame quality metrics
struct FrameQuality {
    int frame_id;
    float sharpness;      // Laplacian variance (higher = sharper)
    float brightness;     // Average brightness
    float contrast;       // Standard deviation
    float motion_blur;    // Motion blur score (lower = less blur)
    float overall_score;  // Combined quality score
    
    cv::Mat frame;        // Optional: store the frame data
};

// Frame selection strategy
enum class FrameSelectionStrategy {
    Uniform,              // Uniform sampling (original behavior)
    QualityBased,         // Select frames based on quality scores
    DiversityBased,       // Maximize frame diversity (different views)
    Hybrid                // Combine quality and diversity
};

// Configuration for frame extraction
struct VideoExtractionConfig {
    int target_fps = 1;                           // Target FPS for extraction
    int max_frames = 300;                         // Maximum frames to extract
    int max_width = 1920;                         // Max image width
    int max_height = 1080;                        // Max image height
    
    // Quality filters
    float min_sharpness = 50.0f;                  // Minimum sharpness threshold
    float min_brightness = 20.0f;                 // Minimum brightness
    float max_brightness = 235.0f;                // Maximum brightness
    int target_candidates = 0;                    // Target candidate count (0=auto: max_frames*3)
    
    // Selection strategy
    FrameSelectionStrategy strategy = FrameSelectionStrategy::Uniform;
    
    // Advanced options
    int num_worker_threads = 8;                   // Number of worker threads
    int jpeg_quality = 95;                        // JPEG compression quality
    bool enable_optimization = true;              // Enable JPEG optimization
    
    // Diversity-based options
    float diversity_threshold = 0.15f;            // Minimum difference for diversity
    int histogram_bins = 64;                      // Histogram bins for comparison
};

// Progress callback
using ProgressCallback = std::function<void(int current, int total, const std::string& status)>;

// Video frame extractor class
class VideoFrameExtractor {
public:
    VideoFrameExtractor(const VideoExtractionConfig& config = VideoExtractionConfig());
    ~VideoFrameExtractor();
    
    // Main extraction method
    bool extract(const std::string& video_path, 
                 const std::string& output_dir,
                 ProgressCallback progress_callback = nullptr);
    
    // Get extraction statistics
    struct Statistics {
        int total_frames;
        int extracted_frames;
        int skipped_blurry;
        int skipped_dark;
        int skipped_similarity;
        float avg_sharpness;
        float processing_time_sec;
    };
    
    const Statistics& getStatistics() const { return stats_; }
    
    // Quality assessment (can be used independently)
    static float computeSharpness(const cv::Mat& image);
    static float computeBrightness(const cv::Mat& image);
    static float computeContrast(const cv::Mat& image);
    static float computeMotionBlur(const cv::Mat& image);
    static float computeFrameSimilarity(const cv::Mat& frame1, const cv::Mat& frame2);
    
private:
    // Frame selection strategies
    std::vector<FrameQuality> selectFramesUniform(cv::VideoCapture& cap, int total_frames, ProgressCallback progress_callback);
    std::vector<FrameQuality> selectFramesQualityBased(cv::VideoCapture& cap, int total_frames, ProgressCallback progress_callback);
    std::vector<FrameQuality> selectFramesDiversityBased(cv::VideoCapture& cap, int total_frames, ProgressCallback progress_callback);
    std::vector<FrameQuality> selectFramesHybrid(cv::VideoCapture& cap, int total_frames, ProgressCallback progress_callback);
    
    // Helper methods
    FrameQuality evaluateFrame(const cv::Mat& frame, int frame_id);
    bool passesQualityFilter(const FrameQuality& quality);
    void saveFrame(const cv::Mat& frame, const std::string& output_path, int index);
    
    VideoExtractionConfig config_;
    Statistics stats_;
    std::atomic<int> processed_count_{0};
};

} // namespace diverse

