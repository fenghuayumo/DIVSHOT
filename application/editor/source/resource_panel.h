#pragma once

#include "editor_panel.h"
#include <assets/asset_manager.h>
#include <core/string.h>
#include <core/reference.h>
#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif

namespace diverse
{
    enum class FileType
    {
        Unknown = 0,
        Scene,
        Prefab,
        Script,
        Audio,
        Shader,
        Texture,
        Cubemap,
        Model,
        Material,
        Project,
        Ini,
        Font,
        Video,
        Splat
    };

    struct DirectoryInformation
    {
        SharedPtr<DirectoryInformation> Parent;
        std::vector<SharedPtr<DirectoryInformation>> Children;

        bool IsFile;
        bool Opened = false;
        bool Leaf   = true;
        bool IsRenaming = false;

        String8 Path;
        SharedPtr<asset::Texture> Thumbnail = nullptr;
        FileType Type;
        uint64_t FileSize;
        String8 FileSizeString;
        int64_t FileTypeID;
        bool Hidden;
        ImVec4 FileTypeColour;

    public:
        DirectoryInformation(String8 path, bool isF)
        {
            Path   = path;
            IsFile = isF;
            Hidden = false;
        }

        ~DirectoryInformation()
        {
        }
    };

    class ResourcePanel : public EditorPanel
    {
    public:
        ResourcePanel(bool active = false);
        ~ResourcePanel()
        {
            m_TextureLibrary.destroy();
            ArenaRelease(m_Arena);
        }

        void on_imgui_render() override;

        bool render_file(int dirIndex, bool folder, int shownIndex, bool gridView);
        void draw_folder(SharedPtr<DirectoryInformation>& dirInfo, bool defaultOpen = false);

        void destroy_graphics_resources() override
        {
            for(auto& dir : m_Directories)
            {
                if(dir.second)
                {
                    dir.second->Parent.reset();
                    dir.second->Children.clear();
                }
            }
            m_FolderIcon.reset();
            m_FileIcon.reset();
            m_Directories.clear();
            m_CurrentSelected.reset();
            m_CurrentDir.reset();
            m_BaseProjectDir.reset();
            m_NextDirectory.reset();
            m_PreviousDirectory.reset();
            m_BreadCrumbData.clear();
            m_TextureLibrary.destroy();
        }

        int get_parsed_asset_id(String8 extension)
        {
            for(int i = 0; i < assetTypes.size(); i++)
            {
                if(Str8Match(extension, assetTypes[i]))
                {
                    return i;
                }
            }

            return -1;
        }

        static bool move_file(String8 filePath, String8 movePath);

        String8 process_directory(String8 directoryPath, const SharedPtr<DirectoryInformation>& parent, bool processChildren);

        void change_directory(SharedPtr<DirectoryInformation>& directory);
        void remove_directory(SharedPtr<DirectoryInformation>& directory, bool removeFromParent = true);
        void on_new_project() override;
        void refresh();
        void queue_refresh() { m_Refresh = true; }

    private:
        static inline std::vector<String8> assetTypes = {
            Str8Lit("fbx"), Str8Lit("obj"), Str8Lit("wav"), Str8Lit("cs"), Str8Lit("png"), Str8Lit("blend"), Str8Lit("lsc"), Str8Lit("ogg"), Str8Lit("lua")
        };

        float MinGridSize = 50;
        float MaxGridSize = 400;
        String8 m_MovePath;
        String8 m_LastNavPath;
        String8 m_Delimiter;

        size_t m_BasePathLen;
        bool m_IsDragging;
        bool m_IsInListView;
        bool m_UpdateBreadCrumbs;
        bool m_ShowHiddenFiles;
        int m_GridItemsPerRow;
        float grid_size = 360.0f;

        ImGuiTextFilter m_Filter;

        bool textureCreated = false;

        String8 m_BasePath;
        String8 m_AssetPath;

        bool m_Refresh = false;

        bool m_UpdateNavigationPath = true;

        SharedPtr<DirectoryInformation> m_CurrentDir;
        SharedPtr<DirectoryInformation> m_BaseProjectDir;
        SharedPtr<DirectoryInformation> m_NextDirectory;
        SharedPtr<DirectoryInformation> m_PreviousDirectory;

        struct cmp_str
        {
            bool operator()(String8 a, String8 b) const
            {
                return Str8Match(a, b);
            }
        };

        struct String8Hash
        {
            std::size_t operator()(const String8& s) const
            {
                // Simple hash function that combines the bytes of the string
                std::size_t hash = 0;
                for(uint64_t i = 0; i < s.size; ++i)
                {
                    hash = hash * 31 + s.str[i];
                }
                return hash;
            }
        };

        std::unordered_map<String8, SharedPtr<DirectoryInformation>, String8Hash, cmp_str> m_Directories;
        std::vector<SharedPtr<DirectoryInformation>> m_BreadCrumbData;
        SharedPtr<asset::Texture> m_FolderIcon;
        SharedPtr<asset::Texture> m_FileIcon;
        SharedPtr<asset::Texture> m_ArchiveIcon;
        SharedPtr<asset::Texture> m_VideoIcon;
        SharedPtr<asset::Texture> m_AudioIcon;
        SharedPtr<asset::Texture> m_ModelIcon;
        SharedPtr<asset::Texture> m_FontIcon;
        SharedPtr<asset::Texture> m_SplatIcon;
        SharedPtr<asset::Texture> m_UnknownIcon;

        SharedPtr<DirectoryInformation> m_CurrentSelected;

        ResourceManager<asset::Texture> m_TextureLibrary;

        String8 m_CopiedPath;
        bool m_CutFile = false;
        std::string m_RenamePath;
        bool        m_RenameFile = false;
        Arena* m_Arena;
    };
}
