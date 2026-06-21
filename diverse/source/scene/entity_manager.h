#pragma once

#include "entity.h"

DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entt.hpp>
DISABLE_WARNING_POP

namespace diverse
{

    class Scene;
    class Entity;

    template <typename... Component>
    struct EntityView
    {
        class iterator;
        using view_type = entt::view<entt::get_t<Component...>>;

    public:
        EntityView(Scene* scene);

        Entity operator[](int i)
        {
            DS_ASSERT(i < size(), "Index out of range on Entity View");
            return Entity(m_view[i], m_scene);
        }

        bool empty() const { return m_view.empty(); }
        size_t size() const { return m_view.size(); }
        Entity front() { return Entity(m_view[0], m_scene); }

        iterator begin();
        iterator end();

        class iterator
        {
        public:
            using iterator_category = std::output_iterator_tag;
            using value_type        = Entity;
            using difference_type   = std::ptrdiff_t;
            using pointer           = Entity*;
            using reference         = Entity&;

            explicit iterator(EntityView<Component...>& view, size_t index = 0)
                : view(view)
                , index(index)
            {
            }

            Entity operator*() const
            {
                return view[int(index)];
            }

            iterator& operator++()
            {
                index++;
                return *this;
            }

            iterator operator++(int)
            {
                iterator temp = *this;
                ++(*this);
                return temp;
            }

            bool operator!=(const iterator& rhs) const
            {
                return index != rhs.index;
            }

        private:
            size_t index = 0;
            EntityView<Component...>& view;
        };

        Scene* m_scene;
        view_type m_view;
    };

    template <typename... Component>
    EntityView<Component...>::EntityView(Scene* scene)
        : m_scene(scene)
        , m_view(scene->get_registry().view<Component...>())
    {
    }

    template <typename... Component>
    typename EntityView<Component...>::iterator EntityView<Component...>::begin()
    {
        return EntityView<Component...>::iterator(*this, 0);
    }

    template <typename... Component>
    typename EntityView<Component...>::iterator EntityView<Component...>::end()
    {
        return EntityView<Component...>::iterator(*this, size());
    }

    template <typename...>
    struct TypeList
    {
    };

    class EntityManager
    {
    public:
        EntityManager(Scene* scene)
            : m_scene(scene)
        {
            m_registry = {};
        }

        Entity create();
        Entity create(const std::string& name);

        template <typename... Components>
        auto get_entities_with_types()
        {
            return m_registry.group<Components...>();
        }

        template <typename... Component>
        EntityView<Component...> get_entities_with_type()
        {
            return EntityView<Component...>(m_scene);
        }

        template <typename R, typename T>
        void add_dependency()
        {
            m_registry.template on_construct<R>().template connect<&entt::registry::get_or_emplace<T>>();
        }

        entt::registry& get_registry()
        {
            return m_registry;
        }

        void clear();

        Entity get_entity_by_uuid(uint64_t id);

    private:
        Scene* m_scene = nullptr;
        entt::registry m_registry;
    };
}
