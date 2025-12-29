#include "video_frame_extractor.hpp"
#include <utility/thread_pool.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>

// This module uses OpenCV's VideoCapture with FFmpeg backend for video decoding
// FFmpeg provides support for a wide range of video codecs including:
// - H.264/AVC, H.265/HEVC, VP8, VP9, AV1
// - MPEG-4, MPEG-2, MPEG-1
// - ProRes, DNxHD, and many more professional codecs

namespace diverse {

VideoFrameExtractor::VideoFrameExtractor(const VideoExtractionConfig& config)
    : config_(config) {
    stats_ = Statistics{};
}

VideoFrameExtractor::~VideoFrameExtractor() = default;

// ============================================================================
// Quality Assessment Methods
// ============================================================================

float VideoFrameExtractor::computeSharpness(const cv::Mat& image) {
    // Laplacian variance method - higher value = sharper image
    cv::Mat gray, laplacian;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }
    
    cv::Laplacian(gray, laplacian, CV_64F);
    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacian, mean, stddev);
    
    return static_cast<float>(stddev[0] * stddev[0]); // Variance
}

float VideoFrameExtractor::computeBrightness(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }
    
    cv::Scalar mean = cv::mean(gray);
    return static_cast<float>(mean[0]);
}

float VideoFrameExtractor::computeContrast(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }
    
    cv::Scalar mean, stddev;
    cv::meanStdDev(gray, mean, stddev);
    return static_cast<float>(stddev[0]);
}

float VideoFrameExtractor::computeMotionBlur(const cv::Mat& image) {
    // Motion blur detection using edge analysis
    // Lower values indicate less motion blur
    cv::Mat gray, edges;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }
    
    cv::Canny(gray, edges, 50, 150);
    int edge_count = cv::countNonZero(edges);
    float edge_ratio = static_cast<float>(edge_count) / (gray.rows * gray.cols);
    
    // More edges = less motion blur
    return 1.0f - edge_ratio;
}

float VideoFrameExtractor::computeFrameSimilarity(const cv::Mat& frame1, const cv::Mat& frame2) {
    // Histogram-based similarity (0 = identical, 1 = completely different)
    cv::Mat hist1, hist2;
    int histSize[] = {32, 32, 32};
    float range[] = {0, 256};
    const float* ranges[] = {range, range, range};
    int channels[] = {0, 1, 2};
    
    cv::calcHist(&frame1, 1, channels, cv::Mat(), hist1, 3, histSize, ranges, true, false);
    cv::calcHist(&frame2, 1, channels, cv::Mat(), hist2, 3, histSize, ranges, true, false);
    
    cv::normalize(hist1, hist1, 0, 1, cv::NORM_MINMAX);
    cv::normalize(hist2, hist2, 0, 1, cv::NORM_MINMAX);
    
    double similarity = cv::compareHist(hist1, hist2, cv::HISTCMP_CORREL);
    return static_cast<float>(1.0 - similarity); // Convert to dissimilarity
}

// ============================================================================
// Frame Evaluation
// ============================================================================

FrameQuality VideoFrameExtractor::evaluateFrame(const cv::Mat& frame, int frame_id) {
    FrameQuality quality;
    quality.frame_id = frame_id;
    quality.sharpness = computeSharpness(frame);
    quality.brightness = computeBrightness(frame);
    quality.contrast = computeContrast(frame);
    quality.motion_blur = computeMotionBlur(frame);
    
    // Combined quality score (weighted average)
    quality.overall_score = 
        0.5f * (quality.sharpness / 100.0f) +    // Sharpness is most important
        0.2f * (1.0f - quality.motion_blur) +    // Less motion blur is better
        0.15f * (quality.contrast / 50.0f) +     // Good contrast
        0.15f * std::min(1.0f, quality.brightness / 128.0f); // Decent brightness
    
    return quality;
}

bool VideoFrameExtractor::passesQualityFilter(const FrameQuality& quality) {
    // Filter out low-quality frames
    // Note: If min_sharpness is 0, skip sharpness check (accept all)
    if (config_.min_sharpness > 0 && quality.sharpness < config_.min_sharpness) {
        stats_.skipped_blurry++;
        return false;
    }
    
    // Note: If brightness range is [0, 255], skip brightness check (accept all)
    if (config_.min_brightness > 0 || config_.max_brightness < 255) {
        if (quality.brightness < config_.min_brightness || 
            quality.brightness > config_.max_brightness) {
            stats_.skipped_dark++;
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Frame Selection Strategies
// ============================================================================

std::vector<FrameQuality> VideoFrameExtractor::selectFramesUniform(
    cv::VideoCapture& cap, int total_frames, ProgressCallback progress_callback) {
    
    std::vector<FrameQuality> selected;
    int frame_skip = std::max(1, total_frames / config_.max_frames);
    
    int captured = 0;
    int filtered_count = 0;
    
    // Try seeking first, if it fails consistently, fall back to sequential reading
    bool seek_failed = false;
    int consecutive_seek_failures = 0;
    int current_sequential_frame = 0;
    
    while (captured < config_.max_frames) {
        int target_frame = captured * frame_skip;
        
        if (target_frame >= total_frames) break;
        
        cv::Mat frame;
        
        if (!seek_failed) {
            // Try to seek to target frame
            bool seek_ok = cap.set(cv::CAP_PROP_POS_FRAMES, target_frame);
            
            if (seek_ok && cap.read(frame) && !frame.empty()) {
                // Seeking worked!
                consecutive_seek_failures = 0;
            } else {
                // Seeking failed
                consecutive_seek_failures++;
                
                if (consecutive_seek_failures >= 3) {
                    std::cerr << "Warning: Frame seeking not working for this video. Falling back to sequential reading...\n";
                    std::cerr << "  This will be slower but more reliable.\n";
                    seek_failed = true;
                    
                    // Reset video to start for sequential reading
                    cap.set(cv::CAP_PROP_POS_FRAMES, 0);
                    captured = 0;
                    selected.clear();
                    filtered_count = 0;
                    current_sequential_frame = 0;
                    continue;
                }
                
                captured++;
                continue;
            }
        } else {
            // Sequential reading mode (fallback)
            // Read frames sequentially until we reach the target
            while (current_sequential_frame < target_frame) {
                cv::Mat dummy;
                if (!cap.read(dummy)) {
                    std::cerr << "Error: Failed to read frame " << current_sequential_frame << " in sequential mode\n";
                    break;
                }
                current_sequential_frame++;
                
                // Progress update for sequential reading
                if (progress_callback && current_sequential_frame % 100 == 0) {
                    progress_callback(current_sequential_frame, total_frames, "Sequential reading (seeking not supported)");
                }
            }
            
            if (!cap.read(frame) || frame.empty()) {
                std::cerr << "Warning: Failed to read frame " << target_frame << " in sequential mode\n";
                captured++;
                current_sequential_frame++;
                continue;
            }
            current_sequential_frame++;
        }
        
        FrameQuality quality = evaluateFrame(frame, target_frame);
        quality.frame = frame.clone();
        
#ifndef DS_PRODUCTION
        // Debug: Print quality metrics for first few frames
        if (captured < 3 && progress_callback) {
            std::cout << "  Frame " << target_frame << " quality: sharpness=" << quality.sharpness 
                      << ", brightness=" << quality.brightness 
                      << ", contrast=" << quality.contrast 
                      << ", score=" << quality.overall_score << "\n";
        }
#endif
        // Uniform strategy: Accept ALL frames (no quality filtering)
        // This ensures we get exactly the frames at uniform intervals
        selected.push_back(std::move(quality));
        
        if (progress_callback && captured % 10 == 0) {
            progress_callback(captured, config_.max_frames, "Selecting frames (Uniform)");
        }
        
        captured++;
    }
    
    if (progress_callback) {
        progress_callback(config_.max_frames, config_.max_frames, "Frame selection complete");
        std::cout << "  Selected: " << selected.size() << " frames, Filtered: " << filtered_count << " frames\n";
        
        if (selected.empty() && filtered_count > 0) {
            std::cout << "  WARNING: All frames were filtered out! Consider:\n";
            std::cout << "    - Lowering min_sharpness (current: " << config_.min_sharpness << ")\n";
            std::cout << "    - Adjusting brightness range (current: [" << config_.min_brightness 
                      << ", " << config_.max_brightness << "])\n";
        }
    }
    
    return selected;
}

std::vector<FrameQuality> VideoFrameExtractor::selectFramesQualityBased(
    cv::VideoCapture& cap, int total_frames, ProgressCallback progress_callback) {
    
    // Note: This strategy evaluates ALL frames, which can be slow for long videos
    // Consider using Hybrid strategy for better performance
    
    if (progress_callback && total_frames > 1000) {
        std::cout << "  Warning: Quality-based strategy will evaluate all " << total_frames 
                  << " frames. This may take a while...\n";
        std::cout << "  Tip: Use Hybrid strategy for faster processing on long videos.\n";
    }
    
    // First pass: evaluate all frames
    std::vector<FrameQuality> all_qualities;
    cv::Mat frame;
    
    for (int frame_id = 0; frame_id < total_frames; frame_id++) {
        // Seek to frame (more reliable than sequential read for some codecs)
        cap.set(cv::CAP_PROP_POS_FRAMES, frame_id);
        
        if (!cap.read(frame) || frame.empty()) {
            continue;
        }
        
        FrameQuality quality = evaluateFrame(frame, frame_id);
        quality.frame = frame.clone();
        
        if (passesQualityFilter(quality)) {
            all_qualities.push_back(std::move(quality));
        }
        
        processed_count_++;
        
        if (progress_callback && frame_id % 50 == 0) {
            progress_callback(frame_id, total_frames, "Evaluating frame quality");
        }
    }
    
    // Sort by quality score (descending)
    std::sort(all_qualities.begin(), all_qualities.end(),
        [](const FrameQuality& a, const FrameQuality& b) {
            return a.overall_score > b.overall_score;
        });
    
    // Select top N frames
    std::vector<FrameQuality> selected;
    int count = std::min(config_.max_frames, static_cast<int>(all_qualities.size()));
    for (int i = 0; i < count; i++) {
        selected.push_back(std::move(all_qualities[i]));
    }
    
    // Re-sort by frame_id for sequential output
    std::sort(selected.begin(), selected.end(),
        [](const FrameQuality& a, const FrameQuality& b) {
            return a.frame_id < b.frame_id;
        });
    
    return selected;
}

std::vector<FrameQuality> VideoFrameExtractor::selectFramesDiversityBased(
    cv::VideoCapture& cap, int total_frames, ProgressCallback progress_callback) {
    
    std::vector<FrameQuality> candidates;
    cv::Mat frame;
    
    // Collect candidate frames
    int sample_rate = std::max(1, total_frames / (config_.max_frames * 3));
    int samples_to_check = total_frames / sample_rate;
    
    if (progress_callback) {
        std::cout << "  Sample rate: 1/" << sample_rate << "\n";
    }
    
    for (int i = 0; i < samples_to_check; i++) {
        int frame_id = i * sample_rate;
        
        if (frame_id >= total_frames) break;
        
        // Seek to target frame
        cap.set(cv::CAP_PROP_POS_FRAMES, frame_id);
        
        if (!cap.read(frame) || frame.empty()) {
            continue;
        }
        
        FrameQuality quality = evaluateFrame(frame, frame_id);
        
        if (passesQualityFilter(quality)) {
            quality.frame = frame.clone();
            candidates.push_back(std::move(quality));
        }
        
        if (progress_callback && i % 10 == 0) {
            progress_callback(i, samples_to_check, "Collecting candidate frames");
        }
    }
    
    if (candidates.empty()) return {};
    
    // Greedy diversity selection
    std::vector<FrameQuality> selected;
    
    // Start with the highest quality frame
    auto best_it = std::max_element(candidates.begin(), candidates.end(),
        [](const FrameQuality& a, const FrameQuality& b) {
            return a.overall_score < b.overall_score;
        });
    
    selected.push_back(std::move(*best_it));
    candidates.erase(best_it);
    
    // Iteratively add most diverse frames
    while (selected.size() < static_cast<size_t>(config_.max_frames) && !candidates.empty()) {
        float max_min_distance = -1.0f;
        size_t best_idx = 0;
        
        for (size_t i = 0; i < candidates.size(); i++) {
            // Find minimum distance to selected frames
            float min_distance = std::numeric_limits<float>::max();
            
            for (const auto& sel : selected) {
                float distance = computeFrameSimilarity(candidates[i].frame, sel.frame);
                min_distance = std::min(min_distance, distance);
            }
            
            if (min_distance > max_min_distance) {
                max_min_distance = min_distance;
                best_idx = i;
            }
        }
        
        if (max_min_distance < config_.diversity_threshold) {
            stats_.skipped_similarity++;
            break; // No more diverse frames
        }
        
        selected.push_back(std::move(candidates[best_idx]));
        candidates.erase(candidates.begin() + best_idx);
        
        if (progress_callback) {
            progress_callback(selected.size(), config_.max_frames, "Selecting diverse frames");
        }
    }
    
    return selected;
}

std::vector<FrameQuality> VideoFrameExtractor::selectFramesHybrid(
    cv::VideoCapture& cap, int total_frames, ProgressCallback progress_callback) {
    
    // Phase 1: Collect high-quality candidate frames
    if (progress_callback) {
        progress_callback(0, total_frames, "Phase 1/2: Scanning video for quality");
    }
    
    std::vector<FrameQuality> candidates;
    cv::Mat frame;
    int frame_id = 0;
    
    // Calculate sample rate based on target candidates
    // Target: collect enough candidates for diversity selection
    int target_candidates = config_.target_candidates > 0 ? 
        config_.target_candidates : (config_.max_frames * 3);
    
    // Sample more frames than target to account for quality filtering
    // Assume ~50% pass rate, so sample 2x more
    int sample_rate = std::max(1, total_frames / (target_candidates * 2));
    
    if (progress_callback) {
        std::cout << "  Sample rate: 1/" << sample_rate << " (checking every " << sample_rate << "th frame)\n";
        std::cout << "  Target candidates: ~" << target_candidates << "\n";
    }
    
    // Use frame seeking for better performance
    int samples_to_check = total_frames / sample_rate;
    for (int i = 0; i < samples_to_check && frame_id < total_frames; i++) {
        frame_id = i * sample_rate;
        
        // Seek to target frame
        cap.set(cv::CAP_PROP_POS_FRAMES, frame_id);
        
        if (!cap.read(frame) || frame.empty()) {
            continue;
        }
        
        FrameQuality quality = evaluateFrame(frame, frame_id);
        
        if (passesQualityFilter(quality)) {
            quality.frame = frame.clone();
            candidates.push_back(std::move(quality));
        }
        
        // Progress update every 10 samples
        if (progress_callback && i % 10 == 0) {
            progress_callback(i, samples_to_check, 
                std::string("Phase 1/2: Scanning video (") + 
                std::to_string(candidates.size()) + " candidates found)");
        }
    }
    
    if (candidates.empty()) {
        std::cerr << "Warning: No candidates found! All frames filtered out.\n";
        std::cerr << "  Suggestion: Lower min_sharpness threshold (current: " 
                  << config_.min_sharpness << ")\n";
        return {};
    }
    
    // Report candidate collection results
    if (progress_callback) {
        int sampled_frames = total_frames / std::max(1, sample_rate);
        float pass_rate = (candidates.size() * 100.0f) / std::max(1, sampled_frames);
        std::cout << "  Collected " << candidates.size() << " candidates from " 
                  << sampled_frames << " sampled frames (pass rate: " 
                  << std::fixed << std::setprecision(1) << pass_rate << "%)\n";
        
        if (candidates.size() < static_cast<size_t>(target_candidates / 2)) {
            std::cout << "  Warning: Candidate count is low (target: ~" << target_candidates << "). Consider:\n";
            std::cout << "     - Lowering min_sharpness (current: " << config_.min_sharpness << ")\n";
            std::cout << "     - Adjusting brightness thresholds\n";
        }
    }
    
    // Sort by quality, take top candidates
    // Use smaller multiplier for large frame counts to reduce O(K²×C) complexity
    std::sort(candidates.begin(), candidates.end(),
        [](const FrameQuality& a, const FrameQuality& b) {
            return a.overall_score > b.overall_score;
        });
    
    // Adaptive candidate pool size: smaller ratio for larger targets
    float multiplier = config_.max_frames <= 100 ? 2.0f : 
                      config_.max_frames <= 300 ? 1.5f : 1.2f;
    int top_n = std::min(static_cast<int>(config_.max_frames * multiplier), 
                        static_cast<int>(candidates.size()));
    candidates.resize(top_n);
    
    if (progress_callback) {
        float estimated_comparisons = config_.max_frames * top_n;
        std::cout << "  Estimated comparisons: ~" << static_cast<int>(estimated_comparisons) 
                  << " (target: " << config_.max_frames << " frames from " << top_n << " candidates)\n";
    }
    
    // Phase 2: Apply diversity selection on top candidates
    if (progress_callback) {
        progress_callback(0, config_.max_frames, 
            std::string("Phase 2/2: Selecting diverse views from ") + 
            std::to_string(top_n) + " candidates");
    }
    
    std::vector<FrameQuality> selected;
    
    selected.push_back(std::move(candidates[0]));
    candidates.erase(candidates.begin());
    
    while (selected.size() < static_cast<size_t>(config_.max_frames) && !candidates.empty()) {
        float max_combined_score = -1.0f;
        size_t best_idx = 0;
        
        for (size_t i = 0; i < candidates.size(); i++) {
            float min_similarity = std::numeric_limits<float>::max();
            
            for (const auto& sel : selected) {
                float distance = computeFrameSimilarity(candidates[i].frame, sel.frame);
                min_similarity = std::min(min_similarity, 1.0f - distance);
            }
            
            // Combined score: quality + diversity
            float combined = 0.6f * candidates[i].overall_score + 
                           0.4f * (1.0f - min_similarity);
            
            if (combined > max_combined_score) {
                max_combined_score = combined;
                best_idx = i;
            }
        }
        
        selected.push_back(std::move(candidates[best_idx]));
        candidates.erase(candidates.begin() + best_idx);
        
        // Progress update (this is the slow part, so update every frame)
        if (progress_callback) {
            progress_callback(selected.size(), config_.max_frames, 
                "Phase 2/2: Maximizing view diversity");
        }
    }
    
    return selected;
}

// ============================================================================
// Main Extraction Method
// ============================================================================

bool VideoFrameExtractor::extract(const std::string& video_path,
                                   const std::string& output_dir,
                                   ProgressCallback progress_callback) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Reset statistics
    stats_ = Statistics{};
    processed_count_ = 0;
    
    // Create output directory
    std::filesystem::create_directories(output_dir);
    
    // Open video with FFmpeg backend
    // CAP_FFMPEG explicitly requests FFmpeg backend for better codec support
    cv::VideoCapture cap(video_path, cv::CAP_FFMPEG);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open video: " << video_path << std::endl;
        std::cerr << "  Tip: Make sure FFmpeg backend is available in OpenCV build" << std::endl;
        return false;
    }
    
    // Log video information
    if (progress_callback) {
        double fps = cap.get(cv::CAP_PROP_FPS);
        int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        
        // Extract codec FourCC
        std::string codec_fourcc;
        int fourcc = static_cast<int>(cap.get(cv::CAP_PROP_FOURCC));
        if (fourcc > 0) {
            codec_fourcc += static_cast<char>(fourcc & 0xFF);
            codec_fourcc += static_cast<char>((fourcc >> 8) & 0xFF);
            codec_fourcc += static_cast<char>((fourcc >> 16) & 0xFF);
            codec_fourcc += static_cast<char>((fourcc >> 24) & 0xFF);
        } else {
            codec_fourcc = "Unknown";
        }
        
        std::cout << "Video Info:\n";
        std::cout << "  Resolution: " << width << "x" << height << "\n";
        std::cout << "  FPS: " << fps << "\n";
        std::cout << "  Codec: " << codec_fourcc << "\n";
        
        // Try to get backend name (may not be available in all OpenCV versions)
        try {
            std::string backend_name = cap.getBackendName();
            std::cout << "  Backend: " << backend_name << "\n";
        } catch (...) {
            std::cout << "  Backend: FFmpeg (requested)\n";
        }
    }
    
    int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    stats_.total_frames = total_frames;
    
    if (progress_callback) {
        progress_callback(0, total_frames, "Starting frame extraction");
    }
    
    // Select frames based on strategy
    std::vector<FrameQuality> selected_frames;
    
    switch (config_.strategy) {
        case FrameSelectionStrategy::Uniform:
            selected_frames = selectFramesUniform(cap, total_frames, progress_callback);
            break;
        case FrameSelectionStrategy::QualityBased:
            selected_frames = selectFramesQualityBased(cap, total_frames, progress_callback);
            break;
        case FrameSelectionStrategy::DiversityBased:
            selected_frames = selectFramesDiversityBased(cap, total_frames, progress_callback);
            break;
        case FrameSelectionStrategy::Hybrid:
            selected_frames = selectFramesHybrid(cap, total_frames, progress_callback);
            break;
    }
    
    cap.release();
    
    if (selected_frames.empty()) {
        std::cerr << "No frames selected" << std::endl;
        return false;
    }
    
    // Save selected frames (multi-threaded)
    ThreadPool pool(config_.num_worker_threads);
    std::vector<std::future<void>> futures;
    std::atomic<int> saved_count{0};
    
    for (size_t i = 0; i < selected_frames.size(); i++) {
        futures.push_back(pool.enqueue_task([&, i, this]() {
            const auto& quality = selected_frames[i];
            
            // Resize if needed
            cv::Mat processed = quality.frame.clone();
            float aspect_ratio = static_cast<float>(processed.cols) / processed.rows;
            
            if (processed.cols > config_.max_width || processed.rows > config_.max_height) {
                int new_width = config_.max_width;
                int new_height = static_cast<int>(new_width / aspect_ratio);
                
                if (new_height > config_.max_height) {
                    new_height = config_.max_height;
                    new_width = static_cast<int>(new_height * aspect_ratio);
                }
                
                cv::resize(processed, processed, cv::Size(new_width, new_height), 
                          0, 0, cv::INTER_AREA);
            }
            
            // Generate filename
            std::stringstream ss;
            ss << output_dir << "/frame_" << std::setw(4) << std::setfill('0') << i 
               << "_q" << std::fixed << std::setprecision(2) << quality.overall_score 
               << ".jpg";
            
            // Save with optimization
            std::vector<int> params = {
                cv::IMWRITE_JPEG_QUALITY, config_.jpeg_quality
            };
            if (config_.enable_optimization) {
                params.push_back(cv::IMWRITE_JPEG_OPTIMIZE);
                params.push_back(1);
            }
            
            cv::imwrite(ss.str(), processed, params);
            
            int count = ++saved_count;
            if (progress_callback && count % 10 == 0) {
                progress_callback(count, selected_frames.size(), "Saving frames");
            }
        }));
    }
    
    wait_all(futures);
    
    // Calculate statistics
    stats_.extracted_frames = static_cast<int>(selected_frames.size());
    
    float total_sharpness = 0.0f;
    for (const auto& quality : selected_frames) {
        total_sharpness += quality.sharpness;
    }
    stats_.avg_sharpness = total_sharpness / selected_frames.size();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.processing_time_sec = std::chrono::duration<float>(end_time - start_time).count();
    
    if (progress_callback) {
        progress_callback(stats_.extracted_frames, stats_.extracted_frames, "Complete");
    }
    
    return true;
}

} // namespace diverse

