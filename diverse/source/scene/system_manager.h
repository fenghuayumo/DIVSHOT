#pragma once
#include "scene/isystem.h"
#include <imgui/imgui.h>
#include <unordered_map>

namespace diverse
{
    class SystemManager
    {
    public:
        template <typename T, typename... Args>
        SharedPtr<T> register_system(Args&&... args)
        {
            auto typeName = typeid(T).hash_code();

            DS_ASSERT((m_Systems.find(typeName) == m_Systems.end()), "Registering system more than once.");

            // Create a pointer to the system and return it so it can be used externally
            SharedPtr<T> system = createSharedPtr<T>(std::forward<Args>(args)...);
            m_Systems.insert({ typeName, std::move(system) });
            return system;
        }

        template <typename T>
        SharedPtr<T> register_system(T* t)
        {
            auto typeName = typeid(T).hash_code();

            DS_ASSERT(m_Systems.find(typeName) == m_Systems.end(), "Registering system more than once.");

            // Create a pointer to the system and return it so it can be used externally
            SharedPtr<T> system = SharedPtr<T>(t);
            m_Systems.insert({ typeName, std::move(system) });
            return system;
        }

        template <typename T>
        void remove_system()
        {
            auto typeName = typeid(T).hash_code();

            if(m_Systems.find(typeName) != m_Systems.end())
            {
                m_Systems.erase(typeName);
            }
        }

        template <typename T>
        T* get_system()
        {
            auto typeName = typeid(T).hash_code();

            if(m_Systems.find(typeName) != m_Systems.end())
            {
                return dynamic_cast<T*>(m_Systems[typeName].get());
            }

            return nullptr;
        }

        template <typename T>
        T* has_system()
        {
            auto typeName = typeid(T).hash_code();

            return m_Systems.find(typeName) != m_Systems.end();
        }

        void on_update(const TimeStep& dt, Scene* scene)
        {
            for(auto& system : m_Systems)
                system.second->on_update(dt, scene);
        }

        void on_imgui()
        {
            for(auto& system : m_Systems)
            {
                if(ImGui::TreeNode(system.second->get_name().c_str()))
                {
                    system.second->on_imgui();
                    ImGui::TreePop();
                }
            }
        }

        void on_debug_draw()
        {
            for(auto& system : m_Systems)
                system.second->on_debug_draw();
        }

    private:
        // Map from system type string pointer to a system pointer
        std::unordered_map<size_t, SharedPtr<ISystem>> m_Systems;
    };
}
