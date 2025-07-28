#include "material.h"
#include "engine/file_system.h"
#include "core/profiler.h"
namespace diverse
{
    Material::Material()
    {
        DS_PROFILE_FUNCTION();

        flags = 0;
        set_render_flag(RenderFlags::DEPTHTEST);
        pbr_material_textures.albedo = nullptr;
    }

    Material::Material(const std::string& filePath)
    {
    }

    Material::~Material()
    {
        DS_PROFILE_FUNCTION();
    }

    void Material::set_textures(const PBRMataterialTextures& textures)
    {
        DS_PROFILE_FUNCTION();
        pbr_material_textures.albedo = textures.albedo;
        pbr_material_textures.normal = textures.normal;
        pbr_material_textures.roughness = textures.roughness;
        pbr_material_textures.metallic = textures.metallic;
        pbr_material_textures.ao = textures.ao;
        pbr_material_textures.emissive = textures.emissive;
    }

    void Material::set_material_properites(const MaterialProperties& properties)
    {
        DS_PROFILE_FUNCTION();
        
        material_properties = properties;
    }

    bool FileExists(const std::string& path)
    {
        DS_PROFILE_FUNCTION();

        std::string physicalPath;

        FileSystem::get().resolve_physical_path(path, physicalPath);
        return FileSystem::file_exists(physicalPath);
    }

    void Material::load_pbr_material(const std::string& name, const std::string& path, const std::string& extension)
    {
        DS_PROFILE_FUNCTION();

        this->name = name;
        pbr_material_textures = PBRMataterialTextures();
        auto filePath = path + "/" + name + "/albedo" + extension;

        if (FileExists(filePath))
            pbr_material_textures.albedo = createSharedPtr<asset::Texture>(asset::Texture(path + "/" + name + "/albedo" + extension));

        filePath = path + "/" + name + "/normal" + extension;

        if (FileExists(filePath))
            pbr_material_textures.normal = createSharedPtr<asset::Texture>(asset::Texture(path + "/" + name + "/normal" + extension));

        filePath = path + "/" + name + "/roughness" + extension;

        if (FileExists(filePath))
            pbr_material_textures.roughness = createSharedPtr<asset::Texture>(asset::Texture(path + "/" + name + "/roughness" + extension));

        filePath = path + "/" + name + "/metallic" + extension;

        if (FileExists(filePath))
            pbr_material_textures.metallic = createSharedPtr<asset::Texture>(asset::Texture(path + "/" + name + "/metallic" + extension));

        filePath = path + "/" + name + "/ao" + extension;

        if (FileExists(filePath))
            pbr_material_textures.ao = createSharedPtr<asset::Texture>(asset::Texture(path + "/" + name + "/ao" + extension));

        filePath = path + "/" + name + "/emissive" + extension;

        if (FileExists(filePath))
            pbr_material_textures.emissive = createSharedPtr<asset::Texture>(asset::Texture(path + "/" + name + "/emissive" + extension));
    }

    void Material::load_material(const std::string& name, const std::string& path)
    {
        DS_PROFILE_FUNCTION();

        this->name = name;
        pbr_material_textures = PBRMataterialTextures();
        pbr_material_textures.albedo = createSharedPtr<asset::Texture>(path);
        pbr_material_textures.normal = nullptr;
        pbr_material_textures.roughness = nullptr;
        pbr_material_textures.metallic = nullptr;
        pbr_material_textures.ao = nullptr;
        pbr_material_textures.emissive = nullptr;
    }

    void Material::set_albedo_texture(const std::string& path)
    {
        DS_PROFILE_FUNCTION();

        auto tex = createSharedPtr<asset::Texture>(path);
        if (tex)
        {
            pbr_material_textures.albedo = tex;
            textures_updated = true;
        }
    }

    void Material::set_normal_texture(const std::string& path)
    {
        DS_PROFILE_FUNCTION();

        auto tex = createSharedPtr<asset::Texture>(path);
        if (tex)
        {
            pbr_material_textures.normal = tex;
            textures_updated = true;
        }
    }

    void Material::set_roughness_texture(const std::string& path)
    {
        DS_PROFILE_FUNCTION();

        auto tex = createSharedPtr<asset::Texture>(path);
        if (tex)
        {
            pbr_material_textures.roughness = tex;
            textures_updated = true;
        }
    }

    void Material::set_metallic_texture(const std::string& path)
    {
        DS_PROFILE_FUNCTION();

        auto tex = createSharedPtr<asset::Texture>(path);
        if (tex)
        {
            pbr_material_textures.metallic = tex;
            textures_updated = true;
        }
    }

    void Material::set_ao_texture(const std::string& path)
    {
        DS_PROFILE_FUNCTION();

        auto tex = createSharedPtr<asset::Texture>(path);
        if (tex)
        {
            pbr_material_textures.ao = tex;
            textures_updated = true;
        }
    }

    void Material::set_emissive_texture(const std::string& path)
    {
        DS_PROFILE_FUNCTION();

        auto tex = createSharedPtr<asset::Texture>(path);
        if (tex)
        {
            pbr_material_textures.emissive = tex;
            textures_updated = true;
        }
    }
    bool PBRMataterialTextures::is_upload_2_gpu() const
    {
        auto flag = true;
        if(albedo)
			flag &= albedo->is_flag_set(AssetFlag::UploadedGpu);
        if (normal)
            flag &= normal->is_flag_set(AssetFlag::UploadedGpu);
        if (roughness)
            flag &= roughness->is_flag_set(AssetFlag::UploadedGpu);
        if(metallic)
			flag &= metallic->is_flag_set(AssetFlag::UploadedGpu);
        if(ao)
            flag &= ao->is_flag_set(AssetFlag::UploadedGpu);
        if(metallic)
            flag &= metallic->is_flag_set(AssetFlag::UploadedGpu);
        if(emissive)
            flag &= emissive->is_flag_set(AssetFlag::UploadedGpu);
        return flag;
    }
}