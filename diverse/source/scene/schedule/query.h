#pragma once

#include <entt/entity/registry.hpp>
#include <entt/entity/view.hpp>
#include <functional>
#include <type_traits>
#include <utility>  // For std::index_sequence

namespace diverse::schedule
{

    /**
     * @brief Type-safe query over EnTT components.
     *
     * Provides a Bevy-like API for iterating over entities with specific components.
     * Wraps EnTT's view functionality with a more ergonomic interface.
     *
     * @tparam Components The component types to query for
     */
    template<typename... Components>
    class Query
    {
    public:
        using ViewType = entt::view<entt::get_t<Components...>>;

        /**
         * @brief Construct a query from a registry
         */
        explicit Query(entt::registry& registry)
            : m_registry(registry)
        {
            m_view = registry.view<Components...>();
        }

        /**
         * @brief Construct a query from an existing view
         */
        explicit Query(entt::registry& registry, ViewType view)
            : m_registry(registry)
            , m_view(view)
        {
        }

        ~Query() = default;

        // Non-copyable
        Query(const Query&) = delete;
        Query& operator=(const Query&) = delete;

        // Movable
        Query(Query&&) noexcept = default;
        Query& operator=(Query&&) noexcept = default;

        /**
         * @brief Iterate over all matching entities
         *
         * @param func Function called with (entity, components...) for each entity
         */
        template<typename Func>
        void each(Func&& func)
        {
            m_view.each(std::forward<Func>(func));
        }

        /**
         * @brief Iterate over all matching entities (without entity handle)
         *
         * @param func Function called with (components...) for each entity
         */
        template<typename Func>
        void for_each(Func&& func)
        {
            for (auto entity : m_view)
            {
                invoke(func, entity, std::index_sequence_for<Components...>{});
            }
        }

        /**
         * @brief Get the number of entities matching this query
         */
        size_t size() const
        {
            return m_view.size();
        }

        /**
         * @brief Check if the query is empty
         */
        bool empty() const
        {
            return m_view.empty();
        }

        /**
         * @brief Get the underlying EnTT view
         */
        ViewType& view() { return m_view; }
        const ViewType& view() const { return m_view; }

        /**
         * @brief Get the registry
         */
        entt::registry& registry() { return m_registry; }
        const entt::registry& registry() const { return m_registry; }

        /**
         * @brief Check if a specific entity matches this query
         */
        bool contains(entt::entity entity) const
        {
            return m_view.contains(entity);
        }

        /**
         * @brief Get a single entity (first match)
         *
         * @param func Function called with (entity, components...) for the first entity
         * @return true if an entity was found, false otherwise
         */
        template<typename Func>
        bool single(Func&& func)
        {
            auto it = m_view.begin();
            if (it != m_view.end())
            {
                m_view.get(*it, std::forward<Func>(func));
                return true;
            }
            return false;
        }

    private:
        template<typename Func, size_t... Is>
        void invoke(Func& func, entt::entity entity, std::index_sequence<Is...>)
        {
            func(m_view.get<Components>(entity)...);
        }

        entt::registry& m_registry;
        ViewType m_view;
    };

    /**
     * @brief Query with exclusion filter (simplified version)
     *
     * Allows querying for entities that have certain components
     * but exclude those that have other components.
     *
     * @tparam IncludedComponents Components the entities must have
     */
    template<typename... IncludedComponents>
    class QueryExcluding
    {
    public:
        using ViewType = entt::view<entt::get_t<IncludedComponents...>>;

        explicit QueryExcluding(entt::registry& registry)
            : m_registry(registry)
        {
            m_view = registry.view<IncludedComponents...>();
        }

        ~QueryExcluding() = default;

        // Non-copyable
        QueryExcluding(const QueryExcluding&) = delete;
        QueryExcluding& operator=(const QueryExcluding&) = delete;

        // Movable
        QueryExcluding(QueryExcluding&&) noexcept = default;
        QueryExcluding& operator=(QueryExcluding&&) noexcept = default;

        template<typename Func>
        void each(Func&& func)
        {
            m_view.each(std::forward<Func>(func));
        }

        template<typename Func>
        void for_each(Func&& func)
        {
            for (auto entity : m_view)
            {
                invoke(func, entity, std::index_sequence_for<IncludedComponents...>{});
            }
        }

        size_t size() const { return m_view.size(); }
        bool empty() const { return m_view.empty(); }

        ViewType& view() { return m_view; }
        const ViewType& view() const { return m_view; }

    private:
        template<typename Func, size_t... Is>
        void invoke(Func& func, entt::entity entity, std::index_sequence<Is...>)
        {
            func(m_view.get<IncludedComponents>(entity)...);
        }

        entt::registry& m_registry;
        ViewType m_view;
    };

    /**
     * @brief Helper function to create a query
     */
    template<typename... Components>
    Query<Components...> make_query(entt::registry& registry)
    {
        return Query<Components...>(registry);
    }

    /**
     * @brief Helper function to create a query with exclusion
     */
    template<typename... IncludedComponents>
    auto make_query_excluding(entt::registry& registry)
    {
        return QueryExcluding<IncludedComponents...>(registry);
    }

} // namespace diverse::schedule
