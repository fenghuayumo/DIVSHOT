#ifndef SPLAT_ADC_PLUS_HPP
#define SPLAT_ADC_PLUS_HPP
#include "model.hpp"

class SplatADCPlusDensify : public DensifyStrategy
{
public:
    SplatADCPlusDensify(GaussianTrainModel* model) : DensifyStrategy(model) { type = SplatDensifyType::SplatADCPlus;}
    void stepAfterbackward(int step, std::unordered_map<std::string_view, torch::Tensor>& infos) override;

    void resetState() override;
public:
    torch::Tensor xysGradNorm; // set in afterTrain()
    torch::Tensor visCounts; // set in afterTrain()  
};

#endif