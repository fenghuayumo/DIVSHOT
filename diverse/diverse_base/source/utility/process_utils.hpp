#pragma once
#include <string>

namespace diverse
{
    auto lauch_process(const std::string& commandLine,const char* env_path = nullptr)->int;
}