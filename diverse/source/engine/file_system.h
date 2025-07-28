#pragma once
#include "core/vector.h"
#include "utility/singleton.h"
#include <map>
#include "core/string.h"
#include "utility/file_utils.h"
#include "dialog_window.h"

namespace diverse
{
    enum class FileOpenFlags
    {
        READ,
        WRITE,
        READ_WRITE,
        WRITE_READ
    };

    class FileSystem : public ThreadSafeSingleton<FileSystem>
    {
        friend class ThreadSafeSingleton<FileSystem>;

    public:
        bool resolve_physical_path(const std::string& path, std::string& outPhysicalPath, bool folder = false);
        bool absolute_path_2_fileSystem(const std::string& path, std::string& outFileSystemPath, bool folder = false);
        std::string absolute_path_2_fileSystem(const std::string& path, bool folder = false);

        uint8_t* read_file_vfs(const std::string& path);
        std::string read_text_file_vfs(const std::string& path);

        bool write_file_vfs(const std::string& path, uint8_t* buffer, uint32_t size);
        bool write_text_file_vfs(const std::string& path, const std::string& text);

        void set_asset_root(String8 root) { m_AssetRootPath = root; };

    private:
        String8 m_AssetRootPath;

    public:
        // Static Helpers. Implemented in OS specific Files
        static bool file_exists(const std::string& path);
        static bool folder_exists(const std::string& path);
        static int64_t get_file_size(const std::string& path);

        static uint8_t* read_file(const std::string& path);
        static bool read_file(const std::string& path, void* buffer, int64_t size = -1);
        static std::string read_text_file(const std::string& path);

        static bool write_file(const std::string& path, uint8_t* buffer, uint32_t size);
        static bool write_text_file(const std::string& path, const std::string& text);

        static std::string get_working_directory();

        static bool is_relative_path(const char* path);
        static bool is_absolute_path(const char* path);
        static const char* get_file_open_mode_string(FileOpenFlags flag);
    };

}
