#pragma once

#include "console_panel.h"
#include <spdlog/sinks/base_sink.h>
#include <spdlog/fmt/chrono.h>

namespace diverse
{
    template <typename Mutex>
    class ImGuiConsoleSink : public spdlog::sinks::base_sink<Mutex>
    {
    public:
        explicit ImGuiConsoleSink() {};

        ImGuiConsoleSink(const ImGuiConsoleSink&) = delete;
        ImGuiConsoleSink& operator=(const ImGuiConsoleSink&) = delete;
        virtual ~ImGuiConsoleSink() = default;

        // SPDLog sink interface
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
            std::string source = fmt::format("File : {0} | Function : {1} | Line : {2}", msg.source.filename, msg.source.funcname, msg.source.line);
            const auto time = fmt::localtime(std::chrono::system_clock::to_time_t(msg.time));
            auto processed = fmt::format("{:%H:%M:%S}", time);

            auto message = createSharedPtr<ConsolePanel::Message>(fmt::to_string(formatted), get_message_level(msg.level), source, static_cast<int>(msg.thread_id), processed);
            ConsolePanel::add_message(message);
        }

        static ConsolePanel::Message::Level get_message_level(const spdlog::level::level_enum level)
        {
            switch (level)
            {
            case spdlog::level::level_enum::trace:
                return ConsolePanel::Message::Level::Trace;
            case spdlog::level::level_enum::debug:
                return ConsolePanel::Message::Level::Debug;
            case spdlog::level::level_enum::info:
                return ConsolePanel::Message::Level::Info;
            case spdlog::level::level_enum::warn:
                return ConsolePanel::Message::Level::Warn;
            case spdlog::level::level_enum::err:
                return ConsolePanel::Message::Level::Error;
            case spdlog::level::level_enum::critical:
                return ConsolePanel::Message::Level::Critical;
            }
            return ConsolePanel::Message::Level::Trace;
        }

        void flush_() override
        {
            ConsolePanel::flush();
        };
    };
}

#include "spdlog/details/null_mutex.h"
#include <mutex>
namespace diverse
{
    using ImGuiConsoleSink_mt = ImGuiConsoleSink<std::mutex>;                  // multi-threaded
    using ImGuiConsoleSink_st = ImGuiConsoleSink<spdlog::details::null_mutex>; // single threaded
}
