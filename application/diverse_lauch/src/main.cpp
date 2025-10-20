#include <iostream>
#include <filesystem>
#include <utility/process_utils.hpp>
#include <format>
int main(int argc, char** argv)
{
    std::cout << "Diverse Launch Application" << std::endl;

    auto update_exe_path = std::filesystem::path("AutoUpdate") / "AutoUpdateInCSharp.exe";
    if(std::filesystem::exists(update_exe_path) )
    {
        std::cout << "Launching AutoUpdate..." << std::endl;
        diverse::lauch_process(update_exe_path.string());
    }
    std::string command = "divshot.exe ";
    if(argc >= 2){
        std::string arg = std::format("--open_project=",argv[1]);
        std::cout << "Argument provided: " << arg << std::endl;
        command = command + arg;
    }
    diverse::lauch_process(command);
    return 0;
}