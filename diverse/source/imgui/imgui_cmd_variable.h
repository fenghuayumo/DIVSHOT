#include "core/cmd_variable.h"
#include <imgui/Plugins/imcmd_command_palette.h>
namespace diverse
{
    template<typename T>
    class ImCmdVariable : CmdVariable
    {
    public:
        ImCmdVariable(const char* cmd_text,T v,const std::string& desc = {})
            : CmdVariable(v, desc)
        {
            // cmd.Name            = cmd_text;
            // cmd.InitialCallback = [&](){

            // }
            // cmd.SubsequentCallback = [&](int selected_option)
            // {
            // };
            // ImCmd::AddCommand(std::move(cmd));
        }
    protected:
        // ImCmd::Command cmd;
    };
}