#include <vector>
#include "gaussian_component.h"
#include "utility/thread_pool.h"

namespace diverse
{
    GaussianComponent::GaussianComponent(const std::string& filePath)
    {
        // load_from_library(path);
        ModelRef =  createSharedPtr<GaussianModel>(filePath);
    }

    GaussianComponent::GaussianComponent()
    {
        ModelRef = createSharedPtr<GaussianModel>();
    }
    GaussianComponent::GaussianComponent(int max_splats)
    {
        ModelRef = createSharedPtr<GaussianModel>(max_splats);
    }

    void GaussianComponent::apply_color_adjustment()
    {
        if(albedo_color != glm::vec3(0)  || black_point != 0 || white_point != 1 || brightness != 1)
        {
            const float SH0 = 0.282094791773878f;
            const auto to = [=](f32 value) {return value * SH0 + 0.5f;};
            const auto from = [=](f32 value) {return (value - 0.5f) / SH0;};
            const auto offset = -black_point + brightness;
            const auto scale = 1.0f / (white_point - black_point);;
            
            parallel_for<size_t>(0, ModelRef->sh0().size(),[&](size_t i){
                auto& f_dc_0 = ModelRef->sh0()[i][0];
                auto& f_dc_1 = ModelRef->sh0()[i][1];
                auto& f_dc_2 = ModelRef->sh0()[i][2];
                f_dc_0 = from(offset + to(f_dc_0) * albedo_color.x * scale);
                f_dc_1 = from(offset + to(f_dc_1) * albedo_color.y * scale);
                f_dc_2 = from(offset + to(f_dc_2) * albedo_color.z * scale);
            });
        }
        if( transparency != 1.0f )
        {
            const auto invSig = [](f32 value) {return (value <= 0) ? -400 : ((value >= 1) ? 400 : -std::log(1/value - 1)); };
            const auto sig = [](f32 value) {return 1.0f / (1.0f + std::exp(-value));};
            parallel_for<size_t>(0, ModelRef->opacity().size(),[&](size_t i){
                auto& opacity = ModelRef->opacity()[i];
                opacity = invSig(sig(opacity) * transparency);
            });
        }
    }
    // void GaussianComponent::load_from_library(const std::string& path)
    // {
    //     // ModelRef = Application::get().get_model_library()->GetResource(path);
    // }
}