#pragma once
#include <optional>
#include <any>
#include <string>
#include <vector>
#include <unordered_map>
#include "core/base_type.h"
#include "utility/singleton.h"
namespace diverse
{
    class CmdVariable
    {
    public:
        CmdVariable(const char* text,i32 default_value, const std::string& desciption = {});
        CmdVariable(const char* text,f32 default_value, const std::string& desciption = {});
        CmdVariable(const char* text,f64 default_value, const std::string& desciption = {});
        CmdVariable(const char* text,bool default_value, const std::string& desciption = {});
        CmdVariable(const char* text,u32 default_value, const std::string& desciption = {});
        CmdVariable(const char* text,i64 default_value, const std::string& desciption = {});
        CmdVariable(const char* text,u8  default_value, const std::string& desciption = {});

        template<typename T>
        T get_value(){ return std::any_cast<T>(value); }
        template<typename T>
        void set_value(T t) {value = t;}
    friend struct CmadVariableMgr;
    protected:
        void add_to_mgr(const std::string& cmd_text);

        std::any value;
        std::string cmd_text;
        std::string desc;
    };

    struct CmadVariableMgr : ThreadSafeSingleton<CmadVariableMgr>
    {
        void add(const std::string& cmd_text,CmdVariable*);
        CmdVariable* find(const std::string& cmd_text);
        std::unordered_map<std::string,CmdVariable*>   cmd_variables; 
        friend class CmdVariable;
    };
}