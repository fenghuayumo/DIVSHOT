#pragma once

#include "core/reference.h"
#include "assets/texture.h"
namespace diverse
{
    const uint PBR_WORKFLOW_SEPARATE_TEXTURES = 0;
    const uint PBR_WORKFLOW_METALLIC_ROUGHNESS = 1;
    const uint PBR_WORKFLOW_SPECULAR_GLOSINESS = 2;


    struct MaterialProperties
    {
        glm::vec4 albedoColour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        
        uint albedo_map = 0xffffffff;
        uint metallic_map = 0xffffffff;
        uint normal_map = 0xffffffff;
        uint emissive_map = 0xffffffff;

        float roughness = 0.7f;
        float metalness = 0.7f;
        float emissive_map_factor = 0.0f;
        float ao_map_factor = 0.0f;

        uint work_flow = 0;
        float metallic_map_factor = 0.0f;
        float roughness_map_factor = 0.0f;
        float normal_map_factor = 0.0f;

        float reflectance = 0.3f;
        float ao = 1.0f;
        float alphaCutoff = 0.4f;
        uint ao_map = 0xffffffff;

        glm::vec3 emissive = glm::vec3(0.0f, 0.0f, 0.0f);
        uint roughness_map = 0xffffffff;

        std::array<std::array<f32, 6>, 6> map_transforms = { std::array<f32, 6>{1.0, 0.0, 0.0, 1.0, 0.0, 0.0} ,
                                        {1.0, 0.0, 0.0, 1.0, 0.0, 0.0} ,
                                        {1.0, 0.0, 0.0, 1.0, 0.0, 0.0} ,
                                        {1.0, 0.0, 0.0, 1.0, 0.0, 0.0} ,
                                        {1.0, 0.0, 0.0, 1.0, 0.0, 0.0} ,
                                        {1.0, 0.0, 0.0, 1.0, 0.0, 0.0} };
    };
    struct PBRMataterialTextures
    {
        SharedPtr<asset::Texture> albedo;
        SharedPtr<asset::Texture> normal;
        SharedPtr<asset::Texture> metallic;
        SharedPtr<asset::Texture> roughness;
        SharedPtr<asset::Texture> ao;
        SharedPtr<asset::Texture> emissive;
        
        bool is_upload_2_gpu() const;
    };

    class Material : public Asset
    {
    public:
        enum class RenderFlags
        {
            NONE = 0,
            DEPTHTEST = BIT(0),
            WIREFRAME = BIT(1),
            FORWARDRENDER = BIT(2),
            DEFERREDRENDER = BIT(3),
            NOSHADOW = BIT(4),
            TWOSIDED = BIT(5),
            ALPHABLEND = BIT(6)
        };


    public:
        Material();
        Material(const std::string& filePath);
        ~Material();

        void load_pbr_material(const std::string& name, const std::string& path, const std::string& extension = ".png"); // TODO : Texture Parameters
        void load_material(const std::string& name, const std::string& path);


        void set_textures(const PBRMataterialTextures& textures);
        void set_material_properites(const MaterialProperties& properties);
        void update_material_properties_data();

        void set_albedo_texture(const std::string& path);
        void set_normal_texture(const std::string& path);
        void set_roughness_texture(const std::string& path);
        void set_metallic_texture(const std::string& path);
        void set_ao_texture(const std::string& path);
        void set_emissive_texture(const std::string& path);

        bool& get_textures_updated() { return textures_updated; }
        const bool& get_textures_updated() const { return textures_updated; }
        void set_textures_updated(bool updated) { textures_updated = updated; }
        PBRMataterialTextures& get_textures() { return pbr_material_textures; }
        const PBRMataterialTextures& get_textures() const { return pbr_material_textures; }

        const std::string& get_name() const { return name; }
        void  set_name(const std::string& name) { this->name = name;}
        const MaterialProperties& get_properties() const { return material_properties; }
        MaterialProperties&       get_properties() {return material_properties;}
        uint32_t get_render_flags() const { return render_flags; };
        bool get_render_flag(RenderFlags flag) const { return (uint32_t)flag & render_flags; };
        void set_render_flag(RenderFlags flag, bool value = true)
        {
            if (value)
            {
                render_flags |= (uint32_t)flag;
            }
            else
            {
                render_flags &= ~(uint32_t)flag;
            }
        };

        const std::string& get_material_path() const { return material_path; }
        void set_material_path(const std::string& path) { material_path = path; }
        bool& dirty_flag() {return dirty;}
    public:
        template <typename Archive>
        void save(Archive& archive) const
        {

        }
        template <typename Archive>
        void load(Archive& archive)
        {
        }
    private:
        PBRMataterialTextures pbr_material_textures;
        MaterialProperties material_properties;
        std::string name;
        bool textures_updated = false;
        uint32_t render_flags;

        std::string material_path;
        bool dirty = false;
    };
}