#include "ipc_server.h"

namespace diverse::editor_api
{
    bool IpcServer::start(uint16_t port)
    {
        port_ = port;
        running_ = true;
        // TODO: bind localhost TCP socket and dispatch JSON-RPC to editor_api C functions.
        return true;
    }

    void IpcServer::stop()
    {
        running_ = false;
        port_ = 0;
    }

    void IpcServer::poll()
    {
        if (!running_)
            return;

        // TODO: accept connections, read requests, write responses.
    }
}
