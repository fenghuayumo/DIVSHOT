#ifndef CV_UTILS
#define CV_UTILS

#include <torch/torch.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

cv::Mat imreadRGB(const std::string& filename,int flags = 1);
void imwriteRGB(const std::string &filename, const cv::Mat &image);
// Fast image dimension retrieval without decoding the entire image
std::pair<int, int> getImageSize(const std::string& filename);
cv::Mat floatNxNtensorToMat(const torch::Tensor &t);
torch::Tensor floatNxNMatToTensor(const cv::Mat &m);
cv::Mat tensorToImage(const torch::Tensor &t);
torch::Tensor imageToTensor(const cv::Mat &image);
torch::Tensor maskToTensor(const cv::Mat& image);
#endif