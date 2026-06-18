/**
 * Sidecar entry point for Tauri externalBin ("divshot-engine").
 * Phase 0: minimal loop that exposes editor-api over IPC.
 */

#include <editor_api/editor_api.h>

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

namespace
{
    struct CliOptions
    {
        const char* project_root = ".";
        uint16_t ipc_port = 17350;
        int width = 1280;
        int height = 720;
    };

    void print_usage()
    {
        std::fprintf(stderr,
            "divshot-engine (editor-api sidecar)\n"
            "  --project <path>   project root (default: .)\n"
            "  --ipc-port <port>  JSON-RPC port (default: 17350)\n"
            "  --width <px>       viewport width hint\n"
            "  --height <px>      viewport height hint\n");
    }

    bool parse_args(int argc, char** argv, CliOptions& out)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc)
                out.project_root = argv[++i];
            else if (std::strcmp(argv[i], "--ipc-port") == 0 && i + 1 < argc)
                out.ipc_port = static_cast<uint16_t>(std::atoi(argv[++i]));
            else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc)
                out.width = std::atoi(argv[++i]);
            else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc)
                out.height = std::atoi(argv[++i]);
            else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
            {
                print_usage();
                return false;
            }
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    CliOptions options;
    if (!parse_args(argc, argv, options))
        return 0;

    EditorInitParams params{};
    params.project_root = options.project_root;
    params.render_api = "Vulkan";
    params.width = options.width;
    params.height = options.height;
    params.flags = EditorApiFlag_EnableIpc | EditorApiFlag_DisableImGui;

    if (!editor_initialize(&params))
    {
        std::fprintf(stderr, "editor_initialize failed: %s\n", editor_get_last_error());
        return 1;
    }

    if (!editor_ipc_start(options.ipc_port))
    {
        std::fprintf(stderr, "editor_ipc_start failed: %s\n", editor_get_last_error());
        editor_shutdown();
        return 1;
    }

    std::printf("divshot-engine ready (api %s, ipc %u)\n", editor_get_version(), options.ipc_port);

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    while (true)
    {
        auto now = clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        editor_ipc_poll();
        if (!editor_tick(dt))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    editor_ipc_stop();
    editor_shutdown();
    return 0;
}
