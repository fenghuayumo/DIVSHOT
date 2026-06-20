#pragma once

#include "asset_id.h"
#include "asset_cache.h"
#include "asset_metadata.h"
#include "cpu_assets.h"
#include <future>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <queue>
#include <atomic>

namespace diverse
{
    // Load result for async operations
    struct LoadResult
    {
        bool success;
        AssetId id;
        std::string error_message;

        LoadResult()
            : success(false)
        {}

        static LoadResult ok(const AssetId& id)
        {
            LoadResult result;
            result.success = true;
            result.id = id;
            return result;
        }

        static LoadResult failed(const AssetId& id, const std::string& error)
        {
            LoadResult result;
            result.success = false;
            result.id = id;
            result.error_message = error;
            return result;
        }
    };

    // Load function type
    template<typename AssetType>
    using LoadFunc = std::function<std::shared_ptr<AssetType>(const AssetId&, const AssetMetadata&)>;

    // Thread pool for async loading
    class AssetThreadPool
    {
    public:
        explicit AssetThreadPool(size_t num_threads = std::thread::hardware_concurrency());
        ~AssetThreadPool();

        template<typename F>
        auto enqueue(F&& f) -> std::future<typename std::invoke_result<F>::type>;

        size_t get_queue_size() const;
        void wait_for_all();

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        mutable std::mutex queue_mutex;
        std::condition_variable condition;
        std::atomic<bool> stop;
    };

    // AssetLoader - Async asset loading with failure fallback
    // Responsibilities:
    // - Async IO from disk
    // - Asset decoding
    // - Failure fallback with default assets
    // - Thread pool management
    template<typename AssetType>
    class AssetLoader
    {
    public:
        AssetLoader();
        ~AssetLoader();

        // Register load function for asset type
        void set_load_func(LoadFunc<AssetType> func);

        using LoadedCallback = std::function<void(const AssetId&, std::shared_ptr<AssetType>)>;
        void set_on_loaded(LoadedCallback callback);

        // Async load with future result
        std::future<LoadResult> load(const AssetId& id, const AssetMetadata& metadata);

        // Check if load is in progress
        bool is_loading(const AssetId& id) const;

        // Get default asset on failure
        std::shared_ptr<AssetType> get_default_asset() const;

        // Set default asset
        void set_default_asset(std::shared_ptr<AssetType> asset);

        // Get pending load count
        size_t get_pending_count() const;

        // Wait for all pending loads
        void wait_for_all();

    private:
        LoadFunc<AssetType> load_func;
        LoadedCallback on_loaded;

        // Default asset for fallback
        std::shared_ptr<AssetType> default_asset;

        // Loading state tracking
        mutable std::mutex loading_mutex;
        std::unordered_set<AssetId> loading_assets;

        // Thread pool for IO
        std::shared_ptr<AssetThreadPool> thread_pool;
    };

    // Type aliases
    using TextureLoader = AssetLoader<TextureAsset>;
    using MeshLoader = AssetLoader<MeshAsset>;

    // Specialization for common asset types
    template<>
    std::shared_ptr<TextureAsset> TextureLoader::get_default_asset() const;

    template<>
    std::shared_ptr<MeshAsset> MeshLoader::get_default_asset() const;

    // Implementation
    template<typename F>
    auto AssetThreadPool::enqueue(F&& f) -> std::future<typename std::invoke_result<F>::type>
    {
        using return_type = typename std::invoke_result<F>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
        std::future<return_type> result = task->get_future();

        {
            std::unique_lock lock(queue_mutex);
            if (stop)
            {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks.emplace([task]() { (*task)(); });
        }

        condition.notify_one();
        return result;
    }

    template<typename AssetType>
    AssetLoader<AssetType>::AssetLoader()
        : thread_pool(std::make_shared<AssetThreadPool>(4))  // 4 IO threads
    {
    }

    template<typename AssetType>
    AssetLoader<AssetType>::~AssetLoader()
    {
        wait_for_all();
    }

    template<typename AssetType>
    void AssetLoader<AssetType>::set_load_func(LoadFunc<AssetType> func)
    {
        load_func = std::move(func);
    }

    template<typename AssetType>
    void AssetLoader<AssetType>::set_on_loaded(LoadedCallback callback)
    {
        on_loaded = std::move(callback);
    }

    template<typename AssetType>
    std::future<LoadResult> AssetLoader<AssetType>::load(const AssetId& id, const AssetMetadata& metadata)
    {
        {
            std::lock_guard lock(loading_mutex);
            loading_assets.insert(id);
        }

        return thread_pool->enqueue([this, id, metadata]() -> LoadResult
        {
            try
            {
                if (!load_func)
                {
                    return LoadResult::failed(id, "No load function registered");
                }

                auto asset = load_func(id, metadata);
                if (!asset)
                {
                    return LoadResult::failed(id, "Load function returned null asset");
                }

                if (on_loaded)
                    on_loaded(id, asset);

                {
                    std::lock_guard lock(loading_mutex);
                    loading_assets.erase(id);
                }

                return LoadResult::ok(id);
            }
            catch (const std::exception& e)
            {
                {
                    std::lock_guard lock(loading_mutex);
                    loading_assets.erase(id);
                }
                return LoadResult::failed(id, e.what());
            }
        });
    }

    template<typename AssetType>
    bool AssetLoader<AssetType>::is_loading(const AssetId& id) const
    {
        std::lock_guard lock(loading_mutex);
        return loading_assets.find(id) != loading_assets.end();
    }

    template<typename AssetType>
    std::shared_ptr<AssetType> AssetLoader<AssetType>::get_default_asset() const
    {
        return default_asset;
    }

    template<typename AssetType>
    void AssetLoader<AssetType>::set_default_asset(std::shared_ptr<AssetType> asset)
    {
        default_asset = asset;
    }

    template<typename AssetType>
    size_t AssetLoader<AssetType>::get_pending_count() const
    {
        std::lock_guard lock(loading_mutex);
        return loading_assets.size();
    }

    template<typename AssetType>
    void AssetLoader<AssetType>::wait_for_all()
    {
        while (get_pending_count() > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
