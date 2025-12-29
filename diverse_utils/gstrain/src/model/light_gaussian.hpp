#ifndef LIGHT_GS_HPP
#define LIGHT_GS_HPP

#include "model.hpp"
/* Implement Light Gaussian Prune Strategy form https://arxiv.org/abs/2311.17245
*/
class LightSplatModel : public PruneStrategy
{
public:
    LightSplatModel(GaussianTrainModel* model) : PruneStrategy(model){}
    auto prune(std::vector<Camera>& cam, int step) -> void override;
protected:
    auto calculateVolumeImportanceScore(torch::Tensor importance_list, float v_pow)->torch::Tensor;
    auto pruneList(std::vector<Camera>& cam, int step)->std::tuple<torch::Tensor,torch::Tensor>;
    auto pruneGaussians(float percent, const torch::Tensor& importance_score)->void;
private:
    float prunePercent = 0.25f;
    int   pruneIter = 0;
};

#endif