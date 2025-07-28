#include "engine/engine.h"
#include "asset.h"
#include "utility/singleton.h"
#include "assets/texture.h"
#include "assets/material.h"
#include "assets/mesh_model.h"
#include "assets/gaussian_model.h"
#include "assets/point_cloud.h"
#include "core/reference.h"
#include <mutex>
#include <thread>
#include <future>
namespace diverse
{
    template <typename T>
    class ResourceManager : public ThreadSafeSingleton<ResourceManager<T>>
    {
    public:
        typedef T Type;
        typedef std::string IDType;
        typedef SharedPtr<T> ResourceHandle;

        struct Resource
        {
            float time_since_reload = 0.0f;
            float last_accessed = 0.0f;
            ResourceHandle data;
            bool onDisk = false;
        };

        typedef std::unordered_map<IDType, Resource> MapType;

        typedef std::function<bool(const IDType&, ResourceHandle&)> LoadFunc;
        typedef std::function<void(ResourceHandle&)> ReleaseFunc;
        typedef std::function<bool(const IDType&, ResourceHandle&)> ReloadFunc;
        typedef std::function<IDType(const ResourceHandle&)> GetIdFunc;

        //get resource, if the resource does not exist, it will be loaded asynchronously, and a resource ptr will be returned
        //so if you want to use the resource, you must attention the synchronization of the resource ptr, 
        //you can check whether the asset' flag is AssetFlag::Loaded to confirm the resource is ready to use
        ResourceHandle get_resource(const IDType& name)
        {
            std::lock_guard lock(lock_mutex);
            typename MapType::iterator itr = name_resource_map.find(name);
            if (itr != name_resource_map.end())
            {
                itr->second.last_accessed = (float)Engine::get_time_step().get_elapsed_seconds();
                return itr->second.data;
            }
            if (!load_func)
            {
                throw std::runtime_error("load_func is empty!");
            }
            ResourceHandle resourceData;
            if (!load_func(name, resourceData))
            {
                DS_LOG_ERROR("Resource Manager could not load resource name {0} of type {1}", name, typeid(T).name());
                return ResourceHandle(nullptr);
            }

            Resource newResource;
            newResource.data = resourceData;
            newResource.time_since_reload = 0;
            newResource.onDisk = true;
            newResource.last_accessed = (float)Engine::get_time_step().get_elapsed_seconds();

            name_resource_map.emplace(name, newResource);

            return resourceData;
        }

        ResourceHandle get_resource(const ResourceHandle& data)
        {
            std::lock_guard lock(lock_mutex);
            IDType newId = get_id_func(data);

            typename MapType::iterator itr = name_resource_map.find(newId);
            if (itr == name_resource_map.end())
            {
                ResourceHandle resourceData = data;

                Resource newResource;
                newResource.data = resourceData;
                newResource.time_since_reload = 0;
                newResource.onDisk = false;
                name_resource_map.emplace(newId, newResource);

                return resourceData;
            }

            return itr->second.data;
        }

        void add_resource(const IDType& name, ResourceHandle data)
        {
            std::lock_guard lock(lock_mutex);
            typename MapType::iterator itr = name_resource_map.find(name);
            if (itr != name_resource_map.end())
            {
                itr->second.last_accessed = (float)Engine::get_time_step().get_elapsed_seconds();
                itr->second.data = data;
            }

            Resource newResource;
            newResource.data = data;
            newResource.time_since_reload = 0;
            newResource.onDisk = true;
            newResource.last_accessed = (float)Engine::get_time_step().get_elapsed_seconds();

            name_resource_map.emplace(name, newResource);
        }

        virtual void destroy()
        {
            typename MapType::iterator itr = name_resource_map.begin();

            if (release_func)
            {
                while (itr != name_resource_map.end())
                {
                    release_func((itr->second.data));
                    ++itr;
                }
            }
            name_resource_map.clear();
        }

        void update(const float elapsedSeconds)
        {
            std::lock_guard lock(lock_mutex);
            typename MapType::iterator itr = name_resource_map.begin();

            static IDType keysToDelete[256];
            std::size_t keysToDeleteCount = 0;

            for (auto&& [key, value] : name_resource_map)
            {
                if (value.data.use_count() == 1 && expiration_time < (elapsedSeconds - itr->second.last_accessed))
                {
                    keysToDelete[keysToDeleteCount] = key;
                    keysToDeleteCount++;
                }
            }

            for (std::size_t i = 0; i < keysToDeleteCount; i++)
            {
                name_resource_map.erase(keysToDelete[i]);
            }
        }

        bool reload_resources()
        {
            typename MapType::iterator itr = name_resource_map.begin();
            while (itr != name_resource_map.end())
            {
                itr->second.timeSinceReload = 0;
                if (!reload_func(itr->first, (itr->second.data)))
                {
                    DS_LOG_ERROR("Resource Manager could not reload resource name {0} of type {1}", itr->first, typeid(T).name());
                }
                ++itr;
            }
            return true;
        }

        bool resource_exists(const IDType& name)
        {
            typename MapType::iterator itr = name_resource_map.find(name);
            return itr != name_resource_map.end();
        }

        ResourceHandle operator[](const IDType& name)
        {
            return get_resource(name);
        }

        LoadFunc& LoadFunction() { return load_func; }
        ReleaseFunc& ReleaseFunction() { return release_func; }
        ReloadFunc& ReloadFunction() { return reload_func; }

        ResourceHandle  get_default_resource(const IDType& name)
        {
            if(default_resources.find(name) != default_resources.end())
                return default_resources[name];
            return {};
        }

        void  add_default_resource(const IDType& name, ResourceHandle data)
        {
            std::lock_guard lock(lock_mutex);
            typename std::unordered_map<IDType, ResourceHandle>::iterator itr = default_resources.find(name);
            if (itr == default_resources.end())
            {
                default_resources[name] = data;
            }
        }
        ResourceManager()
        {
            if constexpr (std::is_same_v<T, asset::Texture>)
			{
                futures.clear();
                auto gray_img = asset::RawImage{ PixelFormat::R32G32B32A32_Float, {1, 1}, std::vector<u8>(16, 0) };
                gray_img.put(0, 0, std::array<f32, 4>{ 0.5f, 0.5f, 0.5f, 1.0f });
                auto gray_hdr = createSharedPtr<asset::Texture>(gray_img);
                add_default_resource("gray_hdr", gray_hdr);

                auto black = createSharedPtr<asset::Texture>(asset::Texture::create_black_texture());
                auto white = createSharedPtr<asset::Texture>(asset::Texture::create_white_texture());
                auto normal = createSharedPtr<asset::Texture>(asset::Texture::create_rgb_texture({ 127, 127, 255, 255 }));
                add_default_resource("black", black);
                add_default_resource("white", white);
                add_default_resource("normal", normal);
                auto load_texture = [&](const std::string& filePath, SharedPtr<asset::Texture>& texture) {
                    texture = createSharedPtr<asset::Texture>();
                    futures.emplace_back(std::async(std::launch::async, [filePath, texture]()->void {
                        texture->init_from_path(filePath);
                    }));
                    return true;
                };
				load_func = load_texture;
				release_func = [](ResourceHandle& data) {data.reset(); };
				reload_func = load_texture;
				//get_id_func = [](const ResourceHandle& data) {return data->get_name(); };
			}
			else if constexpr (std::is_same_v<T, Material>)
			{
                futures.clear();
                auto material = createSharedPtr<Material>();
                PBRMataterialTextures textures;
                material->set_textures(textures);
                material->set_name("defalut");
                add_default_resource("default", material);
                auto load_material = [&](const std::string& filePath, SharedPtr<Material>& material) {
                    material = createSharedPtr<Material>();
                    futures.emplace_back(std::async(std::launch::async, [filePath, material]()->void {
                        material->load_material(filePath, filePath);
                        }));
                    return true;
                };
				load_func = load_material;
				release_func = [](ResourceHandle& data) {data.reset(); };
				reload_func = load_material;
				//get_id_func = [](const ResourceHandle& data) {return data->get_name(); };
			}
			else if constexpr (std::is_same_v<T, MeshModel>)
			{
                auto load_mesh_model = [&](const std::string & filePath, SharedPtr<MeshModel>&meshModel) {
                    meshModel = createSharedPtr<MeshModel>();
                    meshModel->set_primitive_type(PrimitiveType::File);

                    futures.emplace_back(std::async(std::launch::async, [filePath, meshModel]()->void {
                        meshModel->load_model(filePath);
                        }));
                    return true;
				};
				load_func = load_mesh_model;
				release_func = [](ResourceHandle& data) {data.reset(); };
				reload_func = load_mesh_model;
				//get_id_func = [](const ResourceHandle& data) {return data->get_name(); };
			}
            else if constexpr (std::is_same_v<T, PointCloud>) {
                auto load_asset = [&](const std::string & filePath, SharedPtr<PointCloud>& meshModel) {
                    meshModel = createSharedPtr<PointCloud>();

                    futures.emplace_back(std::async(std::launch::async, [filePath, meshModel]()->void {
                        meshModel->load(filePath);
                    }));
                    return true;
				};
				load_func = load_asset;
				release_func = [](ResourceHandle& data) {data.reset(); };
				reload_func = load_asset;
            }
        }
    protected:
        MapType name_resource_map = {};
        LoadFunc load_func;
        ReleaseFunc release_func;
        ReloadFunc reload_func;
        GetIdFunc get_id_func;
        float expiration_time = 3.0f;
        std::unordered_map<IDType, ResourceHandle>   default_resources;
        std::mutex  lock_mutex;

        std::vector<std::future<void>> futures;
    };
}