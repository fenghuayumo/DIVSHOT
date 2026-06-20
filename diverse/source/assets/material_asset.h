#pragma once
#include "asset_handle.h"
#include "cpu_assets.h"
#include "gpu_assets.h"
#include "material_properties.h"
#include <array>

namespace diverse
{
    // Forward declarations
    class TextureAsset;

    // MaterialAsset - CPU-side material data
    // Uses AssetHandle for texture references, not direct pointers
    struct MaterialAsset
    {
        AssetId id;
        std::string name;
        std::filesystem::path source_path;

        // Material properties (GPU-compatible layout)
        MaterialProperties properties;

        // Texture references using handles (not SharedPtr!)
        AssetHandle<TextureAsset> albedo;
        AssetHandle<TextureAsset> normal;
        AssetHandle<TextureAsset> metallic;
        AssetHandle<TextureAsset> roughness;
        AssetHandle<TextureAsset> ao;
        AssetHandle<TextureAsset> emissive;
        AssetHandle<TextureAsset> transmission;
        AssetHandle<TextureAsset> normal_detail;

        // Metadata
        uint32_t version;  // Incremented on reload
        bool is_valid;

        // Render flags
        uint32_t render_flags;

        MaterialAsset()
            : version(0)
            , is_valid(false)
            , render_flags(0)
        {
            // Initialize properties with defaults
            properties = MaterialProperties{};
        }

        // Check if all texture handles are valid
        bool has_valid_textures() const;

        // Get texture handle by slot index
        AssetHandle<TextureAsset> get_texture_handle(size_t slot) const;

        // Set texture handle by slot index
        void set_texture_handle(size_t slot, const AssetHandle<TextureAsset>& handle);

        size_t calculate_memory_size() const
        {
            return sizeof(MaterialProperties) + name.capacity() + source_path.native().capacity();
        }
    };

    struct MaterialImportSettings
    {
        bool generate_normals = false;
        float normal_scale = 1.0f;
        bool use_pbr_workflow = true;  // true = PBR, false = specular/glossiness
    };

    // Helper functions for material-texture slot mapping
    inline constexpr size_t texture_slot_to_property_index(size_t slot)
    {
        switch (slot)
        {
            case MaterialGpu::TEXTURE_SLOT_ALBEDO: return offsetof(MaterialProperties, albedo_map) / sizeof(uint32_t);
            case MaterialGpu::TEXTURE_SLOT_NORMAL: return offsetof(MaterialProperties, normal_map) / sizeof(uint32_t);
            case MaterialGpu::TEXTURE_SLOT_METALLIC: return offsetof(MaterialProperties, metallic_map) / sizeof(uint32_t);
            case MaterialGpu::TEXTURE_SLOT_ROUGHNESS: return offsetof(MaterialProperties, roughness_map) / sizeof(uint32_t);
            case MaterialGpu::TEXTURE_SLOT_AO: return offsetof(MaterialProperties, ao_map) / sizeof(uint32_t);
            case MaterialGpu::TEXTURE_SLOT_EMISSIVE: return offsetof(MaterialProperties, emissive_map) / sizeof(uint32_t);
            case MaterialGpu::TEXTURE_SLOT_TRANSMISSION: return offsetof(MaterialProperties, transmission_map) / sizeof(uint32_t);
            case MaterialGpu::TEXTURE_SLOT_NORMAL_DETAIL: return offsetof(MaterialProperties, normal_detail_map) / sizeof(uint32_t);
            default: return 0;
        }
    }

    // Default texture IDs for fallback (must match renderer constants)
    constexpr uint32_t WHITE_TEX_ID = 0;
    constexpr uint32_t BLACK_TEX_ID = 1;
    constexpr uint32_t NORMAL_TEX_ID = 2;

    // Create default material asset
    std::shared_ptr<MaterialAsset> create_default_material();

    // Convert MaterialAsset to MaterialGpu (bindless indices only)
    MaterialGpu create_material_gpu(const MaterialAsset& asset);

    // Update MaterialProperties with bindless indices from MaterialGpu
    void update_material_properties_bindless_indices(MaterialProperties& props, const MaterialGpu& gpu);

} // namespace diverse
