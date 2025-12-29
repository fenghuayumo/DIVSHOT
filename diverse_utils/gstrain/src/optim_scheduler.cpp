#include "optim_scheduler.hpp"


double OptimScheduler::getlr(int step){
    float t = (std::max)((std::min)(static_cast<float>(step) / static_cast<float>(maxSteps), 1.0f), 0.0f);
    return std::exp(std::log(lrInit) * (1.0f - t) + std::log(lrFinal) * t);
    //auto gamma = std::pow(0.01, 1.0 / maxSteps);
    //auto lr = lrInit * std::pow(gamma, step);
    //return lr;
}

void OptimScheduler::step(int step){
    double lr = getlr(step);
    static_cast<torch::optim::AdamOptions&>(opt->param_groups()[0].options()).set_lr(lr);
}

void ExponentialLR::step() {
    if (param_group_index_ >= 0) {
        auto& group = optimizer_.param_groups()[param_group_index_];

        // Try to cast to our custom Options first
        if (auto* adam_options = dynamic_cast<torch::optim::AdamOptions*>(&group.options())) {
            double current_lr = adam_options->lr();
            adam_options->lr(current_lr * gamma_);
        } else {
            throw std::runtime_error("Invalid options passed to ExponentialLR");
        }
    } else {
        // Update all param groups
        for (auto& group : optimizer_.param_groups()) {
            if (auto* adam_options = dynamic_cast<torch::optim::AdamOptions*>(&group.options())) {
                double current_lr = adam_options->lr();
                adam_options->lr(current_lr * gamma_);
            } else {
                throw std::runtime_error("Invalid options passed to ExponentialLR");
            }
        }
    }
}

double ExponentialLR::getlr(int step) {
    auto& group = optimizer_.param_groups()[0];
    auto* adam_options = dynamic_cast<torch::optim::AdamOptions*>(&group.options());
    double current_lr = adam_options->lr();
    return current_lr;
}
