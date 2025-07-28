#pragma once
#include "core/core.h"
#include "core/uuid.h"

#define SET_ASSET_TYPE(type)                        \
    static AssetType get_static_type()                \
    {                                               \
        return type;                                \
    }                                               \
    virtual AssetType get_asset_type() const override \
    {                                               \
        return get_static_type();                     \
    }

namespace diverse
{
    enum class AssetFlag : uint16_t
    {
        None     = 0,
        Missing  = BIT(0),
        Invalid  = BIT(1),
        Loaded   = BIT(2), //load 2 ccpu
        UnLoaded = BIT(3), //unload
        UploadedGpu = BIT(4), //load 2 gpu
    };

    enum class AssetType : uint16_t
    {
        None                = 0,
        Texture             = 1,
        Splat                = 2,
        Scene               = 3,
        Audio               = 4,
        Font                = 5,
        Shader              = 6,
        Material            = 7,
        PhysicsMaterial     = 8,
        MeshModel               = 9,
        Skeleton            = 10,
        Animation           = 11,
        AnimationController = 12,
        PointCloud = 13,
    };

    class Asset
    {
    public:
        uint16_t flags = (uint16_t)AssetFlag::None;

        virtual ~Asset() { }

        static AssetType get_static_type() { return AssetType::None; }
        virtual AssetType get_asset_type() const { return AssetType::None; }

        bool is_valid() const { return ((flags & (uint16_t)AssetFlag::Missing) | (flags & (uint16_t)AssetFlag::Invalid)) == 0; }

        virtual bool operator==(const Asset& other) const
        {
            return handle == other.handle;
        }

        virtual bool operator!=(const Asset& other) const
        {
            return !(*this == other);
        }

        bool is_flag_set(AssetFlag flag) const { return (uint16_t)flag & flags; }
        void set_flag(AssetFlag flag, bool value = true)
        {
            if(value)
                flags |= (uint16_t)flag;
            else
                flags &= ~(uint16_t)flag;
        }

        UUID handle;
    };

}
