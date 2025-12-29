#include "model.hpp"
#include <gsplat_cluster.hpp>

auto PruneStrategy::cullShBands(std::vector<Camera>& cam,int step, float threshold, float stdThreshold)->void
{
#if defined(USE_CUDA) || defined(USE_HIP)
    //const auto& trainConfig = gaussian->trainConfig;
    //auto& means = gaussian->means;
    //auto& opacities = gaussian->opacities;
    //auto& scales = gaussian->scales;
    //auto& quats = gaussian->quats;
    //auto shs = gaussian->getSHs();
    //const float scaleFactor = trainConfig.progressiveTrain ? gaussian->getDownscaleFactor(step) : 1;
    //const int height = static_cast<int>(static_cast<float>(cam[0].height) / scaleFactor);
    //const int width = static_cast<int>(static_cast<float>(cam[0].width) / scaleFactor);

    //torch::Tensor camToWorld = cam[0].camToWorld.unsqueeze(0), KsMat = cam[0].getIntrinsicsMatrix(scaleFactor).unsqueeze(0);
    //for (auto c = 1; c < cam.size(); c++) 
    //{
    //    camToWorld = torch::cat({camToWorld, cam[c].camToWorld.unsqueeze(0)});
    //    KsMat = torch::cat({ camToWorld, cam[c].getIntrinsicsMatrix(scaleFactor).unsqueeze(0) });
    //}
    //auto [_, weightedVariance, weightedMean] = calculateColourVariance(means,opacities, 
    //                                                                   scales, quats,
    //                                                                   shs, degree, 
    //                                                                   camToWorld,KsMat,
    //                                                                   height, width,
    //                                                                   trainConfig.shDegree,
    //                                                                   trainConfig.modelType == SplatModelType::Splat2D);
    //lowVarianceColorCulling(stdThreshold,degree,gaussian->featuresDc,gaussian->featuresRest, weightedVariance, weightedMean);
    //auto [colorDistances,a,b] = calculateColourVariance(means, 
    //                            opacities,
    //                            scales, quats,
    //                            shs, degree,
    //                            camToWorld, KsMat,
    //                            height, width,
    //                            trainConfig.shDegree,
    //                            trainConfig.modelType == SplatModelType::Splat2D);
    //int degreesToUse = (std::min<int>)(step / trainConfig.shDegreeInterval, trainConfig.shDegree);
    //lowDistanceColorCulling(threshold, degreesToUse, degree, gaussian->featuresRest,colorDistances);
#endif
}

auto PruneStrategy::prune(std::vector<Camera>& cam, int step) -> void
{
    if(gaussian->trainConfig.refineStopIter == step && gaussian->trainConfig.cullSH )
        cullShBands(cam, step, cdistThreshold * std::sqrt(3) / 255.0f, stdThreshold);
    //     //cull point which is not in sky box 
    //     auto dist = gaussian->means - gaussian->pointsCenter;
    //     auto dist2 = dist.pow(2).sum(-1);
    //     torch::Tensor isInSkyBox = dist2 < gaussian->skyDist * gaussian->skyDist;
    //     auto cull = ~isInSkyBox;
    //     gaussian->prunePoints(cull);
    // }
}