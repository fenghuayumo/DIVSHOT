#pragma once

#include "core/core.h"

namespace diverse
{
    class CommandLine;

    // Low-level System operations
    namespace coresystem
    {
        bool init(int argc = 0, char** argv = nullptr);
        void shut_down();

        CommandLine* get_command_line();
    };

};