#pragma once

#include "asset_id.h"
#include "cpu_assets.h"
#include "gpu_assets.h"
#include <unordered_map>
#include <shared_mutex>
#include <memory>
#include <functional>
#include <cstdint>
#include <algorithm>

namespace diverse
{
    // Loading state for assets in the cache
    enum class LoadState : uint8_t
    {
        Unloaded = 0,
        Loading = 1,
        Loaded = 2,
        Failed = 3,
        GpuUploaded = 4,  // CPU data can be evicted
        Evicted = 5      // CPU data was evicted
    };

    // Memory pressure callback - returns current memory usage in bytes
    using MemoryPressureCallback = std::function<size_t()>;

    // Cache entry metadata
    template<typename AssetType>
    struct CacheEntry
    {
        std::shared_ptr<AssetType> asset;
        LoadState state;
        uint64_t last_accessed_frame;
        float last_access_time;
        bool gpu_uploaded;  // True = safe to evict CPU data
        size_t memory_size;
        uint32_t access_count;

        CacheEntry()
            : state(LoadState::Unloaded)
            , last_accessed_frame(0)
            , last_access_time(0.0f)
            , gpu_uploaded(false)
            , memory_size(0)
            , access_count(0)
        {}
    };

    // LRU cache entry for eviction tracking
    template<typename AssetType>
    struct LruEntry
    {
        AssetId id;
        uint64_t last_access;
        size_t memory_size;

        LruEntry() : last_access(0), memory_size(0) {}
        LruEntry(const AssetId& aid, uint64_t frame, size_t size)
            : id(aid), last_access(frame), memory_size(size) {}
    };

    // AssetCache - CPU-side asset cache with memory management
    // Responsibilities:
    // - Cache CPU asset data
    // - Track loading state
    // - Manage memory pressure via LRU eviction
    // - Support deferred release after GPU upload
    template<typename AssetType>
    class AssetCache
    {
    public:
        AssetCache();
        ~AssetCache() = default;

        // Get cached asset or trigger load
        std::shared_ptr<AssetType> get(const AssetId& id);

        // Insert asset into cache
        void insert(const AssetId& id, std::shared_ptr<AssetType> asset);

        // Get loading state
        LoadState get_state(const AssetId& id) const;

        // Set loading state (used by loader)
        void set_state(const AssetId& id, LoadState state);

        // Mark asset as GPU uploaded (CPU data can be evicted)
        void mark_gpu_uploaded(const AssetId& id);

        // Mark asset as GPU failed (should not be evicted)
        void mark_gpu_failed(const AssetId& id);

        // Memory management
        void set_memory_pressure_callback(MemoryPressureCallback cb);

        // Evict LRU assets to free memory
        size_t evict_lru(size_t target_bytes);

        // Unload unused assets
        void unload_unused(float threshold_seconds, float current_time);

        // Get total memory usage
        size_t get_memory_usage() const;

        // Get asset count
        size_t get_asset_count() const;

        // Access asset for use (updates LRU)
        std::shared_ptr<AssetType> access(const AssetId& id);

        // Clear cache
        void clear();

        // Check if asset exists in cache
        bool contains(const AssetId& id) const;

        // Remove asset from cache (used by hot reload)
        bool remove(const AssetId& id);

    private:
        mutable std::shared_mutex mutex;
        std::unordered_map<AssetId, CacheEntry<AssetType>> cache;

        // LRU tracking
        std::vector<LruEntry<AssetType>> lru_list;

        // Memory pressure callback
        MemoryPressureCallback memory_pressure_cb;

        // Current memory usage
        size_t total_memory_usage;

        // Frame counter for LRU tracking
        uint64_t current_frame;

        // Update LRU on access
        void update_lru(const AssetId& id);

        // Remove from LRU list
        void remove_from_lru(const AssetId& id);

        // Evict single asset
        size_t evict_single(const AssetId& id);
    };

    // Type aliases for common asset caches
    using TextureAssetCache = AssetCache<TextureAsset>;
    using MeshAssetCache = AssetCache<MeshAsset>;
    using MaterialAssetCache = AssetCache<MaterialAsset>;

    // Implementation
    template<typename AssetType>
    AssetCache<AssetType>::AssetCache()
        : total_memory_usage(0)
        , current_frame(0)
    {
    }

    template<typename AssetType>
    std::shared_ptr<AssetType> AssetCache<AssetType>::get(const AssetId& id)
    {
        std::unique_lock lock(mutex);

        auto it = cache.find(id);
        if (it != cache.end())
        {
            ++current_frame;
            it->second.access_count++;
            it->second.last_accessed_frame = current_frame;
            update_lru(id);
            return it->second.asset;
        }
        return nullptr;
    }

    template<typename AssetType>
    void AssetCache<AssetType>::insert(const AssetId& id, std::shared_ptr<AssetType> asset)
    {
        std::unique_lock lock(mutex);

        auto& entry = cache[id];
        if (entry.memory_size > 0)
        {
            total_memory_usage = entry.memory_size > total_memory_usage ?
                0 : total_memory_usage - entry.memory_size;
        }

        ++current_frame;
        remove_from_lru(id);

        entry.asset = asset;
        entry.state = LoadState::Loaded;
        entry.memory_size = asset ? asset->calculate_memory_size() : 0;
        entry.last_accessed_frame = current_frame;
        entry.gpu_uploaded = false;

        total_memory_usage += entry.memory_size;

        // Add to LRU list
        lru_list.emplace_back(id, current_frame, entry.memory_size);
    }

    template<typename AssetType>
    LoadState AssetCache<AssetType>::get_state(const AssetId& id) const
    {
        std::shared_lock lock(mutex);

        auto it = cache.find(id);
        if (it != cache.end())
        {
            return it->second.state;
        }
        return LoadState::Unloaded;
    }

    template<typename AssetType>
    void AssetCache<AssetType>::set_state(const AssetId& id, LoadState state)
    {
        std::unique_lock lock(mutex);

        auto it = cache.find(id);
        if (it != cache.end())
        {
            it->second.state = state;
        }
    }

    template<typename AssetType>
    void AssetCache<AssetType>::mark_gpu_uploaded(const AssetId& id)
    {
        std::unique_lock lock(mutex);

        auto it = cache.find(id);
        if (it != cache.end())
        {
            it->second.gpu_uploaded = true;
            it->second.state = LoadState::GpuUploaded;
        }
    }

    template<typename AssetType>
    void AssetCache<AssetType>::mark_gpu_failed(const AssetId& id)
    {
        std::unique_lock lock(mutex);

        auto it = cache.find(id);
        if (it != cache.end())
        {
            it->second.gpu_uploaded = false;
            it->second.state = LoadState::Loaded;  // Keep CPU data for retry
        }
    }

    template<typename AssetType>
    void AssetCache<AssetType>::set_memory_pressure_callback(MemoryPressureCallback cb)
    {
        std::unique_lock lock(mutex);
        memory_pressure_cb = std::move(cb);
    }

    template<typename AssetType>
    size_t AssetCache<AssetType>::evict_lru(size_t target_bytes)
    {
        std::unique_lock lock(mutex);

        size_t freed = 0;
        size_t index = 0;

        // Sort LRU list by last access (oldest first)
        std::sort(lru_list.begin(), lru_list.end(),
            [](const LruEntry<AssetType>& a, const LruEntry<AssetType>& b)
            {
                return a.last_access < b.last_access;
            });

        while (freed < target_bytes && index < lru_list.size())
        {
            const auto& lru_entry = lru_list[index];

            // Only evict if GPU upload is complete
            auto it = cache.find(lru_entry.id);
            if (it != cache.end() && it->second.gpu_uploaded)
            {
                size_t entry_freed = evict_single(lru_entry.id);
                freed += entry_freed;
            }

            index++;
        }

        // Clean up LRU list (remove evicted entries)
        lru_list.erase(
            std::remove_if(lru_list.begin(), lru_list.end(),
                [this](const LruEntry<AssetType>& entry)
                {
                    auto it = cache.find(entry.id);
                    return it == cache.end() || it->second.state == LoadState::Evicted;
                }),
            lru_list.end());

        return freed;
    }

    template<typename AssetType>
    size_t AssetCache<AssetType>::evict_single(const AssetId& id)
    {
        auto it = cache.find(id);
        if (it == cache.end())
        {
            return 0;
        }

        size_t freed = it->second.memory_size;
        it->second.asset.reset();
        it->second.state = LoadState::Evicted;
        it->second.memory_size = 0;
        total_memory_usage = freed > total_memory_usage ? 0 : total_memory_usage - freed;

        return freed;
    }

    template<typename AssetType>
    void AssetCache<AssetType>::unload_unused(float threshold_seconds, float current_time)
    {
        std::unique_lock lock(mutex);

        for (auto& [id, entry] : cache)
        {
            if (entry.state == LoadState::GpuUploaded &&
                (current_time - entry.last_access_time) > threshold_seconds)
            {
                // Can be evicted
                evict_single(id);
            }
        }
    }

    template<typename AssetType>
    size_t AssetCache<AssetType>::get_memory_usage() const
    {
        std::shared_lock lock(mutex);
        return total_memory_usage;
    }

    template<typename AssetType>
    size_t AssetCache<AssetType>::get_asset_count() const
    {
        std::shared_lock lock(mutex);
        return cache.size();
    }

    template<typename AssetType>
    std::shared_ptr<AssetType> AssetCache<AssetType>::access(const AssetId& id)
    {
        std::unique_lock lock(mutex);

        auto it = cache.find(id);
        if (it != cache.end())
        {
            ++current_frame;
            it->second.access_count++;
            it->second.last_accessed_frame = current_frame;
            update_lru(id);
            return it->second.asset;
        }
        return nullptr;
    }

    template<typename AssetType>
    void AssetCache<AssetType>::clear()
    {
        std::unique_lock lock(mutex);
        cache.clear();
        lru_list.clear();
        total_memory_usage = 0;
    }

    template<typename AssetType>
    bool AssetCache<AssetType>::contains(const AssetId& id) const
    {
        std::shared_lock lock(mutex);
        return cache.find(id) != cache.end();
    }

    template<typename AssetType>
    void AssetCache<AssetType>::update_lru(const AssetId& id)
    {
        // Update last access in LRU list
        for (auto& entry : lru_list)
        {
            if (entry.id == id)
            {
                entry.last_access = current_frame;
                return;
            }
        }

        auto it = cache.find(id);
        if (it != cache.end())
            lru_list.emplace_back(id, current_frame, it->second.memory_size);
    }

    template<typename AssetType>
    void AssetCache<AssetType>::remove_from_lru(const AssetId& id)
    {
        lru_list.erase(
            std::remove_if(lru_list.begin(), lru_list.end(),
                [&id](const LruEntry<AssetType>& entry) { return entry.id == id; }),
            lru_list.end());
    }

    template<typename AssetType>
    bool AssetCache<AssetType>::remove(const AssetId& id)
    {
        std::unique_lock lock(mutex);

        auto it = cache.find(id);
        if (it == cache.end())
            return false;

        total_memory_usage = it->second.memory_size > total_memory_usage ?
            0 : total_memory_usage - it->second.memory_size;
        remove_from_lru(id);
        cache.erase(it);
        return true;
    }
}
