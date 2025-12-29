#ifndef OPTIM_SCHEDULER
#define OPTIM_SCHEDULER

#include <iostream>
#include <torch/torch.h>

class OptimScheduler{
public:
    OptimScheduler(torch::optim::Adam *opt, float lrFinal, int maxSteps) :
        opt(opt), lrInit(
            static_cast<torch::optim::AdamOptions&>(opt->param_groups()[0].options()).get_lr()
        ), lrFinal(lrFinal), maxSteps(maxSteps) {};
    void step(int step);
    double getlr(int step);

private:
    torch::optim::Adam *opt;
    float lrInit;
    float lrFinal;
    int maxSteps;
};


class ExponentialLR {
public:
    ExponentialLR(torch::optim::Optimizer& optimizer, double gamma, int param_group_index = -1)
        : optimizer_(optimizer),
          gamma_(gamma),
          param_group_index_(param_group_index) {}

    void step();
    double getlr(int step);
private:
    torch::optim::Optimizer& optimizer_;
    double gamma_;
    int param_group_index_;
};

#endif