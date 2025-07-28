#pragma once

#include "engine/application.h"
#include "core/uuid.h"
#include <string>
#include <map>
#include <cereal/cereal.hpp>

namespace diverse
{
    class Scene;
    class Entity;

    class PythonScriptComponent
    {
    public:
        PythonScriptComponent();
        PythonScriptComponent(const std::string& fileName, Scene* scene);
        ~PythonScriptComponent();

        void init();
        void on_init();
        void on_update(float dt);
        void reload();
        void load(const std::string& fileName);
        Entity get_current_entity();

        // For accessing this component in lua
        void set_this_component();

        void load_script(const std::string& fileName);
      
        const std::string& get_file_path() const
        {
            return file_name;
        }

        void set_file_path(const std::string& path)
        {
            file_name = path;
        }

        const std::map<int, std::string>& GetErrors() const
        {
            return errors;
        }

        template <typename Archive>
        void save(Archive& archive) const
        {
            std::string newPath;
            FileSystem::get().absolute_path_2_fileSystem(file_name, newPath);
            archive(cereal::make_nvp("FilePath", newPath));
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            scene = Application::get().get_current_scene();
            archive(cereal::make_nvp("FilePath", file_name));
            init();
        }
        //
        //        UUID GetUUID() const
        //        {
        //            return m_UUID;
        //        }

    private:
        Scene* scene = nullptr;
        std::string file_name;
        std::map<int, std::string> errors;

    };
}