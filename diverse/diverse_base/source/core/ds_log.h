#pragma once
#include "core.h"

#define DS_ENABLE_LOG 1

#if DS_ENABLE_LOG
#ifdef DS_PLATFORM_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX // For windows.h
#endif
#endif

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

// Core log macros
#define DS_LOG_TRACE(...) SPDLOG_LOGGER_CALL(::diverse::debug::Log::get_core_logger(), spdlog::level::level_enum::trace, __VA_ARGS__)
#define DS_LOG_INFO(...) SPDLOG_LOGGER_CALL(::diverse::debug::Log::get_core_logger(), spdlog::level::level_enum::info, __VA_ARGS__)
#define DS_LOG_WARN(...) SPDLOG_LOGGER_CALL(::diverse::debug::Log::get_core_logger(), spdlog::level::level_enum::warn, __VA_ARGS__)
#define DS_LOG_ERROR(...) SPDLOG_LOGGER_CALL(::diverse::debug::Log::get_core_logger(), spdlog::level::level_enum::err, __VA_ARGS__)
#define DS_LOG_CRITICAL(...) SPDLOG_LOGGER_CALL(::diverse::debug::Log::get_core_logger(), spdlog::level::level_enum::critical, __VA_ARGS__)

#else
namespace spdlog
{
    namespace sinks
    {
        class sink;
    }
    class logger;
}
#define DS_LOG_TRACE(...) ((void)0)
#define DS_LOG_INFO(...) ((void)0)
#define DS_LOG_WARN(...) ((void)0)
#define DS_LOG_ERROR(...) ((void)0)
#define DS_LOG_CRITICAL(...) ((void)0)

#endif

namespace diverse
{
    namespace debug
    {
        class DS_EXPORT Log
        {
        public:
            static void init();
            static void release();

#if DS_ENABLE_LOG
            inline static std::shared_ptr<spdlog::logger>& get_core_logger() { return s_CoreLogger; }
            static void add_sink(std::shared_ptr<spdlog::sinks::sink>& sink);
#endif

        private:
#if DS_ENABLE_LOG
            static std::shared_ptr<spdlog::logger> s_CoreLogger;
#endif
        };
    }
}
