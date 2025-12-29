#ifndef DEFAULT_MODEL_HPP
#define DEFAULT_MODEL_HPP
#include "model.hpp"

class DefaultDensify : public DensifyStrategy
{
public:
    DefaultDensify(GaussianTrainModel* model) : DensifyStrategy(model) { type = SplatDensifyType::SplatADC;}
    void stepAfterbackward(int step, std::unordered_map<std::string_view, torch::Tensor>& infos) override;

    void resetState() override;
public:
    torch::Tensor xysGradNorm; // set in afterTrain()
    torch::Tensor visCounts; // set in afterTrain()  
    torch::Tensor max2DSize; // set in afterTrain()
};

#endif