#include "quatized_model.hpp"
#include "model.hpp"

extern auto produceClusters(const torch::Tensor& means,
                    const torch::Tensor& scales,
                    const torch::Tensor& rots,
                    const torch::Tensor& opacities,
                    const torch::Tensor& featuresDc,
                    const torch::Tensor& featuresRest,
                    int shDegree,
                    int numClusters) -> std::unordered_map<std::string, tinygsplat::CodeBook>;

// auto produceClusters(GaussianTrainModel* gaussian,int numClusters)->std::unordered_map<std::string, tinygsplat::CodeBook>
// {
//     const auto& trainConfig = gaussian->trainConfig;
//     auto& means = gaussian->means;
//     auto& opacities = gaussian->opacities;
//     auto& featuresDc = gaussian->featuresDc;
//     auto& featuresRest = gaussian->featuresRest;
//     auto& scales = gaussian->scales;
//     auto& quats = gaussian->quats;
//     // auto quats = gaussian->quats / gaussian->quats.norm(2, { -1 }, true);
//     return produceClusters(means, scales, quats, opacities, 
//            featuresDc, featuresRest, trainConfig.shDegree, numClusters);
// }

void QuantizedStrategy::quantized(GaussianTrainModel* gaussian)
{
    
}