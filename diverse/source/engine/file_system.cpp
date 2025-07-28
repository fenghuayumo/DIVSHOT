#include "precompile.h"
#include "file_system.h"

namespace diverse
{
    bool FileSystem::is_relative_path(const char* path)
    {
        if(!path || path[0] == '/' || path[0] == '\\')
        {
            return false;
        }

        if(strlen(path) >= 2 && isalpha(path[0]) && path[1] == ':')
        {
            return false;
        }

        return true;
    }

    bool FileSystem::is_absolute_path(const char* path)
    {
        if(!path)
        {
            return false;
        }

        return !is_relative_path(path);
    }

    const char* FileSystem::get_file_open_mode_string(FileOpenFlags flag)
    {
        if(flag == FileOpenFlags::READ)
        {
            return "rb";
        }
        else if(flag == FileOpenFlags::WRITE)
        {
            return "wb";
        }
        else if(flag == FileOpenFlags::READ_WRITE)
        {
            return "rb+";
        }
        else if(flag == FileOpenFlags::WRITE_READ)
        {
            return "wb+";
        }
        else
        {
            DS_LOG_WARN("Invalid open flag");
            return "rb";
        }
    }

    bool FileSystem::resolve_physical_path(const std::string& path, std::string& outPhysicalPath, bool folder)
    {
        DS_PROFILE_FUNCTION();
        const std::string& updatedPath = path;

        if(!(path[0] == '/' && path[1] == '/'))
        {
            outPhysicalPath = path;
            return folder ? FileSystem ::folder_exists(outPhysicalPath) : FileSystem::file_exists(outPhysicalPath);
        }

        // Assume path starts with //Assets
#ifndef DS_PRODUCTION
        if(path.substr(2, 6) != "assets")
        {
            // Previously paths saved in scenes could be like //Textures and then converted to .../assets/textures/...
            outPhysicalPath = ToStdString(m_AssetRootPath) + path.substr(1, path.size());
            return folder ? FileSystem::folder_exists(outPhysicalPath) : FileSystem::file_exists(outPhysicalPath);
        }
        else
#endif
        {
            outPhysicalPath = ToStdString(m_AssetRootPath) + path.substr(8, path.size());
            return folder ? FileSystem::folder_exists(outPhysicalPath) : FileSystem::file_exists(outPhysicalPath);
        }

        return false;
    }

    uint8_t* FileSystem::read_file_vfs(const std::string& path)
    {
        DS_PROFILE_FUNCTION();
        std::string physicalPath;
        return get().resolve_physical_path(path, physicalPath) ? FileSystem::read_file(physicalPath) : nullptr;
    }

    std::string FileSystem::read_text_file_vfs(const std::string& path)
    {
        DS_PROFILE_FUNCTION();
        std::string physicalPath;
        return get().resolve_physical_path(path, physicalPath) ? FileSystem::read_text_file(physicalPath) : "";
    }

    bool FileSystem::write_file_vfs(const std::string& path, uint8_t* buffer, uint32_t size)
    {
        DS_PROFILE_FUNCTION();
        std::string physicalPath;
        return get().resolve_physical_path(path, physicalPath) ? FileSystem::write_file(physicalPath, buffer, size) : false;
    }

    bool FileSystem::write_text_file_vfs(const std::string& path, const std::string& text)
    {
        DS_PROFILE_FUNCTION();
        std::string physicalPath;
        return get().resolve_physical_path(path, physicalPath) ? FileSystem::write_text_file(physicalPath, text) : false;
    }

    bool FileSystem::absolute_path_2_fileSystem(const std::string& path, std::string& outFileSystemPath, bool folder)
    {
        DS_PROFILE_FUNCTION();
        std::string updatedPath = path;
        std::replace(updatedPath.begin(), updatedPath.end(), '\\', '/');

        if(updatedPath.find(ToStdString(m_AssetRootPath)) != std::string::npos)
        {
            std::string newPath     = updatedPath;
            std::string newPartPath = "//assets";
            newPath.replace(0, m_AssetRootPath.size, newPartPath);
            outFileSystemPath = newPath;
            return true;
        }

        outFileSystemPath = updatedPath;
        return false;
    }

    std::string FileSystem::absolute_path_2_fileSystem(const std::string& path, bool folder)
    {
        std::string outPath;
        absolute_path_2_fileSystem(path, outPath, folder);
        return outPath;
    }
}
