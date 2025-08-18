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
            texture_library.destroy();
            ArenaRelease(arena);
        }

        void on_imgui_render() override;

        bool render_file(int dirIndex, bool folder, int shownIndex, bool gridView);
        void draw_folder(SharedPtr<DirectoryInformation>& dirInfo, bool defaultOpen = false);

        void destroy_graphics_resources() override
        {
            for(auto& dir : directories)
            {
                if(dir.second)
                {
                    dir.second->Parent.reset();
                    dir.second->Children.clear();
                }
            }
            folder_icon.reset();
            file_icon.reset();
            directories.clear();
            current_selected.reset();
            current_dir.reset();
            base_project_dir.reset();
            next_directory.reset();
            previous_directory.reset();
            bread_crumb_data.clear();
            texture_library.destroy();
        }

        int get_parsed_asset_id(String8 extension)
        {
            for(int i = 0; i < asset_types.size(); i++)
            {
                if(Str8Match(extension, asset_types[i]))
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
        void queue_refresh() { is_refresh = true; }

    private:
        static inline std::vector<String8> asset_types = {
            Str8Lit("fbx"), Str8Lit("obj"), Str8Lit("wav"), Str8Lit("cs"), Str8Lit("png"), Str8Lit("blend"), Str8Lit("lsc"), Str8Lit("ogg"), Str8Lit("lua")
        };

        float MinGridSize = 50;
        float MaxGridSize = 400;
        String8 move_path;
        String8 last_nav_path;
        String8 delimiter;

        size_t base_path_len;
        bool is_dragging;
        bool is_in_list_view;
        bool update_bread_crumbs;
        bool show_hidden_files;
        int grid_items_per_row;
        float grid_size = 360.0f;

        ImGuiTextFilter filter;

        bool texture_created = false;

        String8 base_path;
        String8 asset_path;

        bool is_refresh = false;

        bool update_navigation_path = true;

        SharedPtr<DirectoryInformation> current_dir;
        SharedPtr<DirectoryInformation> base_project_dir;
        SharedPtr<DirectoryInformation> next_directory;
        SharedPtr<DirectoryInformation> previous_directory;

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

        std::unordered_map<String8, SharedPtr<DirectoryInformation>, String8Hash, cmp_str> directories;
        std::vector<SharedPtr<DirectoryInformation>> bread_crumb_data;
        SharedPtr<asset::Texture> folder_icon;
        SharedPtr<asset::Texture> file_icon;
        SharedPtr<asset::Texture> archive_icon;
        SharedPtr<asset::Texture> video_icon;
        SharedPtr<asset::Texture> audio_icon;
        SharedPtr<asset::Texture> model_icon;
        SharedPtr<asset::Texture> font_icon;
        SharedPtr<asset::Texture> splat_icon;
        SharedPtr<asset::Texture> unknown_icon;

        SharedPtr<DirectoryInformation> current_selected;

        ResourceManager<asset::Texture> texture_library;

        String8 copied_path;
        bool cut_file = false;
        std::string rename_path;
        bool        rename_file = false;
        Arena* arena;
    };
}
