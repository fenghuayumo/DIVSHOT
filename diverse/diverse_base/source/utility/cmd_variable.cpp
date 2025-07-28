#include "cmd_variable.h"

namespace diverse
{
    CmdVariable::CmdVariable(const char* text,i32 default_value, const std::string& desciption)
        :cmd_text(text), value(default_value), desc(desciption)
    {
        add_to_mgr(cmd_text);
    }

    CmdVariable::CmdVariable(const char* text,f32 default_value, const std::string& desciption)
       :cmd_text(text), value(default_value), desc(desciption)
    {
        add_to_mgr(cmd_text);
    }

    CmdVariable::CmdVariable(const char* text,f64 default_value, const std::string& desciption)
        :cmd_text(text), value(default_value), desc(desciption)
    {
        add_to_mgr(cmd_text);
    }

    CmdVariable::CmdVariable(const char* text,bool default_value, const std::string& desciption)
        :cmd_text(text), value(default_value), desc(desciption)
    {
        add_to_mgr(cmd_text);
    }

    CmdVariable::CmdVariable(const char* text,u32 default_value, const std::string& desciption)
        :cmd_text(text), value(default_value), desc(desciption)
    {
        add_to_mgr(cmd_text);
    }
    CmdVariable::CmdVariable(const char* text,i64 default_value, const std::string& desciption)
        :cmd_text(text), value(default_value), desc(desciption)
    {
        add_to_mgr(cmd_text);
    }

    CmdVariable::CmdVariable(const char* text,u8  default_value, const std::string& desciption)
        :cmd_text(text), value(default_value), desc(desciption)
    {
        add_to_mgr(cmd_text);
    }
    
    void CmdVariable::add_to_mgr(const std::string& cmd_text)
    {
        CmadVariableMgr::get().add(cmd_text, this);
    }

    void CmadVariableMgr::add(const std::string& cmd_text,CmdVariable* v)
    {
        if(v)
        {
            if(cmd_variables.find(cmd_text) == cmd_variables.end())
                cmd_variables[cmd_text] = v;
        }
    }

    CmdVariable* CmadVariableMgr::find(const std::string& text)
    {
        if(cmd_variables.find(text) == cmd_variables.end())
                return nullptr;
        return cmd_variables[text];
    }
}