#include "selective_adam.hpp"


void SelectiveAdam::step(torch::Tensor visibility,bool skip_sh,const int num_steps) {
    torch::NoGradGuard nograd;
    const int stopStep = num_steps * 2 / 3;
    for (auto& group : param_groups()) {
        auto* adam_options = dynamic_cast<torch::optim::AdamOptions*>(&group.options());
        auto& param = group.params()[0];
        auto lr = adam_options->get_lr();
        auto eps = adam_options->eps();
        auto [beta1,beta2] = adam_options->betas();
        if(!param.grad().defined() || param.grad().numel() <= 0 ) continue;
#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
        auto pId = param.unsafeGetTensorImpl();
#else
        auto pId = c10::guts::to_string(param.unsafeGetTensorImpl());
#endif
        auto& state_ = state();
        auto state_ptr = state_.find(pId);
        if (state_ptr == state_.end()) {
            auto paramState = std::make_unique<AdamCustomParamState>();
            paramState->exp_avg() = torch::zeros_like(param,torch::MemoryFormat::Preserve);
            paramState->exp_avg_sq() = torch::zeros_like(param,torch::MemoryFormat::Preserve);
            paramState->step() = 0;
            state_[pId] = std::move(paramState);
            state_ptr = state_.find(pId);
        }
        auto& stored_state = static_cast<AdamCustomParamState&>(*state_ptr->second);
        auto& exp_avg = stored_state.exp_avg();
        auto& exp_avg_sq = stored_state.exp_avg_sq();
        auto& state_step = stored_state.step();
        state_step++;
        if(skip_sh && state_step <= 1000)
            continue;
        if(skip_sh && state_step % 2 != 0 && state_step <= stopStep)
            continue;

        auto bias_correction1_rcp = 1.0 / (1.0 - std::pow(beta1, state_step));
        auto bias_correction2_sqrt_rcp = 1.0 / std::sqrt(1.0 - std::pow(beta2, state_step));
        gsplat::adam_step(
            param,
            exp_avg,
            exp_avg_sq,
            param.grad(),
            static_cast<float>(lr),
            static_cast<float>(beta1),
            static_cast<float>(beta2),
            static_cast<float>(eps),
            static_cast<float>(bias_correction1_rcp),
            static_cast<float>(bias_correction2_sqrt_rcp));
    }
}

torch::Tensor CustomAdam::step(LossClosure closure) {
    torch::NoGradGuard nograd;
    for (auto& group : param_groups()) {
        auto* adam_options = dynamic_cast<torch::optim::AdamOptions*>(&group.options());
        auto& param = group.params()[0];
        auto lr = adam_options->get_lr();
        auto eps = adam_options->eps();
        auto [beta1, beta2] = adam_options->betas();
        if (!param.grad().defined() || param.grad().numel() <= 0) continue;
#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
        auto pId = param.unsafeGetTensorImpl();
#else
        auto pId = c10::guts::to_string(param.unsafeGetTensorImpl());
#endif
        if (!state()[pId]) {
            auto paramState = std::make_unique<AdamCustomParamState>();
            paramState->exp_avg() = torch::zeros_like(param);
            paramState->exp_avg_sq() = torch::zeros_like(param);
            paramState->step() = 0;
            state()[pId] = std::move(paramState);
        }
        return torch::optim::Adam::step(closure);
    }
    return torch::Tensor();
}

void FusedAdam::step(torch::Tensor visibility,bool skip_sh,const int num_steps) {
    torch::NoGradGuard nograd;
    const int stopStep = num_steps * 2 / 3;
    auto adam_options = static_cast<torch::optim::AdamOptions*>(&defaults());
    for (auto& group : param_groups()) {
        auto& param = group.params()[0];
        auto lr = adam_options->get_lr();
        auto eps = adam_options->eps();
        auto [beta1,beta2] = adam_options->betas();
        if(!param.grad().defined() || param.grad().numel() <= 0 ) continue;
#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
        auto pId = param.unsafeGetTensorImpl();
#else
        auto pId = c10::guts::to_string(param.unsafeGetTensorImpl());
#endif
        auto& state_ = state();
        auto state_ptr = state_.find(pId);
        if (state_ptr == state_.end()) {
            auto paramState = std::make_unique<AdamCustomParamState>();
            paramState->exp_avg() = torch::zeros_like(param,torch::MemoryFormat::Preserve);
            paramState->exp_avg_sq() = torch::zeros_like(param,torch::MemoryFormat::Preserve);
            paramState->step() = 0;
            state_[pId] = std::move(paramState);
            state_ptr = state_.find(pId);
        }
        auto& stored_state = static_cast<AdamCustomParamState&>(*state_ptr->second);
        auto& exp_avg = stored_state.exp_avg();
        auto& exp_avg_sq = stored_state.exp_avg_sq();
        auto& state_step = stored_state.step();
        state_step++;
        if(skip_sh && state_step <= 1000)
            continue;
        if(skip_sh && state_step % 2 != 0 && state_step <= stopStep)
            continue;

        auto bias_correction1_rcp = 1.0 / (1.0 - std::pow(beta1, state_step));
        auto bias_correction2_sqrt_rcp = 1.0 / std::sqrt(1.0 - std::pow(beta2, state_step));
        gsplat::adam_step(
            param,
            exp_avg,
            exp_avg_sq,
            param.grad(),
            static_cast<float>(lr),
            static_cast<float>(beta1),
            static_cast<float>(beta2),
            static_cast<float>(eps),
            static_cast<float>(bias_correction1_rcp),
            static_cast<float>(bias_correction2_sqrt_rcp));
    }
}

void SparseAdam::step(torch::Tensor visibility,bool skip_sh,const int num_steps) {
    torch::NoGradGuard nograd;
    
    for (auto& group : param_groups()) {
        auto* adam_options = dynamic_cast<torch::optim::AdamOptions*>(&group.options());
        double lr = adam_options->get_lr();
        double eps = adam_options->eps();
        auto [beta1, beta2] = adam_options->betas();
        bool maximize = maximize_; // Use member variable
        
        for (auto& param : group.params()) {
            // Check if gradient exists
            if (!param.grad().defined() || param.grad().numel() == 0) {
                continue;
            }
            
            auto grad = param.grad();
            
            // Negate gradient if maximizing
            if (maximize) {
                grad = -grad;
            }
            
            // Coalesce sparse gradient to ensure unique indices
            if (grad.is_sparse()) {
                grad = grad.coalesce();
            }
            
            // Get parameter ID
#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
            auto pId = param.unsafeGetTensorImpl();
#else
            auto pId = c10::guts::to_string(param.unsafeGetTensorImpl());
#endif
            
            // Initialize or get state
            // Use AdamCustomParamState for compatibility with addToOptimizer/removeFromOptimizer
            auto& state_ = state();
            auto state_ptr = state_.find(pId);
            if (state_ptr == state_.end()) {
                auto paramState = std::make_unique<AdamCustomParamState>();
                // Initialize momentum with same layout as parameter
                paramState->exp_avg() = torch::zeros_like(param, torch::MemoryFormat::Preserve);
                paramState->exp_avg_sq() = torch::zeros_like(param, torch::MemoryFormat::Preserve);
                paramState->step_per_gaussian() = torch::zeros_like(param, torch::MemoryFormat::Preserve);
                paramState->step() = 0;
                state_[pId] = std::move(paramState);
                state_ptr = state_.find(pId);
            }
            
            auto& stored_state = static_cast<AdamCustomParamState&>(*state_ptr->second);
            auto& exp_avg = stored_state.exp_avg();
            auto& exp_avg_sq = stored_state.exp_avg_sq();
            auto& state_step = stored_state.step();
            
            // Increment step
            state_step++;
            
            // Handle sparse gradients with CUDA acceleration
            if (grad.is_sparse()) {
                auto grad_indices = grad._indices();
                auto grad_values = grad._values();
                
                // Skip update if gradient values are empty
                if (grad_values.numel() == 0) {
                    continue;
                }
                
                // Ensure grad_indices is 1D (squeeze if needed)
                if (grad_indices.dim() > 1) {
                    grad_indices = grad_indices.squeeze(0);
                }
                
                // Compute bias correction
                double bias_correction1 = 1.0 - std::pow(beta1, state_step);
                double bias_correction2 = 1.0 - std::pow(beta2, state_step);
                float bias_correction1_rcp = static_cast<float>(1.0 / bias_correction1);
                float bias_correction2_sqrt_rcp = static_cast<float>(1.0 / std::sqrt(bias_correction2));
                
                // Use CUDA kernel for fast sparse update
                gsplat::launch_sparse_adam_kernel(
                    param,
                    exp_avg,
                    exp_avg_sq,
                    grad_indices,
                    grad_values,
                    static_cast<float>(lr),
                    static_cast<float>(beta1),
                    static_cast<float>(beta2),
                    static_cast<float>(eps),
                    bias_correction1_rcp,
                    bias_correction2_sqrt_rcp
                );
                
            } else {
                // For dense gradients, fallback to gsplat::adam_step for acceleration
                double bias_correction1 = 1.0 - std::pow(beta1, state_step);
                double bias_correction2 = 1.0 - std::pow(beta2, state_step);
                float bias_correction1_rcp = static_cast<float>(1.0 / bias_correction1);
                float bias_correction2_sqrt_rcp = static_cast<float>(1.0 / std::sqrt(bias_correction2));
                
                gsplat::adam_step(
                    param,
                    exp_avg,
                    exp_avg_sq,
                    grad,
                    static_cast<float>(lr),
                    static_cast<float>(beta1),
                    static_cast<float>(beta2),
                    static_cast<float>(eps),
                    bias_correction1_rcp,
                    bias_correction2_sqrt_rcp
                );
            }
        }
    }
}

// Standard step method (without visibility parameter)
void SparseAdam::step() {
    torch::NoGradGuard nograd;
    
    for (auto& group : param_groups()) {
        auto* adam_options = dynamic_cast<torch::optim::AdamOptions*>(&group.options());
        double lr = adam_options->get_lr();
        double eps = adam_options->eps();
        auto [beta1, beta2] = adam_options->betas();
        bool maximize = maximize_;
        
        for (auto& param : group.params()) {
            // Check if gradient exists
            if (!param.grad().defined() || param.grad().numel() == 0) {
                continue;
            }
            
            auto grad = param.grad();
            
            // Negate gradient if maximizing
            if (maximize) {
                grad = -grad;
            }
            
            // Coalesce sparse gradient to ensure unique indices
            if (grad.is_sparse()) {
                grad = grad.coalesce();
            }
            
            // Get parameter ID
#if TORCH_VERSION_MAJOR == 2 && TORCH_VERSION_MINOR > 1
            auto pId = param.unsafeGetTensorImpl();
#else
            auto pId = c10::guts::to_string(param.unsafeGetTensorImpl());
#endif
            
            // Initialize or get state
            // Use AdamCustomParamState for compatibility with addToOptimizer/removeFromOptimizer
            auto& state_ = state();
            auto state_ptr = state_.find(pId);
            if (state_ptr == state_.end()) {
                auto paramState = std::make_unique<AdamCustomParamState>();
                paramState->exp_avg() = torch::zeros_like(param, torch::MemoryFormat::Preserve);
                paramState->exp_avg_sq() = torch::zeros_like(param, torch::MemoryFormat::Preserve);
                paramState->step_per_gaussian() = torch::zeros_like(param, torch::MemoryFormat::Preserve);
                paramState->step() = 0;
                state_[pId] = std::move(paramState);
                state_ptr = state_.find(pId);
            }
            
            auto& stored_state = static_cast<AdamCustomParamState&>(*state_ptr->second);
            auto& exp_avg = stored_state.exp_avg();
            auto& exp_avg_sq = stored_state.exp_avg_sq();
            auto& state_step = stored_state.step();
            
            // Increment step
            state_step++;
            
            // Handle sparse gradients with CUDA acceleration
            if (grad.is_sparse()) {
                auto grad_indices = grad._indices();
                auto grad_values = grad._values();
                
                // Skip update if gradient values are empty
                if (grad_values.numel() == 0) {
                    continue;
                }
                
                // Ensure grad_indices is 1D (squeeze if needed)
                if (grad_indices.dim() > 1) {
                    grad_indices = grad_indices.squeeze(0);
                }
                
                // Compute bias correction
                double bias_correction1 = 1.0 - std::pow(beta1, state_step);
                double bias_correction2 = 1.0 - std::pow(beta2, state_step);
                float bias_correction1_rcp = static_cast<float>(1.0 / bias_correction1);
                float bias_correction2_sqrt_rcp = static_cast<float>(1.0 / std::sqrt(bias_correction2));
                
                // Use CUDA kernel for fast sparse update
                gsplat::launch_sparse_adam_kernel(
                    param,
                    exp_avg,
                    exp_avg_sq,
                    grad_indices,
                    grad_values,
                    static_cast<float>(lr),
                    static_cast<float>(beta1),
                    static_cast<float>(beta2),
                    static_cast<float>(eps),
                    bias_correction1_rcp,
                    bias_correction2_sqrt_rcp
                );
                
            } else {
                // For dense gradients, fallback to gsplat::adam_step for acceleration
                double bias_correction1 = 1.0 - std::pow(beta1, state_step);
                double bias_correction2 = 1.0 - std::pow(beta2, state_step);
                float bias_correction1_rcp = static_cast<float>(1.0 / bias_correction1);
                float bias_correction2_sqrt_rcp = static_cast<float>(1.0 / std::sqrt(bias_correction2));
                
                gsplat::adam_step(
                    param,
                    exp_avg,
                    exp_avg_sq,
                    grad,
                    static_cast<float>(lr),
                    static_cast<float>(beta1),
                    static_cast<float>(beta2),
                    static_cast<float>(eps),
                    bias_correction1_rcp,
                    bias_correction2_sqrt_rcp
                );
            }
        }
    }
}