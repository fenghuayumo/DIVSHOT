#pragma once

#include "asset_id.h"
#include "core/core.h"
#include <cstdint>

namespace diverse
{
    // Forward declaration
    class AssetRegistry;

    // AssetHandle is a stable reference to an asset
    // It contains the asset ID and a generation counter for validation
    // This allows detecting stale handles after hot reload
    template<typename AssetType>
    class AssetHandle
    {
    public:
        AssetHandle() : id(InvalidAssetId()), generation(0) {}

        explicit AssetHandle(const AssetId& asset_id, uint32_t gen = 0)
            : id(asset_id), generation(gen)
        {}

        // Get the asset ID
        const AssetId& get_id() const { return id; }

        // Check if handle is valid (points to a real asset)
        bool is_valid() const { return id.is_valid(); }

        // Get generation counter for validation
        uint32_t get_generation() const { return generation; }

        // Update generation (called by registry on hot reload)
        void update_generation(uint32_t new_gen) { generation = new_gen; }

        // Comparison operators
        bool operator==(const AssetHandle& other) const
        {
            return id == other.id && generation == other.generation;
        }

        bool operator!=(const AssetHandle& other) const
        {
            return !(*this == other);
        }

        // For use in ordered containers
        bool operator<(const AssetHandle& other) const
        {
            if (id.id != other.id.id)
                return id.id < other.id.id;
            return generation < other.generation;
        }

    private:
        AssetId id;
        uint32_t generation;  // Incremented on asset reload
    };

    // Type aliases for common asset handles
    using TextureAssetHandle = AssetHandle<class TextureAsset>;
    using MeshAssetHandle = AssetHandle<class MeshAsset>;
    using MaterialAssetHandle = AssetHandle<class MaterialAsset>;

    // Hash function for AssetHandle
    template<typename AssetType>
    struct AssetHandleHash
    {
        size_t operator()(const AssetHandle<AssetType>& handle) const
        {
            size_t h = AssetId::Hash()(handle.get_id());
            // Combine generation into hash
            return h ^ (handle.get_generation() << 16);
        }
    };
}

// Specialization for std::unordered_map
template<typename AssetType>
struct std::hash<diverse::AssetHandle<AssetType>>
{
    size_t operator()(const diverse::AssetHandle<AssetType>& handle) const
    {
        return std::hash<diverse::AssetId>()(handle.get_id()) ^
               (handle.get_generation() << 16);
    }
};
