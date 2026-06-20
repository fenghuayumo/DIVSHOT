#pragma once
#include "core/core.h"
#include "core/uuid.h"
#include <atomic>

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
        Gaussian = 14,
    };

    class Asset
    {
    public:
        std::atomic<uint16_t> flags = (uint16_t)AssetFlag::None;

        Asset() = default;
        Asset(const Asset& other)
            : flags(other.flags.load(std::memory_order_acquire))
            , handle(other.handle)
        {
        }

        Asset(Asset&& other) noexcept
            : flags(other.flags.load(std::memory_order_acquire))
            , handle(other.handle)
        {
        }

        Asset& operator=(const Asset& other)
        {
            if(this != &other)
            {
                flags.store(other.flags.load(std::memory_order_acquire), std::memory_order_release);
                handle = other.handle;
            }
            return *this;
        }

        Asset& operator=(Asset&& other) noexcept
        {
            if(this != &other)
            {
                flags.store(other.flags.load(std::memory_order_acquire), std::memory_order_release);
                handle = other.handle;
            }
            return *this;
        }

        virtual ~Asset() { }

        static AssetType get_static_type() { return AssetType::None; }
        virtual AssetType get_asset_type() const { return AssetType::None; }

        bool is_valid() const
        {
            const auto currentFlags = flags.load(std::memory_order_acquire);
            return ((currentFlags & (uint16_t)AssetFlag::Missing) | (currentFlags & (uint16_t)AssetFlag::Invalid)) == 0;
        }

        virtual bool operator==(const Asset& other) const
        {
            return handle == other.handle;
        }

        virtual bool operator!=(const Asset& other) const
        {
            return !(*this == other);
        }

        bool is_flag_set(AssetFlag flag) const { return (flags.load(std::memory_order_acquire) & (uint16_t)flag) != 0; }
        void set_flag(AssetFlag flag, bool value = true)
        {
            if(value)
                flags.fetch_or((uint16_t)flag, std::memory_order_release);
            else
                flags.fetch_and((uint16_t)~(uint16_t)flag, std::memory_order_release);
        }

        UUID handle;
    };

}
