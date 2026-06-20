#pragma once

#include "core/core.h"
#include "core/uuid.h"
#include <functional>

namespace diverse
{
    // Stable asset identifier used throughout the asset system
    // Unlike UUID which is generated per asset instance, AssetId is stable
    // and refers to a specific asset in the registry
    struct AssetId
    {
        UUID id;

        AssetId() = default;
        explicit AssetId(const UUID& uuid) : id(uuid) {}

        bool operator==(const AssetId& other) const { return id == other.id; }
        bool operator!=(const AssetId& other) const { return !(*this == other); }

        bool is_valid() const { return id != UUID(); }

        // For use in hash maps
        struct Hash
        {
            size_t operator()(const AssetId& asset_id) const
            {
                return std::hash<UUID>()(asset_id.id);
            }
        };
    };

    // Invalid asset ID constant
    inline AssetId InvalidAssetId() { return AssetId(UUID()); }

    // Generate a new unique asset ID
    inline AssetId GenerateAssetId()
    {
        return AssetId(UUID());
    }
}

// Specialization for std::unordered_map
template <>
struct std::hash<diverse::AssetId>
{
    size_t operator()(const diverse::AssetId& asset_id) const
    {
        return std::hash<diverse::UUID>()(asset_id.id);
    }
};
