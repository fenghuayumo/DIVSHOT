#pragma once

#include "asset.h"
#include "asset_id.h"
#include "cpu_assets.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace diverse
{
    enum class AssetState : uint8_t
    {
        Unloaded = 0,
        LoadingCpu = 1,
        ReadyCpu = 2,
        UploadQueued = 3,
        ResidentGpu = 4,
        Failed = 5
    };

    // Import parameters for different asset types
    struct ImportParameters
    {
        std::unordered_map<std::string, std::string> settings;

        template<typename T>
        T get(const std::string& key, const T& default_value) const
        {
            auto it = settings.find(key);
            if (it != settings.end())
            {
                if constexpr (std::is_same_v<T, float>)
                    return std::stof(it->second);
                else if constexpr (std::is_same_v<T, int>)
                    return std::stoi(it->second);
                else if constexpr (std::is_same_v<T, bool>)
                    return it->second == "true";
                else if constexpr (std::is_same_v<T, std::string>)
                    return it->second;
            }
            return default_value;
        }

        void set(const std::string& key, const std::string& value)
        {
            settings[key] = value;
        }
    };

    // Asset record stored in AssetRegistry (AssetDB)
    struct AssetRecord
    {
        AssetId id;
        std::filesystem::path source_path;
        AssetType type = AssetType::None;
        std::string name;
        ImportParameters import_params;
        uint64_t content_hash = 0;
        uint32_t version = 0;
        AssetState state = AssetState::Unloaded;
        std::vector<AssetId> dependencies;
        std::vector<AssetId> dependents;
        std::filesystem::file_time_type last_modified;

        bool is_valid() const { return id.is_valid() && type != AssetType::None; }

        TextureImportSettings get_texture_settings() const;
        MeshImportSettings get_mesh_settings() const;
    };

    using AssetMetadata = AssetRecord;

    inline TextureImportSettings AssetRecord::get_texture_settings() const
    {
        TextureImportSettings settings;
        settings.generate_mips = import_params.get<bool>("generate_mips", true);
        settings.srgb = import_params.get<bool>("srgb", false);
        settings.compression = import_params.get<bool>("compression", false);
        if (import_params.settings.count("max_mip_levels"))
        {
            settings.max_mip_levels = import_params.get<int>("max_mip_levels", 0);
        }
        return settings;
    }

    inline MeshImportSettings AssetRecord::get_mesh_settings() const
    {
        MeshImportSettings settings;
        settings.calculate_normals = import_params.get<bool>("calculate_normals", false);
        settings.calculate_tangents = import_params.get<bool>("calculate_tangents", true);
        settings.optimize_vertices = import_params.get<bool>("optimize_vertices", true);
        settings.optimization_threshold = import_params.get<float>("optimization_threshold", 0.95f);
        return settings;
    }
}
