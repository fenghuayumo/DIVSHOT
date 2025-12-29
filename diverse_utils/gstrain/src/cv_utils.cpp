#include "cv_utils.hpp"

// Fast image dimension retrieval without decoding full image data
std::pair<int, int> getImageSize(const std::string& filename) {
    // OpenCV decodes image header to get dimensions without decoding full image data
    // This is much faster than fully loading the image
    cv::Mat img = cv::imread(filename, cv::IMREAD_REDUCED_COLOR_8);
    if (img.empty()) {
        // Fallback to standard method if fast method fails
        img = cv::imread(filename, cv::IMREAD_ANYCOLOR);
    }
    return std::make_pair(img.cols, img.rows);
}

cv::Mat imreadRGB(const std::string& filename,int flags) {
    cv::Mat cImg = cv::imread(filename, flags);
    if (cImg.channels() == 4)
        cv::cvtColor(cImg, cImg, cv::COLOR_BGRA2RGBA);
    else if (cImg.channels() == 3)
        cv::cvtColor(cImg, cImg, cv::COLOR_BGR2RGB);
    // else if( cImg.channels() == 1)
    //    cv::cvtColor(cImg, cImg, cv::COLOR_GRAY);
    return cImg;
}

void imwriteRGB(const std::string &filename, const cv::Mat &image){
    cv::Mat rgb;
    cv::cvtColor(image, rgb, cv::COLOR_RGB2BGR);
    cv::imwrite(filename, rgb);
}

cv::Mat floatNxNtensorToMat(const torch::Tensor &t){
    return cv::Mat(t.size(0), t.size(1), CV_32F, t.data_ptr());
}

torch::Tensor floatNxNMatToTensor(const cv::Mat &m){
    return torch::from_blob(m.data, { m.rows, m.cols }, torch::kFloat32).clone();
}

cv::Mat tensorToImage(const torch::Tensor &t){
    int h = t.sizes()[0];
    int w = t.sizes()[1];
    int c = t.sizes()[2];

    int type = CV_8UC3;
    if( c == 1){
        type = CV_8UC1;
    }
    // else if( c != 3){
    //     throw std::runtime_error("Only images with 3 channels are supported");
    // }

    cv::Mat image(h, w, type);
    torch::Tensor scaledTensor = (t * 255.0).toType(torch::kU8);
    uint8_t* dataPtr = static_cast<uint8_t*>(scaledTensor.data_ptr());
    std::copy(dataPtr, dataPtr + (w * h * c), image.data);

    return image;
}

torch::Tensor imageToTensor(const cv::Mat &image){
    if( image.channels() == 4){
        cv::Mat cImage;
        cv::cvtColor(image, cImage, cv::COLOR_RGBA2RGB);
        torch::Tensor img = torch::from_blob(cImage.data, { cImage.rows, cImage.cols, 3 }, torch::kU8);
        return (img.toType(torch::kFloat32) / 255.0f);
    }
    else if(image.channels() == 3)
    { 
        torch::Tensor img = torch::from_blob(image.data, { image.rows, image.cols, image.channels() }, torch::kU8);
        return (img.toType(torch::kFloat32) / 255.0f);
    }
    // chanels 1
    torch::Tensor img = torch::from_blob(image.data, { image.rows, image.cols, 1 }, torch::kU8);
    return (img.toType(torch::kFloat32) / 255.0f);
}

torch::Tensor maskToTensor(const cv::Mat& image) {
    torch::Tensor img = torch::from_blob(image.data, { image.rows, image.cols, 1 }, torch::kU8);
    return img.toType(torch::kBool);
}
