#pragma once

#include <cstdint>

namespace diverse::editor_api
{
    /** JSON-RPC over TCP (localhost). Implementation is stubbed in Phase 0. */
    class IpcServer
    {
    public:
        bool start(uint16_t port);
        void stop();
        void poll();

        bool is_running() const { return running_; }

    private:
        bool running_ = false;
        uint16_t port_ = 0;
    };
}
