#include "light_gaussian.hpp"

auto LightSplatModel::calculateVolumeImportanceScore(torch::Tensor importance_list, float v_pow)->torch::Tensor
{
   auto volume = torch::prod(torch::exp(gaussian->scales),1);
   auto index = int(volume.size(0) * 0.9);
   auto [sorted_volume, _] = torch::sort(volume, -1, true);
   auto kth_percent_largest = sorted_volume[index];
   //Calculate v_list
   auto v_list = torch::pow(volume / kth_percent_largest, v_pow);
   v_list = v_list * importance_list;
   return v_list;
}

auto LightSplatModel::pruneList(std::vector<Camera>& cameras, int step)->std::tuple<torch::Tensor,torch::Tensor>
{
    torch::NoGradGuard noGuard;
    if( gaussian->numCameras <= 0) return {};
    
    //auto [_,gaussian_list, imp_list] = gaussian->renderSplats(view_cam, step,true);
    torch::Tensor gaussian_list, imp_list;
    //for (auto i = 0;i< gaussian->numCameras; i++)
    for(auto& view_cam : cameras)
    {
        //auto& view_cam = cameras[i];
        if(!view_cam.is_loaded_ || !view_cam.K.defined()) continue;
        auto [_,gaussians_count, important_score] = gaussian->renderSplats(view_cam, step, true);
        if(!gaussian_list.defined()) gaussian_list = gaussians_count;
        if(!imp_list.defined()) imp_list = important_score;
        gaussian_list += gaussians_count;
        imp_list += important_score;
    }
    return {gaussian_list, imp_list};
}

auto LightSplatModel::pruneGaussians(float percent, const torch::Tensor& importance_score)->void
{
    torch::NoGradGuard noGuard;
	auto [sorted_tensor,_] = torch::sort(importance_score, 0);
	auto index_nth_percentile = int(percent * (sorted_tensor.size(0) - 1));

	auto value_nth_percentile = sorted_tensor[index_nth_percentile];
	auto pruneMask = (importance_score <= value_nth_percentile).squeeze();
    if (gaussian->trainConfig.verbose)
        std::cout << "Prune based on opacity and volume importance score. " <<std::endl;
    if(gaussian->trainConfig.enableBg)
        pruneMask.index_put_({torch::indexing::Slice(0, gaussian->trainConfig.numSkyPoints)}, 0);
    //cull points
    //torch::Tensor distances = (means - gaussian->pointsCenter).norm(2, -1);
    //auto cull = distances > gaussian->skyDist;
    //if (cull.sum().item<int>())
    //{
    //    gaussian->prunePoints(cull);
    //}
    gaussian->prunePoints(pruneMask);
}

auto LightSplatModel::prune(std::vector<Camera>& cam, int step) -> void
{   
    torch::NoGradGuard noGuard;
    // if ((step > gaussian->trainConfig.warmupLength && step % gaussian->trainConfig.resetAlphaEvery != 0) && 
    //     step % gaussian->trainConfig.pruneInterval == 0 && step <= (gaussian->trainConfig.numIters - 5000) )
    {
        auto [gaussian_list, importance_list] = pruneList(cam, step);
        auto vList = calculateVolumeImportanceScore(importance_list.squeeze(-1), gaussian->trainConfig.v_pow);
        pruneGaussians(std::pow(gaussian->trainConfig.pruneDecay,pruneIter) * prunePercent, vList);
        pruneIter++;
    }
    PruneStrategy::prune(cam, step);
}