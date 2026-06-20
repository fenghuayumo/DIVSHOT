#include "asset_loader.h"
#include "cpu_assets.h"
#include <algorithm>

namespace diverse
{
    // AssetThreadPool implementation
    AssetThreadPool::AssetThreadPool(size_t num_threads)
        : stop(false)
    {
        for (size_t i = 0; i < num_threads; ++i)
        {
            workers.emplace_back([this]
            {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(queue_mutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty())
                        {
                            return;
                        }
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    AssetThreadPool::~AssetThreadPool()
    {
        {
            std::unique_lock lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    size_t AssetThreadPool::get_queue_size() const
    {
        std::unique_lock lock(queue_mutex);
        return tasks.size();
    }

    void AssetThreadPool::wait_for_all()
    {
        while (get_queue_size() > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Default asset implementations

    // Create default white texture
    template<>
    std::shared_ptr<TextureAsset> TextureLoader::get_default_asset() const
    {
        if (default_asset)
        {
            return default_asset;
        }

        // Create 1x1 white texture
        auto texture = std::make_shared<TextureAsset>();
        texture->extent[0] = 1;
        texture->extent[1] = 1;
        texture->extent[2] = 1;
        texture->format = PixelFormat::R8G8B8A8_UNorm;

        MipData mip(1, 1);
        mip.data.resize(4, 255);  // White
        texture->mips.push_back(mip);

        texture->cpu_memory_size = texture->calculate_memory_size();

        return texture;
    }

    // Create default black texture
    inline std::shared_ptr<TextureAsset> create_default_black_texture()
    {
        auto texture = std::make_shared<TextureAsset>();
        texture->extent[0] = 1;
        texture->extent[1] = 1;
        texture->extent[2] = 1;
        texture->format = PixelFormat::R8G8B8A8_UNorm;

        MipData mip(1, 1);
        mip.data.resize(4, 0);  // Black
        texture->mips.push_back(mip);

        texture->cpu_memory_size = texture->calculate_memory_size();
        return texture;
    }

    // Create default normal texture (flat normal: 127, 127, 255, 255)
    inline std::shared_ptr<TextureAsset> create_default_normal_texture()
    {
        auto texture = std::make_shared<TextureAsset>();
        texture->extent[0] = 1;
        texture->extent[1] = 1;
        texture->extent[2] = 1;
        texture->format = PixelFormat::R8G8B8A8_UNorm;

        MipData mip(1, 1);
        mip.data = { 127, 127, 255, 255 };  // Flat normal
        texture->mips.push_back(mip);

        texture->cpu_memory_size = texture->calculate_memory_size();
        return texture;
    }

    // Create default mesh (simple quad)
    template<>
    std::shared_ptr<MeshAsset> MeshLoader::get_default_asset() const
    {
        if (default_asset)
        {
            return default_asset;
        }

        auto mesh = std::make_shared<MeshAsset>();

        // Create a simple quad
        mesh->vertices.resize(4);
        mesh->vertices[0].Position = glm::vec3(-0.5f, -0.5f, 0.0f);
        mesh->vertices[0].TexCoords = glm::vec2(0.0f, 0.0f);
        mesh->vertices[1].Position = glm::vec3(0.5f, -0.5f, 0.0f);
        mesh->vertices[1].TexCoords = glm::vec2(1.0f, 0.0f);
        mesh->vertices[2].Position = glm::vec3(0.5f, 0.5f, 0.0f);
        mesh->vertices[2].TexCoords = glm::vec2(1.0f, 1.0f);
        mesh->vertices[3].Position = glm::vec3(-0.5f, 0.5f, 0.0f);
        mesh->vertices[3].TexCoords = glm::vec2(0.0f, 1.0f);

        // Generate normals
        for (auto& vertex : mesh->vertices)
        {
            vertex.Normal = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        // Triangle indices
        mesh->indices = { 0, 1, 2, 0, 2, 3 };

        mesh->calculate_bounding_box();
        mesh->cpu_memory_size = mesh->calculate_memory_size();

        return mesh;
    }

    // Explicit template instantiation
    template class AssetLoader<TextureAsset>;
    template class AssetLoader<MeshAsset>;
}
