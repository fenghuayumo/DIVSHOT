#include "editor.h"
#include "resource_panel.h"
#include <engine/os.h>
#include <engine/input.h>
#include <engine/window.h>
#include <engine/engine.h>
#include <core/profiler.h>
#include <utility/string_utils.h>
#include <maths/maths_utils.h>
#include <imgui/IconsMaterialDesignIcons.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imgui_manager.h>
#include <imgui/imgui_renderer.h>

#include "embed_resources.h"

#ifdef DS_PLATFORM_WINDOWS
#include <Windows.h>
#undef RemoveDirectory
#undef MoveFile
#include <Shellapi.h>
#endif

namespace diverse
{
    static const std::unordered_map<FileType, String8> s_FileTypesToString = {
        { FileType::Unknown, Str8Lit("Unknown") },
        { FileType::Scene, Str8Lit("Scene") },
        { FileType::Prefab, Str8Lit("Prefab") },
        { FileType::Script, Str8Lit("Script") },
        { FileType::Shader, Str8Lit("Shader") },
        { FileType::Texture, Str8Lit("Texture") },
        { FileType::Font, Str8Lit("Font") },
        { FileType::Cubemap, Str8Lit("Cubemap") },
        { FileType::Model, Str8Lit("Model") },
        { FileType::Audio, Str8Lit("Audio") },
        { FileType::Video, Str8Lit("Video") },
        { FileType::Splat, Str8Lit("Splat") },
    };

    static const std::unordered_map<std::string, FileType> s_FileTypes = {
        { "dsn", FileType::Scene },
        { "dprefab", FileType::Prefab },
        { "cs", FileType::Script },
        { "lua", FileType::Script },
        { "hlsl", FileType::Shader },
        { "png", FileType::Texture },
        { "jpg", FileType::Texture },
        { "jpeg", FileType::Texture },
        { "bmp", FileType::Texture },
        { "gif", FileType::Texture },
        { "tga", FileType::Texture },
        { "ttf", FileType::Font },
        { "hdr", FileType::Cubemap },
        { "obj", FileType::Model },
        { "fbx", FileType::Model },
        { "gltf", FileType::Model },
        { "glb", FileType::Model },
        { "mp3", FileType::Audio },
        { "m4a", FileType::Audio },
        { "wav", FileType::Audio },
        { "ogg", FileType::Audio },
        { "mp4", FileType::Video },
        { "avi", FileType::Video },
		{ "mov", FileType::Video },
		{ "flv", FileType::Video },
		{ "wmv", FileType::Video },
        { "splat", FileType::Splat },
        { "ply", FileType::Splat },
    };

    static const std::unordered_map<FileType, ImVec4> s_TypeColors = {
        { FileType::Scene, { 0.8f, 0.4f, 0.22f, 1.00f } },
        { FileType::Prefab, { 0.10f, 0.50f, 0.80f, 1.00f } },
        { FileType::Script, { 0.10f, 0.50f, 0.80f, 1.00f } },
        { FileType::Font, { 0.60f, 0.19f, 0.32f, 1.00f } },
        { FileType::Shader, { 0.10f, 0.50f, 0.80f, 1.00f } },
        { FileType::Texture, { 0.82f, 0.20f, 0.33f, 1.00f } },
        { FileType::Cubemap, { 0.82f, 0.18f, 0.30f, 1.00f } },
        { FileType::Model, { 0.18f, 0.82f, 0.76f, 1.00f } },
        { FileType::Audio, { 0.20f, 0.80f, 0.50f, 1.00f } },
        { FileType::Video, { 0.50f, 0.80f, 0.50f, 1.00f } },
        { FileType::Splat, { 0.32f, 0.82f, 0.76f, 1.00f } },
    };

    ResourcePanel::ResourcePanel(bool active)
        :EditorPanel(active)
    {
        DS_PROFILE_FUNCTION();
        name       = U8CStr2CStr(ICON_MDI_FOLDER_STAR " Resources###resources");
        simple_name = "Resources";

        arena = ArenaAlloc(Kilobytes(64));
#ifdef DS_PLATFORM_WINDOWS
        delimiter = Str8Lit("\\");
#else
        delimiter = Str8Lit("/");
#endif

        float dpi  = Application::get().get_window()->get_dpi_scale();
        grid_size = 180.0f;
        grid_size *= dpi;
        MinGridSize *= dpi;
        MaxGridSize *= dpi;
        base_path = PushStr8F(arena, "%sassets", Application::get().get_project_settings().ProjectRoot.c_str());

        std::string assetsBasePath;
        FileSystem::get().resolve_physical_path("//assets", assetsBasePath);
        asset_path = PushStr8Copy(arena, Str8C((char*)std::filesystem::path(assetsBasePath).string().c_str()));

        String8 baseDirectoryHandle = process_directory(base_path, nullptr, true);
        base_project_dir            = directories[baseDirectoryHandle];
        change_directory(base_project_dir);

        current_dir = base_project_dir;

        update_navigation_path = true;
        is_dragging           = false;
        is_in_list_view         = false;
        update_bread_crumbs    = true;
        show_hidden_files      = false;
        const std::string resouce_path = "../resource/";
        file_icon = get_embed_texture(resouce_path + "icons/Document.png");
        folder_icon = get_embed_texture(resouce_path + "icons/Folder.png");
        archive_icon = get_embed_texture(resouce_path + "icons/Archive.png");
        model_icon = get_embed_texture(resouce_path + "icons/Mesh.png");
        video_icon = get_embed_texture(resouce_path + "icons/Video.png");
        audio_icon = get_embed_texture(resouce_path + "icons/Audio.png");
        splat_icon = get_embed_texture(resouce_path + "icons/TexturedMesh.png");
        unknown_icon = get_embed_texture(resouce_path + "icons/Undefined.png");
        is_refresh = false;
    }

    void ResourcePanel::change_directory(SharedPtr<DirectoryInformation>& directory)
    {
        if(!directory)
            return;

        previous_directory    = current_dir;
        current_dir           = directory;
        update_navigation_path = true;

        if(!current_dir->Opened)
        {
            process_directory(current_dir->Path, current_dir->Parent, true);
        }
    }

    void ResourcePanel::remove_directory(SharedPtr<DirectoryInformation>& directory, bool removeFromParent)
    {
        if(directory->Parent && removeFromParent)
        {
            directory->Parent->Children.clear();
        }

        for(auto& subdir : directory->Children)
            remove_directory(subdir, false);

        directories.erase(directories.find(directory->Path));
    }

    bool IsHidden(const std::filesystem::path& filePath)
    {
        try
        {
            std::filesystem::file_status status = std::filesystem::status(filePath);
            std::string name                    = filePath.stem().string();
            return (status.permissions() & std::filesystem::perms::owner_read) == std::filesystem::perms::none || name == ".DS_Store";
        }
        catch(const std::filesystem::filesystem_error& ex)
        {
            std::cerr << "Error accessing file: " << ex.what() << std::endl;
        }

        return false; // Return false by default if any error occurs
    }

    String8 ResourcePanel::process_directory(String8 directoryPath, const SharedPtr<DirectoryInformation>& parent, bool processChildren)
    {
        const auto& directory = directories[directoryPath];
        if(directory && directory->Opened)
            return directory->Path;

        SharedPtr<DirectoryInformation> directoryInfo = directory ? directory : createSharedPtr<DirectoryInformation>(directoryPath, !std::filesystem::is_directory(std::string((const char*)directoryPath.str, directoryPath.size)));
        directoryInfo->Parent                         = parent;

        if(Str8Match(directoryPath, base_path))
            directoryInfo->Path = base_path;
        else
            directoryInfo->Path = directoryPath;

        directoryInfo->Path       = directoryPath;
        String8 extension         = stringutility::str8_path_skip_last_period(directoryInfo->Path);
        directoryInfo->FileTypeID = get_parsed_asset_id(extension);

        auto stdPath = std::filesystem::path(std::string((const char*)directoryPath.str, directoryPath.size));

        if(std::filesystem::is_directory(stdPath))
        {
            directoryInfo->IsFile = false;
            directoryInfo->Path   = directoryPath; // .filename().string();
            directoryInfo->Leaf   = true;
            for(auto& entry : std::filesystem::directory_iterator(stdPath))
            {
                if(!show_hidden_files && IsHidden(entry.path()))
                {
                    continue;
                }

                if(entry.is_directory())
                    directoryInfo->Leaf = false;

                if(processChildren)
                {
                    directoryInfo->Opened = true;

                    String8 subdirHandle = process_directory(PushStr8Copy(arena, Str8C((char*)entry.path().generic_string().c_str())), directoryInfo, false);
                    directoryInfo->Children.push_back(directories[subdirHandle]);
                }
            }
        }
        else
        {
            auto fileType          = FileType::Unknown;
            const auto& fileTypeIt = s_FileTypes.find(std::string((const char*)extension.str, extension.size));
            if(fileTypeIt != s_FileTypes.end())
                fileType = fileTypeIt->second;

            directoryInfo->IsFile         = true;
            directoryInfo->Type           = fileType;
            directoryInfo->Path           = directoryPath; // .filename().string();
            directoryInfo->FileSize       = std::filesystem::exists(stdPath) ? std::filesystem::file_size(stdPath) : 0;
            directoryInfo->FileSizeString = PushStr8Copy(arena, stringutility::byte_2_string(directoryInfo->FileSize).c_str());
            directoryInfo->Hidden         = std::filesystem::exists(stdPath) ? IsHidden(stdPath) : true;
            directoryInfo->Opened         = true;
            directoryInfo->Leaf           = true;

            ImVec4 fileTypeColor        = { 1.0f, 1.0f, 1.0f, 1.0f };
            const auto& fileTypeColorIt = s_TypeColors.find(fileType);
            if(fileTypeColorIt != s_TypeColors.end())
                fileTypeColor = fileTypeColorIt->second;

            directoryInfo->FileTypeColour = fileTypeColor;
        }

        if(!directory)
            directories[directoryInfo->Path] = directoryInfo;
        return directoryInfo->Path;
    }

    void ResourcePanel::draw_folder(SharedPtr<DirectoryInformation>& dirInfo, bool defaultOpen)
    {
        DS_PROFILE_FUNCTION();
        ImGuiTreeNodeFlags nodeFlags = ((dirInfo == current_dir) ? ImGuiTreeNodeFlags_Selected : 0);
        nodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        if(dirInfo->Parent == nullptr)
            nodeFlags |= ImGuiTreeNodeFlags_Framed;

        const ImColor TreeLineColor = ImColor(128, 128, 128, 128);
        const float SmallOffsetX    = 6.0f * Application::get().get_window_dpi();
        ImDrawList* drawList        = ImGui::GetWindowDrawList();

        if(ImGui::IsWindowFocused() && (Input::get().get_key_held(InputCode::Key::LeftSuper) || Input::get().get_key_held(InputCode::Key::LeftControl)))
        {
            float mouseScroll = Input::get().get_scroll_offset();

            if(is_in_list_view)
            {
                if(mouseScroll > 0)
                    is_in_list_view = false;
            }
            else
            {
                grid_size += mouseScroll;
                if(grid_size < MinGridSize)
                    is_in_list_view = true;
            }

            grid_size = maths::Clamp(grid_size, MinGridSize, MaxGridSize);
        }

        if(!dirInfo->IsFile)
        {
            if(dirInfo->Leaf)
                nodeFlags |= ImGuiTreeNodeFlags_Leaf;

            if(defaultOpen)
                nodeFlags |= ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf;

            nodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

            bool isOpen = ImGui::TreeNodeEx((void*)(intptr_t)(dirInfo.get()), nodeFlags, "");
            if(ImGui::IsItemClicked())
            {
                change_directory(dirInfo);
            }

            const char* folderIcon = ((isOpen && !dirInfo->Leaf) || current_dir == dirInfo) ? U8CStr2CStr(ICON_MDI_FOLDER_OPEN) : U8CStr2CStr(ICON_MDI_FOLDER);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetIconColour());
            ImGui::TextUnformatted(folderIcon);
            ImGui::PopStyleColor();
            ImGui::SameLine();

            auto RootPath = Str8StdS(Application::get().get_project_settings().ProjectRoot);
            String8 RelativePath = { dirInfo->Path.str + RootPath.size, dirInfo->Path.size - RootPath.size };
            ImGui::TextUnformatted((const char*)(stringutility::str8_path_skip_last_slash(RelativePath).str));

            ImVec2 verticalLineStart = ImGui::GetCursorScreenPos();

            if(isOpen && !dirInfo->Leaf)
            {
                verticalLineStart.x += SmallOffsetX; // to nicely line up with the arrow symbol
                ImVec2 verticalLineEnd = verticalLineStart;

                for(int i = 0; i < dirInfo->Children.size(); i++)
                {
                    if(!dirInfo->Children[i]->IsFile)
                    {
                        auto currentPos = ImGui::GetCursorScreenPos();

                        ImGui::Indent(10.0f);

                        float HorizontalTreeLineSize = 16.0f * Application::get().get_window_dpi(); // chosen arbitrarily

                        if(!dirInfo->Children[i]->Leaf)
                            HorizontalTreeLineSize *= 0.5f;
                        draw_folder(dirInfo->Children[i]);

                        const ImRect childRect = ImRect(currentPos, currentPos + ImVec2(0.0f, ImGui::GetFontSize()));

                        const float midpoint = (childRect.Min.y + childRect.Max.y) * 0.5f;
                        drawList->AddLine(ImVec2(verticalLineStart.x, midpoint), ImVec2(verticalLineStart.x + HorizontalTreeLineSize, midpoint), TreeLineColor);
                        verticalLineEnd.y = midpoint;

                        ImGui::Unindent(10.0f);
                    }
                }

                drawList->AddLine(verticalLineStart, verticalLineEnd, TreeLineColor);

                ImGui::TreePop();
            }

            if(isOpen && dirInfo->Leaf)
                ImGui::TreePop();
        }

        if(is_dragging && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            move_path = dirInfo->Path;
        }
    }

    static int FileIndex = 0;

    void ResourcePanel::on_imgui_render()
    {
        DS_PROFILE_FUNCTION();

        texture_library.update((float)Engine::get().get_time_step().get_elapsed_seconds());
        if(ImGui::Begin(name.c_str(), &is_active))
        {
            FileIndex        = 0;
            auto windowSize  = ImGui::GetWindowSize();
            bool vertical    = windowSize.y > windowSize.x;
            static bool Init = false;

            if(is_refresh)
            {
                refresh();
                is_refresh = false;
            }

            if(!vertical)
            {
                ImGui::BeginColumns("ResourcePanelColumns", 2, 0);
                if(!Init)
                {
                    ImGui::SetColumnWidth(0, ImGui::GetWindowContentRegionMax().x / 3.0f);
                    Init = true;
                }
                ImGui::BeginChild("##folders_common");
            }
            else
                ImGui::BeginChild("##folders_common", ImVec2(0, ImGui::GetWindowHeight() / 3.0f));

            {
                {
                    ImGui::BeginChild("##folders");
                    {
                        draw_folder(base_project_dir, true);
                    }
                    ImGui::EndChild();
                }
            }

            ImGui::EndChild();

            if(ImGui::BeginDragDropTarget())
            {
                auto data = ImGui::AcceptDragDropPayload("selectable", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                if(data)
                {
                    String8* file = (String8*)data->Data;
                    if(move_file(*file, move_path))
                    {
                        DS_LOG_INFO("Moved File: {0} to {1}", (const char*)((*file).str), (const char*)move_path.str);
                    }
                    is_dragging = false;
                }
                ImGui::EndDragDropTarget();
            }
            float offset = 0.0f;
            if(!vertical)
            {
                ImGui::NextColumn();
            }
            else
            {
                offset = ImGui::GetWindowHeight() / 3.0f + 6.0f;
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
            }

            {
                {
                    ImGui::BeginChild("##directory_breadcrumbs", ImVec2(ImGui::GetColumnWidth(), ImGui::GetFrameHeightWithSpacing() * 2.0f));

                    ImGui::AlignTextToFramePadding();
                    {
                        ImGuiHelper::ScopedColour buttonColour(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                        if (ImGui::Button(U8CStr2CStr(ICON_MDI_REFRESH)))
                            refresh();
                    }
                    ImGui::SameLine();

                    ImGui::TextUnformatted(U8CStr2CStr(ICON_MDI_MAGNIFY));
                    ImGui::SameLine();
       
                    filter.Draw("##Filter", ImGui::GetContentRegionAvail().x - ImGui::GetStyle().IndentSpacing - 30);

                    ImGui::SameLine();
                    // Button for advanced settings
                    {
                        ImGuiHelper::ScopedColour buttonColour(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                        if (ImGui::Button(U8CStr2CStr(ICON_MDI_COGS)))
                            ImGui::OpenPopup("SettingsPopup");
                    }
                    if (ImGui::BeginPopup("SettingsPopup"))
                    {
                        if (is_in_list_view)
                        {
                            if (ImGui::Button(U8CStr2CStr(ICON_MDI_VIEW_LIST " Switch to Grid View")))
                            {
                                is_in_list_view = !is_in_list_view;
                            }
                        }
                        else
                        {
                            if (ImGui::Button(U8CStr2CStr(ICON_MDI_VIEW_GRID " Switch to List View")))
                            {
                                is_in_list_view = !is_in_list_view;
                            }
                        }

                        if (ImGui::Selectable("Refresh"))
                        {
                            queue_refresh();
                        }

                        if (ImGui::Selectable("New folder"))
                        {
                            std::filesystem::create_directory(std::filesystem::path(std::string((char*)current_dir->Path.str, current_dir->Path.size)) / "NewFolder");
                            queue_refresh();
                        }

                        if (!is_in_list_view)
                        {
                            ImGui::SliderFloat("##GridSize", &grid_size, 40.0f, 400.0f);
                        }

                        ImGui::EndPopup();
                    }
              
                    if(ImGui::Button(U8CStr2CStr(ICON_MDI_ARROW_LEFT)))
                    {
                        if(current_dir != base_project_dir)
                        {
                            change_directory(current_dir->Parent);
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::Button(U8CStr2CStr(ICON_MDI_ARROW_RIGHT)))
                    {
                        previous_directory = current_dir;
                        // m_CurrentDir = m_LastNavPath;
                        update_navigation_path = true;
                    }
                    ImGui::SameLine();

                    if(update_navigation_path)
                    {
                        bread_crumb_data.clear();
                        auto current = current_dir;
                        while(current)
                        {
                            if(current->Parent != nullptr)
                            {
                                bread_crumb_data.push_back(current);
                                current = current->Parent;
                            }
                            else
                            {
                                bread_crumb_data.push_back(base_project_dir);
                                current = nullptr;
                            }
                        }

                        std::reverse(bread_crumb_data.begin(), bread_crumb_data.end());
                        update_navigation_path = false;
                    }
                    {
                        int secIdx = 0, newPwdLastSecIdx = -1;

                        // String8 AssetsDir = m_CurrentDir->Path;

                        // String8List dirList = PathPartsFromStr8(scratch, m_AssetPath);
                        int dirIndex = 0;
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.7f, 0.0f));

                        for(auto& directory : bread_crumb_data)
                        {
                            auto RootPath = Str8StdS(Application::get().get_project_settings().ProjectRoot);
                            String8 RelativePath = { directory->Path.str + RootPath.size, directory->Path.size - RootPath.size};
                            if(ImGui::SmallButton((const char*)stringutility::get_file_name(RelativePath).str))
                                change_directory(directory);

                            ImGui::SameLine();
                        }

                        ImGui::PopStyleColor();

                        if(newPwdLastSecIdx >= 0)
                        {
                            int i        = 0;
                            auto stdPath = std::filesystem::path(std::string((const char*)current_dir->Path.str, current_dir->Path.size));

                            for(const auto& sec : stdPath)
                            {
                                if(i++ > newPwdLastSecIdx)
                                    break;
                                stdPath /= sec;
                            }
#ifdef _WIN32
                            if(newPwdLastSecIdx == 0)
                                stdPath /= "\\";
#endif

                            previous_directory    = current_dir;
                            current_dir           = directories[Str8C((char*)stdPath.string().c_str())];
                            update_navigation_path = true;
                        }

                        ImGui::SameLine();
                    }
                    ImGui::EndChild();
                }

                {
                    int shownIndex = 0;

                    float xAvail = ImGui::GetContentRegionAvail().x;

                    constexpr float padding          = 4.0f;
                    const float scaledThumbnailSize  = grid_size * ImGui::GetIO().FontGlobalScale;
                    const float scaledThumbnailSizeX = scaledThumbnailSize * 0.55f;
                    const float cellSize             = scaledThumbnailSizeX + 2 * padding + scaledThumbnailSizeX * 0.1f;

                    constexpr float overlayPaddingY  = 6.0f * padding;
                    constexpr float thumbnailPadding = overlayPaddingY * 0.5f;
                    const float thumbnailSize        = scaledThumbnailSizeX - thumbnailPadding;

                    const ImVec2 backgroundThumbnailSize = { scaledThumbnailSizeX + padding * 2, scaledThumbnailSize + padding * 2 };

                    const float panelWidth = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize;
                    int columnCount        = static_cast<int>(panelWidth / cellSize);
                    if(columnCount < 1)
                        columnCount = 1;

                    float lineHeight = ImGui::GetTextLineHeight();
                    int flags        = ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_ScrollY;

                    if(is_in_list_view)
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 0, 0 });
                        columnCount = 1;
                        flags |= ImGuiTableFlags_RowBg
                            | ImGuiTableFlags_NoPadOuterX
                            | ImGuiTableFlags_NoPadInnerX
                            | ImGuiTableFlags_SizingStretchSame;
                    }
                    else
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { scaledThumbnailSizeX * 0.05f, scaledThumbnailSizeX * 0.05f });
                        flags |= ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingFixedFit;
                    }

                    ImVec2 cursorPos    = ImGui::GetCursorPos();
                    const ImVec2 region = ImGui::GetContentRegionAvail();
                    ImGui::InvisibleButton("##DragDropTargetAssetPanelBody", region);

                    ImGui::SetNextItemAllowOverlap();
                    ImGui::SetCursorPos(cursorPos);

                    if(ImGui::BeginTable("BodyTable", columnCount, flags))
                    {
                        grid_items_per_row = (int)floor(xAvail / (grid_size + ImGui::GetStyle().ItemSpacing.x));
                        grid_items_per_row = maths::Max(1, grid_items_per_row);

                        texture_created = false;

                        ImGuiHelper::PushID();

                        if(is_in_list_view)
                        {
                            for(int i = 0; i < current_dir->Children.size(); i++)
                            {
                                if(current_dir->Children.size() > 0)
                                {
                                    if(!show_hidden_files && current_dir->Children[i]->Hidden)
                                    {
                                        continue;
                                    }

                                    if(filter.IsActive())
                                    {
                                        if(!filter.PassFilter((const char*)(stringutility::str8_path_skip_last_slash(current_dir->Children[i]->Path).str)))
                                        {
                                            continue;
                                        }
                                    }

                                    ImGui::TableNextColumn();
                                    bool doubleClicked = render_file(i, !current_dir->Children[i]->IsFile, shownIndex, !is_in_list_view);

                                    if(doubleClicked)
                                        break;
                                    shownIndex++;
                                }
                            }
                        }
                        else
                        {
                            for(int i = 0; i < current_dir->Children.size(); i++)
                            {
                                if(!show_hidden_files && current_dir->Children[i]->Hidden)
                                {
                                    continue;
                                }

                                if(filter.IsActive())
                                {
                                    if(!filter.PassFilter((const char*)(stringutility::str8_path_skip_last_slash(current_dir->Children[i]->Path).str)))
                                    {
                                        continue;
                                    }
                                }

                                ImGui::TableNextColumn();
                                bool doubleClicked = render_file(i, !current_dir->Children[i]->IsFile, shownIndex, !is_in_list_view);

                                if(doubleClicked)
                                    break;
                                shownIndex++;
                            }
                        }

                        ImGuiHelper::PopID();

                        if(ImGui::BeginPopupContextWindow("AssetPanelHierarchyContextWindow", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
                        {
                            if(std::filesystem::exists(ToStdString(copied_path)) && ImGui::Selectable("Paste"))
                            {
                                if(cut_file)
                                {
                                    const std::filesystem::path& fullPath = ToStdString(copied_path);
                                    std::string filename                  = fullPath.stem().string();
                                    std::string extension                 = fullPath.extension().string();
                                    std::filesystem::path destinationPath = std::filesystem::path(ToStdString(current_dir->Path)) / (filename + extension);

                                    {
                                        while(std::filesystem::exists(destinationPath))
                                        {
                                            filename += "_copy";
                                            destinationPath = destinationPath.parent_path() / (filename + extension);
                                        }
                                    }
                                    std::filesystem::rename(fullPath, destinationPath);
                                }
                                else
                                {
                                    const std::filesystem::path& fullPath = ToStdString(copied_path);
                                    std::string filename                  = fullPath.stem().string();
                                    std::string extension                 = fullPath.extension().string();
                                    std::filesystem::path destinationPath = std::filesystem::path(ToStdString(current_dir->Path)) / (filename + extension);
                                    {
                                        while(std::filesystem::exists(destinationPath))
                                        {
                                            filename += "_copy";
                                            destinationPath = destinationPath.parent_path() / (filename + extension);
                                        }
                                    }
                                    std::filesystem::copy(fullPath, destinationPath);
                                }
                                copied_path.str  = nullptr;
                                copied_path.size = 0;

                                cut_file = false;
                                refresh();
                            }

                            if(ImGui::Selectable("Open Location"))
                            {
                                diverse::OS::instance()->openFileLocation(ToStdString(current_dir->Path));
                            }

                            ImGui::Separator();

                            if(ImGui::Selectable("Import New Asset"))
                            {
                                //m_Editor->openFile();
                            }

                            if(ImGui::Selectable("Refresh"))
                            {
                                refresh();
                            }

                            if(ImGui::Selectable("New folder"))
                            {
                                std::filesystem::create_directory(std::filesystem::path(ToStdString(current_dir->Path)) / "NewFolder");
                                refresh();
                            }

                            if(!is_in_list_view)
                            {
                                ImGui::SliderFloat("##GridSize", &grid_size, MinGridSize, MaxGridSize);
                            }
                            ImGui::EndPopup();
                        }
                        if (rename_file)
                        {
                            const std::filesystem::path& fullPath = std::filesystem::path(ToStdString(base_path)) / rename_path;
                            const std::filesystem::path& srcPath = std::filesystem::path(ToStdString(current_selected->Path));
                            std::string filename = fullPath.stem().string();
                            std::string extension = fullPath.extension().string();
                            std::filesystem::path destinationPath = std::filesystem::path(ToStdString(current_dir->Path)) / (filename + extension);
                            std::filesystem::rename(srcPath, destinationPath);
                            rename_path = "";
                            rename_file = false;
                            refresh();
                        }

                        ImGui::EndTable();
                    }
                    ImGui::PopStyleVar();
                }
            }

            if(ImGui::BeginDragDropTarget())
            {
                auto data = ImGui::AcceptDragDropPayload("selectable", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                if(data)
                {
                    String8* a = (String8*)data->Data;
                    if(move_file(*a, move_path))
                    {
                        DS_LOG_INFO("Moved File: {0} to {1}", (const char*)((*a).str), (const char*)move_path.str);
                    }
                    is_dragging = false;
                }
                ImGui::EndDragDropTarget();
            }
        }
        ImGui::End();
    }

    bool ResourcePanel::render_file(int dirIndex, bool folder, int shownIndex, bool gridView)
    {
        DS_PROFILE_FUNCTION();
        constexpr float padding          = 4.0f;
        const float scaledThumbnailSize  = grid_size * ImGui::GetIO().FontGlobalScale;
        const float scaledThumbnailSizeX = scaledThumbnailSize * 0.55f;
        const float cellSize             = scaledThumbnailSizeX + 2 * padding + scaledThumbnailSizeX * 0.1f;

        constexpr float overlayPaddingY  = 6.0f * padding;
        constexpr float thumbnailPadding = overlayPaddingY * 0.5f;
        const float thumbnailSize        = scaledThumbnailSizeX - thumbnailPadding;

        const ImVec2 backgroundThumbnailSize = { scaledThumbnailSizeX + padding * 2, scaledThumbnailSize + padding * 2 };

        const float panelWidth = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize;
        int columnCount        = static_cast<int>(panelWidth / cellSize);
        if(columnCount < 1)
            columnCount = 1;

        bool doubleClicked = false;
        if(gridView)
        {
            auto& CurrentEnty              = current_dir->Children[dirIndex];
            auto textureId = folder_icon;

            auto cursorPos = ImGui::GetCursorPos();

            if(CurrentEnty->IsFile)
            {
                if(CurrentEnty->Type == FileType::Texture)
                {
                    if(CurrentEnty->Thumbnail)
                    {
                        textureId = CurrentEnty->Thumbnail;
                    }
                    else if(!texture_created)
                    {
                        texture_created = true;
                        CurrentEnty->Thumbnail = texture_library.get_resource(ToStdString(CurrentEnty->Path));
                        textureId              = CurrentEnty->Thumbnail ? CurrentEnty->Thumbnail : file_icon;
                    }
                    else
                    {
                        textureId = file_icon;
                    }
                }
                else if(CurrentEnty->Type == FileType::Scene)
                {
                    if(CurrentEnty->Thumbnail)
                    {
                        textureId = CurrentEnty->Thumbnail;
                    }
                    else if(!texture_created)
                    {
                        ArenaTemp scratch = ArenaTempBegin(arena);
                        String8 sceneScreenShotPath = PushStr8F(scratch.arena, "%s/scenes/cache/%s.png", base_path.str, CurrentEnty->Path);
                        const auto stdPath = ToStdString(sceneScreenShotPath);
                        if(std::filesystem::exists(std::filesystem::path(stdPath)))
                        {
                            texture_created = true;
                            CurrentEnty->Thumbnail = texture_library.get_resource(stdPath);
                            textureId              = CurrentEnty->Thumbnail ? CurrentEnty->Thumbnail : file_icon;
                        }
                        else
                            textureId = archive_icon;
                        ArenaTempEnd(scratch);
                    }
                    else
                    {
                        textureId = archive_icon;
                    }
                }
                else if (CurrentEnty->Type == FileType::Font)
                {
                    if (CurrentEnty->Thumbnail)
                    {
                        textureId = CurrentEnty->Thumbnail;
                    }
                }
                else if (CurrentEnty->Type == FileType::Audio)
                {
                    if (CurrentEnty->Thumbnail)
                    {
                        textureId = CurrentEnty->Thumbnail;
                    }
					else if (!texture_created)
					{
                        texture_created = true;
						CurrentEnty->Thumbnail = audio_icon;
						textureId              = CurrentEnty->Thumbnail ? CurrentEnty->Thumbnail : file_icon;
					}
					else
					{
						textureId = audio_icon;
					}
                }
                else if (CurrentEnty->Type == FileType::Video)
                {
                    if (CurrentEnty->Thumbnail)
                    {
                        textureId = CurrentEnty->Thumbnail;
                    }
					else if(!texture_created)
					{
                        texture_created = true;
						CurrentEnty->Thumbnail = video_icon;
						textureId              = CurrentEnty->Thumbnail ? CurrentEnty->Thumbnail : file_icon;
					}
					else
					{
						textureId = video_icon;
					}
                }
                else
                {
                    textureId = file_icon;
                }
            }
            bool flipImage = false;

            bool highlight = false;
            {
                highlight = current_dir->Children[dirIndex] == current_selected;
            }

            // Background button
            bool const clicked = ImGuiHelper::ToggleButton(ImGuiHelper::GenerateID(), highlight, backgroundThumbnailSize, 0.0f, 1.0f, ImGuiButtonFlags_AllowOverlap);
            if(clicked)
            {
                current_selected = current_dir->Children[dirIndex];
                //String8 RelativePath = { m_CurrentSelected->Path.str + m_BasePath.size + 1, m_CurrentSelected->Path.size - m_BasePath.size };
                //IsRenameFile = Str8StdS(m_RenamePath) == RelativePath;
            }
            //whether F2 key is pressed
            if (highlight && Input::get().get_key_pressed(diverse::InputCode::Key::F2))
            {
                String8 RelativePath = { current_selected->Path.str + base_path.size + 1, current_selected->Path.size - base_path.size };
                rename_path = ToStdString(RelativePath);
                current_selected->IsRenaming = true;
            }
            if (highlight && Input::get().get_key_pressed(diverse::InputCode::Key::Delete))
            {
                auto fullPath = current_dir->Children[dirIndex]->Path;
                std::filesystem::remove_all(std::string((const char*)fullPath.str, fullPath.size));
                queue_refresh();
            }

            if(ImGui::BeginPopupContextItem())
            {
                current_selected = current_dir->Children[dirIndex];

                if(ImGui::Selectable("Cut"))
                {
                    copied_path = current_dir->Children[dirIndex]->Path;
                    cut_file    = true;
                }

                if(ImGui::Selectable("Copy"))
                {
                    copied_path = current_dir->Children[dirIndex]->Path;
                    cut_file    = false;
                }

                if(ImGui::Selectable("Delete"))
                {
                    auto fullPath = current_dir->Children[dirIndex]->Path;
                    std::filesystem::remove_all(std::string((const char*)fullPath.str, fullPath.size));
                    queue_refresh();
                }

                if(ImGui::Selectable("Duplicate"))
                {
                    std::filesystem::path fullPath        = std::string((const char*)current_dir->Children[dirIndex]->Path.str, current_dir->Children[dirIndex]->Path.size);
                    std::filesystem::path destinationPath = fullPath;

                    {
                        std::string filename  = fullPath.stem().string();
                        std::string extension = fullPath.extension().string();

                        while(std::filesystem::exists(destinationPath))
                        {
                            filename += "_copy";
                            destinationPath = destinationPath.parent_path() / (filename + extension);
                        }
                    }
                    std::filesystem::copy(fullPath, destinationPath);
                    queue_refresh();
                }

                if (ImGui::Selectable("Rename"))
                {
                    String8 RelativePath = { current_selected->Path.str + base_path.size + 1, current_selected->Path.size - base_path.size };
                    rename_path = ToStdString(RelativePath);
                    current_selected->IsRenaming = true;
				}

                if(ImGui::Selectable("Open Location"))
                {
                    auto fullPath = current_dir->Children[dirIndex]->Path;
                    diverse::OS::instance()->openFileLocation(std::string((const char*)fullPath.str, fullPath.size));
                }

                if(current_dir->Children[dirIndex]->IsFile && ImGui::Selectable("Open External"))
                {
                    auto fullPath = current_dir->Children[dirIndex]->Path;
                    diverse::OS::instance()->openFileExternal(std::string((const char*)fullPath.str, fullPath.size));
                }

                ImGui::Separator();

                if(ImGui::Selectable("Import New Asset"))
                {
                    editor->open_file();
                }

                if(ImGui::Selectable("Refresh"))
                {
                    queue_refresh();
                }

                if(ImGui::Selectable("New folder"))
                {
                    std::filesystem::create_directory(std::filesystem::path(std::string((const char*)current_dir->Path.str, current_dir->Path.size) + "/NewFolder"));
                    queue_refresh();
                }

                if(!is_in_list_view)
                {
                    ImGui::SliderFloat("##GridSize", &grid_size, MinGridSize, MaxGridSize);
                }
                ImGui::EndPopup();
            }

            if(ImGui::IsItemHovered() && current_dir->Children[dirIndex]->Thumbnail)
            {
                ImGuiHelper::Tooltip(current_dir->Children[dirIndex]->Thumbnail->gpu_texture, { 512, 512 }, (const char*)(current_dir->Children[dirIndex]->Path.str));
            }
            else
                ImGuiHelper::Tooltip((const char*)(current_dir->Children[dirIndex]->Path.str));

            if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::TextUnformatted(editor->get_iconfont_icon(ToStdString(current_dir->Children[dirIndex]->Path)));

                ImGui::SameLine();
                move_path = current_dir->Children[dirIndex]->Path;
                ImGui::TextUnformatted((const char*)move_path.str);

                size_t size = sizeof(const char*) + move_path.size;
                ImGui::SetDragDropPayload("AssetFile", move_path.str, size);
                is_dragging = true;
                ImGui::EndDragDropSource();
            }

            if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                doubleClicked = true;
            }

            ImGui::SetCursorPos({ cursorPos.x + padding, cursorPos.y + padding });
            ImGui::SetNextItemAllowOverlap();
            ImGui::Image(reinterpret_cast<ImTextureID>(Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(texture_library.get_default_resource("white")->gpu_texture)), {backgroundThumbnailSize.x - padding * 2.0f, backgroundThumbnailSize.y - padding * 2.0f}, {0, 0}, {1, 1}, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg) + ImVec4(0.04f, 0.04f, 0.04f, 0.04f));

            ImGui::SetCursorPos({ cursorPos.x + thumbnailPadding * 0.75f, cursorPos.y + thumbnailPadding });
            ImGui::SetNextItemAllowOverlap();
            // ImGui::Image(reinterpret_cast<ImTextureID>(textureId), { thumbnailSize, thumbnailSize }, ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
            ImGuiHelper::Image(textureId->gpu_texture, { thumbnailSize, thumbnailSize });

            const ImVec2 typeColorFrameSize = { scaledThumbnailSizeX, scaledThumbnailSizeX * 0.03f };
            ImGui::SetCursorPosX(cursorPos.x + padding);
            ImGui::Image(reinterpret_cast<ImTextureID>(Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(texture_library.get_default_resource("white")->gpu_texture)), typeColorFrameSize, ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f), !CurrentEnty->IsFile ? ImVec4(0.0f, 0.0f, 0.0f, 0.0f) : CurrentEnty->FileTypeColour);

            if (current_selected && current_selected->IsRenaming && highlight)
            {
                ImGui::SetCursorPos(ImVec2(cursorPos.x + padding * 2, ImGui::GetCursorPosY() + padding * 4));
                if (!ImGuiHelper::InputText(rename_path, "##FileNameChange"))
                {
                    const ImVec2 rectMin = ImGui::GetItemRectMin();
                    const ImVec2 rectSize = ImGui::GetItemRectSize();
                    const ImRect clipRect = ImRect({ rectMin.x, rectMin.y }, { rectMin.x + rectSize.x,rectMin.y + rectSize.y });

                    String8 RelativePath = { current_selected->Path.str + base_path.size + 1, current_selected->Path.size - base_path.size };
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyReleased(ImGuiKey_Enter))
                    {
                        if (!(Str8StdS(rename_path) == RelativePath)) 
                        {
                            rename_file = true;
                            current_selected->IsRenaming = false;
                        }
                        else
                            current_selected->IsRenaming = clipRect.Contains(ImGui::GetMousePos());
					}
                }
            }
            else
            { 
                //render filename text
                const ImVec2 rectMin = ImGui::GetItemRectMin() + ImVec2(0.0f, 8.0f);
                const ImVec2 rectSize = ImGui::GetItemRectSize() + ImVec2(0.0f, 4.0f);
                const ImRect clipRect = ImRect({ rectMin.x + padding * 2.0f, rectMin.y + padding * 4.0f },
                    { rectMin.x + rectSize.x, rectMin.y + scaledThumbnailSizeX - ImGui::GetIO().Fonts->Fonts[2]->FontSize - padding * 4.0f });
                ImGuiHelper::ClippedText(clipRect.Min, clipRect.Max, (const char*)(stringutility::get_file_name(CurrentEnty->Path, !CurrentEnty->IsFile).str), nullptr, nullptr, { 0, 0 }, nullptr, clipRect.GetSize().x);
            }
            //render file type and size
            if(CurrentEnty->IsFile)
            {
                ImGui::SetCursorPos({ cursorPos.x + padding * (float)editor->get_window()->get_dpi_scale(), cursorPos.y + backgroundThumbnailSize.y - (ImGui::GetIO().Fonts->Fonts[2]->FontSize - padding * (float)editor->get_window()->get_dpi_scale()) * 3.2f });
                ImGui::BeginDisabled();
                ImGuiHelper::ScopedFont smallFont(ImGui::GetIO().Fonts->Fonts[2]);
                ImGui::Indent();

                String8 fileTypeString       = s_FileTypesToString.at(FileType::Unknown);
                const auto& fileStringTypeIt = s_FileTypesToString.find(CurrentEnty->Type);
                if(fileStringTypeIt != s_FileTypesToString.end())
                    fileTypeString = fileStringTypeIt->second;

                ImGui::TextUnformatted((const char*)(fileTypeString).str);
                ImGui::Unindent();
                cursorPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos({ cursorPos.x + padding * (float)editor->get_window()->get_dpi_scale(), cursorPos.y - (ImGui::GetIO().Fonts->Fonts[2]->FontSize * 0.6f - padding * (float)editor->get_window()->get_dpi_scale()) });
                ImGui::Indent();
                ImGui::TextUnformatted((const char*)CurrentEnty->FileSizeString.str);
                ImGui::Unindent();

                ImGui::EndDisabled();
            }
        }
        else
        {
            //if (m_RenameFile)
            //{
            //    ImGuiHelper::InputText(m_RenamePath, "##FileNameChange");
            //}
            ImGui::TextUnformatted(folder ? U8CStr2CStr(ICON_MDI_FOLDER) : editor->get_iconfont_icon(std::string((const char*)current_dir->Children[dirIndex]->Path.str, current_dir->Children[dirIndex]->Path.size)));
            ImGui::SameLine();

            if(ImGui::Selectable((const char*)stringutility::get_file_name(current_dir->Children[dirIndex]->Path, !current_dir->Children[dirIndex]->IsFile).str, false, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    doubleClicked = true;
                }
            }

            ImGuiHelper::Tooltip((const char*)current_dir->Children[dirIndex]->Path.str);

            if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::TextUnformatted(editor->get_iconfont_icon(ToStdString(current_dir->Children[dirIndex]->Path)));

                ImGui::SameLine();
                move_path = current_dir->Children[dirIndex]->Path;
                ImGui::TextUnformatted((const char*)move_path.str);

                size_t size = sizeof(const char*) + move_path.size;
                ImGui::SetDragDropPayload("AssetFile", move_path.str, size);
                is_dragging = true;
                ImGui::EndDragDropSource();
            }
        }

        if(doubleClicked)
        {
            if(folder)
            {
                change_directory(current_dir->Children[dirIndex]);
            }
            else
            {
                editor->file_open_callback(std::string((const char*)current_dir->Children[dirIndex]->Path.str, current_dir->Children[dirIndex]->Path.size));
            }
        }

        return doubleClicked;
    }

    bool ResourcePanel::move_file(const String8 filePath, String8 movePath)
    {
        DS_PROFILE_FUNCTION();
        std::string s = "move " + std::string((const char*)filePath.str, filePath.size) + " " + std::string((const char*)movePath.str, movePath.size);

#ifndef DS_PLATFORM_IOS
        system(s.c_str());
#endif

        return std::filesystem::exists(std::filesystem::path(std::string((const char*)movePath.str, movePath.size)) / stringutility::get_file_name(std::string((const char*)filePath.str, filePath.size)));
    }

    void ResourcePanel::on_new_project()
    {
        base_path = PushStr8F(arena, "%sassets", Application::get().get_project_settings().ProjectRoot.c_str());

        std::string assetsBasePath;
        FileSystem::get().resolve_physical_path("//assets", assetsBasePath);
        asset_path = PushStr8Copy(arena, Str8C((char*)std::filesystem::path(assetsBasePath).string().c_str()));

        String8 baseDirectoryHandle = process_directory(base_path, nullptr, true);
        base_project_dir = directories[baseDirectoryHandle];
        change_directory(base_project_dir);

        current_dir = base_project_dir;
        queue_refresh();
    }

    void ResourcePanel::refresh()
    {
        texture_library.destroy();

        Arena* temp         = ArenaAlloc(256);
        String8 currentPath = PushStr8Copy(temp, current_dir->Path);

        ArenaClear(arena);
        directories.clear();

        base_path                  = PushStr8F(arena, "%sassets", Application::get().get_project_settings().ProjectRoot.c_str());
        String8 baseDirectoryHandle = process_directory(base_path, nullptr, true);
        base_project_dir            = directories[baseDirectoryHandle];
        change_directory(base_project_dir);

        update_navigation_path = true;

        base_project_dir    = directories[baseDirectoryHandle];
        previous_directory = nullptr;
        current_dir        = nullptr;

        bool dirFound = false;
        for(auto dir : directories)
        {
            if(Str8Match(dir.first, currentPath))
            {
                current_dir = dir.second;
                dirFound     = true;
                break;
            }
        }
        if(dirFound)
            change_directory(current_dir);

        ArenaRelease(temp);
    }
}
