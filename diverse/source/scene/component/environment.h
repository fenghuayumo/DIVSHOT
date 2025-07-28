#pragma once
#include <glm/vec4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <memory>
#include "light/light.h"
#include "renderer/light.h"
namespace diverse
{   
    namespace rhi
    {
        struct GpuTexture;
    }
    class Environment
    {
    public:
        enum class Mode
        {
            Pure,
            SunSky,
            HDR
        }mode = Mode::Pure;

        Environment();

        void    load_hdr(const std::string& path);
        void    load();
    public:
        static auto create_gray_tex()->std::shared_ptr<rhi::GpuTexture>;
    public:
        template <class Archive>
        void save(Archive& archive) const
        {
        }

        template <class Archive>
        void load(Archive& archive)
        {
        }
    public:
        std::shared_ptr<rhi::GpuTexture>    get_enviroment_map() const { return hdr_img; }
        void                                set_enviroment_map(const std::shared_ptr<rhi::GpuTexture>& param) { hdr_img = param; }
        const glm::vec3& get_color() const { return sky_ambient; }
        void set_color(const glm::vec3& param) { sky_ambient = param; }

        auto  get_render_light_data(const maths::Transform& transform, struct LightShaderData* light_data) -> void;
    public:
        glm::vec2   latent;
        float       intensity = 1.0f;
        float       theta = 0;
        float       phi = 0;
    	float sun_size_multiplier = 1.0f;
		glm::vec3 sky_ambient = glm::vec3(0, 0, 0);
        glm::vec3 sun_color = glm::vec3(1, 1, 1);
        bool      dirty_flag = false;
        int       cubeResolution = 256; // Default resolution for HDR ccube environment maps
    private:
        std::string file_path;
        std::shared_ptr<rhi::GpuTexture>   hdr_img;
    };
}