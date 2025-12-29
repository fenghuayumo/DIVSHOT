#pragma once
#include <torch/torch.h>
#include <vector>
#include "gsplat.hpp"
struct AdamCustomParamState : public torch::optim::AdamParamState {
    TORCH_ARG(torch::Tensor, step_per_gaussian);
};
class SelectiveAdam : public torch::optim::Adam {
public:
    explicit SelectiveAdam(
        std::vector<torch::optim::OptimizerParamGroup> param_groups,
        torch::optim::AdamOptions option = {}
    ) : torch::optim::Adam(param_groups, option)
    {}
    explicit SelectiveAdam(std::vector<torch::Tensor> params, torch::optim::AdamOptions defaults = {})
        : SelectiveAdam({ torch::optim::OptimizerParamGroup(std::move(params)) }, defaults) {}
    void step(torch::Tensor visibility,bool skip_sh = false,const int num_steps = 30000);
};

class CustomAdam : public torch::optim::Adam {
public:
    explicit CustomAdam(
        std::vector<torch::optim::OptimizerParamGroup> param_groups,
        torch::optim::AdamOptions option = {}
    ) : torch::optim::Adam(param_groups, option)
    {}
    explicit CustomAdam(std::vector<torch::Tensor> params, torch::optim::AdamOptions defaults = {})
        : CustomAdam({ torch::optim::OptimizerParamGroup(std::move(params)) }, defaults) {}
    torch::Tensor step(LossClosure closure = nullptr) override;
};

class FusedAdam : public torch::optim::Adam {
public:
    explicit FusedAdam(
        std::vector<torch::optim::OptimizerParamGroup> param_groups,
        torch::optim::AdamOptions option = {}
    ) : torch::optim::Adam(param_groups, option)
    {}
    explicit FusedAdam(std::vector<torch::Tensor> params, torch::optim::AdamOptions defaults = {})
        : FusedAdam({ torch::optim::OptimizerParamGroup(std::move(params)) }, defaults) {}
    void step(torch::Tensor visibility,bool skip_sh = false,const int num_steps = 30000);
};

class SparseAdam : public torch::optim::Adam {
public:
    explicit SparseAdam(
        std::vector<torch::optim::OptimizerParamGroup> param_groups,
        torch::optim::AdamOptions option = {}
    ) : torch::optim::Adam(param_groups, option), maximize_(false)
    {}
    explicit SparseAdam(std::vector<torch::Tensor> params, torch::optim::AdamOptions defaults = {})
        : SparseAdam({ torch::optim::OptimizerParamGroup(std::move(params)) }, defaults) {}

    // SparseAdam-specific step method (with visibility parameter for Gaussian splatting optimization)
    void step(torch::Tensor visibility, bool skip_sh = false, const int num_steps = 30000);
    
    // Standard step method (without visibility, for general sparse optimization)
    void step();
    
    // Set whether to maximize objective (default is minimize)
    void set_maximize(bool maximize) { maximize_ = maximize; }
    bool get_maximize() const { return maximize_; }

private:
    bool maximize_;
};