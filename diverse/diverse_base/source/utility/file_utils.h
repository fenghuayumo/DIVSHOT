#pragma once
#include <string>
#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif
//using fs = std::filesystem;

namespace diverse
{
    std::vector<std::string> read_text_file(const std::filesystem::path& file_path);
    bool        loadText(const std::filesystem::path& path, std::string& text);
    bool        readByteData(const std::string& fileName, std::vector<uint8_t>& data, size_t& dataSize);
    bool	    readByteData(const std::string& fileName, uint8_t* dst, size_t size);
    bool        writeData(const std::string& fileName, const uint8_t* data, size_t size);
    bool        directoryExists(const std::string& path);
    bool        write_text(const std::string& path, const std::string& text);
    std::string getInstallDirectory();
    std::string getExecutablePath();
    std::string parentDirectory(const std::string& str);

    int do_system(const std::string& cmd);
    void add_dll_directory(const std::string& path,bool load = true);
    void set_dll_directory(const std::string& path);
    void add_env(const std::string& name, const std::string& value);
    void add_env(const std::string& cmd_lin);
}
