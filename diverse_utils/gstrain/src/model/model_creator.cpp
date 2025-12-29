#include "model.hpp"

std::shared_ptr<GaussianTrainModel> createGaussianModel(
        const InputData& inputData, 
        int numCameras,
        GaussianTrainConfig& config,
        const torch::Device& device)
{
    std::shared_ptr<GaussianTrainModel> model;
    model = std::make_shared<GaussianTrainModel>(
        inputData,
        numCameras,
        config,
        device
    );
    std::cout << "select Splat3D model\n";
    model->setDensifyStrategy(config.densifyStrategy);
    return model;
}

std::shared_ptr<GaussianTrainModel> createGaussianModel(
        const std::filesystem::path& filename, 
        const InputData& inputData,
        int numCameras,
        GaussianTrainConfig& config,
        const torch::Device& device)
{
    std::shared_ptr<GaussianTrainModel> model;
    std::cout << "select Splat3D model\n";
    model =  std::make_shared<GaussianTrainModel>(
                std::filesystem::path(config.modelPath),
                inputData,
                numCameras,
                config,
                device);
    model->setDensifyStrategy(config.densifyStrategy);
    return model;
}