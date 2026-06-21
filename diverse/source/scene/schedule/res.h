#pragma once

#include "world_context.h"
#include <stdexcept>

namespace diverse::schedule
{

    /**
     * @brief Immutable resource accessor.
     *
     * Provides read-only access to global resources or singletons.
     * Use ResMut<T> if you need mutable access.
     *
     * @tparam T The resource type
     */
    template<typename T>
    class Res
    {
    public:
        explicit Res(WorldContext* context)
            : m_context(context)
        {
        }

        /**
         * @brief Get the resource (read-only)
         *
         * @return Pointer to the resource, or nullptr if not found
         */
        const T* get() const
        {
            if constexpr (std::is_same_v<T, TimeStep>)
            {
                return m_context->timestep();
            }
            else if constexpr (std::is_same_v<T, Scene>)
            {
                return m_context->scene();
            }
            else
            {
                return m_context->get_resource<T>();
            }
        }

        /**
         * @brief Get the resource (read-only)
         *
         * @throws std::runtime_error if resource not found
         */
        const T& operator*() const
        {
            const T* ptr = get();
            if (!ptr)
            {
                throw std::runtime_error("Resource not found");
            }
            return *ptr;
        }

        /**
         * @brief Access resource members
         *
         * @throws std::runtime_error if resource not found
         */
        const T* operator->() const
        {
            const T* ptr = get();
            if (!ptr)
            {
                throw std::runtime_error("Resource not found");
            }
            return ptr;
        }

        /**
         * @brief Check if resource exists
         */
        bool exists() const
        {
            return get() != nullptr;
        }

        /**
         * @brief Get the world context
         */
        WorldContext* context() const { return m_context; }

    private:
        WorldContext* m_context;
    };

    /**
     * @brief Mutable resource accessor.
     *
     * Provides read-write access to global resources or singletons.
     *
     * @tparam T The resource type
     */
    template<typename T>
    class ResMut
    {
    public:
        explicit ResMut(WorldContext* context)
            : m_context(context)
        {
        }

        /**
         * @brief Get the resource (read-write)
         *
         * @return Pointer to the resource, or nullptr if not found
         */
        T* get()
        {
            if constexpr (std::is_same_v<T, TimeStep>)
            {
                return m_context->timestep();
            }
            else if constexpr (std::is_same_v<T, Scene>)
            {
                return m_context->scene();
            }
            else
            {
                return m_context->get_resource<T>();
            }
        }

        /**
         * @brief Get the resource (read-only)
         */
        const T* get() const
        {
            if constexpr (std::is_same_v<T, TimeStep>)
            {
                return m_context->timestep();
            }
            else if constexpr (std::is_same_v<T, Scene>)
            {
                return m_context->scene();
            }
            else
            {
                return m_context->get_resource<T>();
            }
        }

        /**
         * @brief Get the resource (read-write)
         *
         * @throws std::runtime_error if resource not found
         */
        T& operator*()
        {
            T* ptr = get();
            if (!ptr)
            {
                throw std::runtime_error("Resource not found");
            }
            return *ptr;
        }

        /**
         * @brief Get the resource (read-only)
         *
         * @throws std::runtime_error if resource not found
         */
        const T& operator*() const
        {
            const T* ptr = get();
            if (!ptr)
            {
                throw std::runtime_error("Resource not found");
            }
            return *ptr;
        }

        /**
         * @brief Access resource members (read-write)
         *
         * @throws std::runtime_error if resource not found
         */
        T* operator->()
        {
            T* ptr = get();
            if (!ptr)
            {
                throw std::runtime_error("Resource not found");
            }
            return ptr;
        }

        /**
         * @brief Access resource members (read-only)
         *
         * @throws std::runtime_error if resource not found
         */
        const T* operator->() const
        {
            const T* ptr = get();
            if (!ptr)
            {
                throw std::runtime_error("Resource not found");
            }
            return ptr;
        }

        /**
         * @brief Check if resource exists
         */
        bool exists() const
        {
            return get() != nullptr;
        }

        /**
         * @brief Get the world context
         */
        WorldContext* context() const { return m_context; }

    private:
        WorldContext* m_context;
    };

} // namespace diverse::schedule
