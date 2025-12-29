#ifndef MCMC_MODEL_HPP
#define MCMC_MODEL_HPP

#include "model.hpp"
/* Implement MCMC Splat Model form 3D Gaussian Splatting as Markov Chain Monte Carlo <https://arxiv.org/abs/2404.09591>`
*/
class MCMCDensify : public DensifyStrategy
{
public:
    MCMCDensify(GaussianTrainModel* model) : DensifyStrategy(model) 
    {
        type = SplatDensifyType::SplatMCMC;
    }
    void resetState() override;
    void stepAfterbackward(int step, std::unordered_map<std::string_view, torch::Tensor>& infos) override;

    torch::Tensor binoms;
};

#endif