#pragma once
#include "core/core.h"

#include <mutex>
#include <thread>

namespace diverse
{
    namespace threading
    {
        namespace detail
        {
            inline std::mutex thread_mutex;
            inline std::thread::id game_thread_id;
            inline std::thread::id render_thread_id;
            inline std::thread::id rhi_thread_id;

            inline bool is_unset(const std::thread::id& id)
            {
                return id == std::thread::id{};
            }
        }

        inline void mark_game_thread()
        {
            std::lock_guard lock(detail::thread_mutex);
            detail::game_thread_id = std::this_thread::get_id();

            // Until a dedicated render thread is registered, the game thread owns rendering too.
            if (detail::is_unset(detail::render_thread_id))
                detail::render_thread_id = detail::game_thread_id;
            if (detail::is_unset(detail::rhi_thread_id))
                detail::rhi_thread_id = detail::game_thread_id;
        }

        inline void mark_render_thread()
        {
            std::lock_guard lock(detail::thread_mutex);
            detail::render_thread_id = std::this_thread::get_id();

            // Until a dedicated RHI thread is registered, the render thread owns RHI submit.
            if (detail::is_unset(detail::rhi_thread_id) || detail::rhi_thread_id == detail::game_thread_id)
                detail::rhi_thread_id = detail::render_thread_id;
        }

        inline void mark_rhi_thread()
        {
            std::lock_guard lock(detail::thread_mutex);
            detail::rhi_thread_id = std::this_thread::get_id();
        }

        inline bool is_game_thread()
        {
            std::lock_guard lock(detail::thread_mutex);
            return !detail::is_unset(detail::game_thread_id) && detail::game_thread_id == std::this_thread::get_id();
        }

        inline bool is_render_thread()
        {
            std::lock_guard lock(detail::thread_mutex);
            return !detail::is_unset(detail::render_thread_id) && detail::render_thread_id == std::this_thread::get_id();
        }

        inline bool is_rhi_thread()
        {
            std::lock_guard lock(detail::thread_mutex);
            return !detail::is_unset(detail::rhi_thread_id) && detail::rhi_thread_id == std::this_thread::get_id();
        }

        inline void assert_game_thread()
        {
            DS_ASSERT(is_game_thread(), "Expected to run on the game thread");
        }

        inline void assert_render_thread()
        {
            DS_ASSERT(is_render_thread(), "Expected to run on the render thread");
        }

        inline void assert_rhi_thread()
        {
            DS_ASSERT(is_rhi_thread(), "Expected to run on the RHI thread");
        }
    }
}
