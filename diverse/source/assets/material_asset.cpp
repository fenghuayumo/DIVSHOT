#include "material_asset.h"
#include <algorithm>

namespace diverse
{
    // MaterialAsset implementation
    bool MaterialAsset::has_valid_textures() const
    {
        return albedo.is_valid() ||
               normal.is_valid() ||
               metallic.is_valid() ||
               roughness.is_valid() ||
               ao.is_valid() ||
               emissive.is_valid();
    }

    AssetHandle<TextureAsset> MaterialAsset::get_texture_handle(size_t slot) const
    {
        switch (slot)
        {
            case MaterialGpu::TEXTURE_SLOT_ALBEDO: return albedo;
            case MaterialGpu::TEXTURE_SLOT_NORMAL: return normal;
            case MaterialGpu::TEXTURE_SLOT_METALLIC: return metallic;
            case MaterialGpu::TEXTURE_SLOT_ROUGHNESS: return roughness;
            case MaterialGpu::TEXTURE_SLOT_AO: return ao;
            case MaterialGpu::TEXTURE_SLOT_EMISSIVE: return emissive;
            case MaterialGpu::TEXTURE_SLOT_TRANSMISSION: return transmission;
            case MaterialGpu::TEXTURE_SLOT_NORMAL_DETAIL: return normal_detail;
            default: return AssetHandle<TextureAsset>();
        }
    }

    void MaterialAsset::set_texture_handle(size_t slot, const AssetHandle<TextureAsset>& handle)
    {
        switch (slot)
        {
            case MaterialGpu::TEXTURE_SLOT_ALBEDO: albedo = handle; break;
            case MaterialGpu::TEXTURE_SLOT_NORMAL: normal = handle; break;
            case MaterialGpu::TEXTURE_SLOT_METALLIC: metallic = handle; break;
            case MaterialGpu::TEXTURE_SLOT_ROUGHNESS: roughness = handle; break;
            case MaterialGpu::TEXTURE_SLOT_AO: ao = handle; break;
            case MaterialGpu::TEXTURE_SLOT_EMISSIVE: emissive = handle; break;
            case MaterialGpu::TEXTURE_SLOT_TRANSMISSION: transmission = handle; break;
            case MaterialGpu::TEXTURE_SLOT_NORMAL_DETAIL: normal_detail = handle; break;
        }
    }

    // Helper functions
    std::shared_ptr<MaterialAsset> create_default_material()
    {
        auto material = std::make_shared<MaterialAsset>();
        material->id = GenerateAssetId();
        material->name = "default_material";
        material->is_valid = true;
        material->version = 0;

        // Set default properties
        material->properties.base_color_mult = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        material->properties.roughness_mult = 0.7f;
        material->properties.metalness_factor = 0.0f;
        material->properties.emissive = glm::vec3(0.0f, 0.0f, 0.0f);

        // All textures will use fallback white/black/normal textures
        // (resolved by renderer)

        return material;
    }

    MaterialGpu create_material_gpu(const MaterialAsset& asset)
    {
        MaterialGpu gpu;
        gpu.resident_version = asset.version;

        // Bindless indices will be resolved by GpuResourceSystem
        // For now, initialize with invalid indices
        gpu.texture_bindless_indices.fill(0xFFFFFFFF);

        return gpu;
    }

    void update_material_properties_bindless_indices(MaterialProperties& props, const MaterialGpu& gpu)
    {
        // Update the MaterialProperties struct with bindless indices
        // This is what gets uploaded to the GPU material buffer

        props.albedo_map = gpu.get_bindless_index(MaterialGpu::TEXTURE_SLOT_ALBEDO);
        props.normal_map = gpu.get_bindless_index(MaterialGpu::TEXTURE_SLOT_NORMAL);
        props.metallic_map = gpu.get_bindless_index(MaterialGpu::TEXTURE_SLOT_METALLIC);
        props.roughness_map = gpu.get_bindless_index(MaterialGpu::TEXTURE_SLOT_ROUGHNESS);
        props.ao_map = gpu.get_bindless_index(MaterialGpu::TEXTURE_SLOT_AO);
        props.emissive_map = gpu.get_bindless_index(MaterialGpu::TEXTURE_SLOT_EMISSIVE);
        props.transmission_map = gpu.get_bindless_index(MaterialGpu::TEXTURE_SLOT_TRANSMISSION);
        props.normal_detail_map = gpu.get_bindless_index(MaterialGpu::TEXTURE_SLOT_NORMAL_DETAIL);
    }

} // namespace diverse
