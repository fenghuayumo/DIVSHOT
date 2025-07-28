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
        m_Name       = U8CStr2CStr(ICON_MDI_FOLDER_STAR " Resources###resources");
        m_SimpleName = "Resources";

        m_Arena = ArenaAlloc(Kilobytes(64));
#ifdef DS_PLATFORM_WINDOWS
        m_Delimiter = Str8Lit("\\");
#else
        m_Delimiter = Str8Lit("/");
#endif

        float dpi  = Application::get().get_window()->get_dpi_scale();
        grid_size = 180.0f;
        grid_size *= dpi;
        MinGridSize *= dpi;
        MaxGridSize *= dpi;
        m_BasePath = PushStr8F(m_Arena, "%sassets", Application::get().get_project_settings().m_ProjectRoot.c_str());

        std::string assetsBasePath;
        FileSystem::get().resolve_physical_path("//assets", assetsBasePath);
        m_AssetPath = PushStr8Copy(m_Arena, Str8C((char*)std::filesystem::path(assetsBasePath).string().c_str()));

        String8 baseDirectoryHandle = process_directory(m_BasePath, nullptr, true);
        m_BaseProjectDir            = m_Directories[baseDirectoryHandle];
        change_directory(m_BaseProjectDir);

        m_CurrentDir = m_BaseProjectDir;

        m_UpdateNavigationPath = true;
        m_IsDragging           = false;
        m_IsInListView         = false;
        m_UpdateBreadCrumbs    = true;
        m_ShowHiddenFiles      = false;
        const std::string resouce_path = "../resource/";
        m_FileIcon = get_embed_texture(resouce_path + "icons/Document.png");
        m_FolderIcon = get_embed_texture(resouce_path + "icons/Folder.png");
        m_ArchiveIcon = get_embed_texture(resouce_path + "icons/Archive.png");
        m_ModelIcon = get_embed_texture(resouce_path + "icons/Mesh.png");
        m_VideoIcon = get_embed_texture(resouce_path + "icons/Video.png");
        m_AudioIcon = get_embed_texture(resouce_path + "icons/Audio.png");
        m_SplatIcon = get_embed_texture(resouce_path + "icons/TexturedMesh.png");
        m_UnknownIcon = get_embed_texture(resouce_path + "icons/Undefined.png");
        m_Refresh     = false;
    }

    void ResourcePanel::change_directory(SharedPtr<DirectoryInformation>& directory)
    {
        if(!directory)
            return;

        m_PreviousDirectory    = m_CurrentDir;
        m_CurrentDir           = directory;
        m_UpdateNavigationPath = true;

        if(!m_CurrentDir->Opened)
        {
            process_directory(m_CurrentDir->Path, m_CurrentDir->Parent, true);
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

        m_Directories.erase(m_Directories.find(directory->Path));
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
        const auto& directory = m_Directories[directoryPath];
        if(directory && directory->Opened)
            return directory->Path;

        SharedPtr<DirectoryInformation> directoryInfo = directory ? directory : createSharedPtr<DirectoryInformation>(directoryPath, !std::filesystem::is_directory(std::string((const char*)directoryPath.str, directoryPath.size)));
        directoryInfo->Parent                         = parent;

        if(Str8Match(directoryPath, m_BasePath))
            directoryInfo->Path = m_BasePath;
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
                if(!m_ShowHiddenFiles && IsHidden(entry.path()))
                {
                    continue;
                }

                if(entry.is_directory())
                    directoryInfo->Leaf = false;

                if(processChildren)
                {
                    directoryInfo->Opened = true;

                    String8 subdirHandle = process_directory(PushStr8Copy(m_Arena, Str8C((char*)entry.path().generic_string().c_str())), directoryInfo, false);
                    directoryInfo->Children.push_back(m_Directories[subdirHandle]);
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
            directoryInfo->FileSizeString = PushStr8Copy(m_Arena, stringutility::byte_2_string(directoryInfo->FileSize).c_str());
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
            m_Directories[directoryInfo->Path] = directoryInfo;
        return directoryInfo->Path;
    }

    void ResourcePanel::draw_folder(SharedPtr<DirectoryInformation>& dirInfo, bool defaultOpen)
    {
        DS_PROFILE_FUNCTION();
        ImGuiTreeNodeFlags nodeFlags = ((dirInfo == m_CurrentDir) ? ImGuiTreeNodeFlags_Selected : 0);
        nodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        if(dirInfo->Parent == nullptr)
            nodeFlags |= ImGuiTreeNodeFlags_Framed;

        const ImColor TreeLineColor = ImColor(128, 128, 128, 128);
        const float SmallOffsetX    = 6.0f * Application::get().get_window_dpi();
        ImDrawList* drawList        = ImGui::GetWindowDrawList();

        if(ImGui::IsWindowFocused() && (Input::get().get_key_held(InputCode::Key::LeftSuper) || Input::get().get_key_held(InputCode::Key::LeftControl)))
        {
            float mouseScroll = Input::get().get_scroll_offset();

            if(m_IsInListView)
            {
                if(mouseScroll > 0)
                    m_IsInListView = false;
            }
            else
            {
                grid_size += mouseScroll;
                if(grid_size < MinGridSize)
                    m_IsInListView = true;
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

            const char* folderIcon = ((isOpen && !dirInfo->Leaf) || m_CurrentDir == dirInfo) ? U8CStr2CStr(ICON_MDI_FOLDER_OPEN) : U8CStr2CStr(ICON_MDI_FOLDER);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetIconColour());
            ImGui::TextUnformatted(folderIcon);
            ImGui::PopStyleColor();
            ImGui::SameLine();

            auto RootPath = Str8StdS(Application::get().get_project_settings().m_ProjectRoot);
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

        if(m_IsDragging && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            m_MovePath = dirInfo->Path;
        }
    }

    static int FileIndex = 0;

    void ResourcePanel::on_imgui_render()
    {
        DS_PROFILE_FUNCTION();

        m_TextureLibrary.update((float)Engine::get().get_time_step().get_elapsed_seconds());
        if(ImGui::Begin(m_Name.c_str(), &m_Active))
        {
            FileIndex        = 0;
            auto windowSize  = ImGui::GetWindowSize();
            bool vertical    = windowSize.y > windowSize.x;
            static bool Init = false;

            if(m_Refresh)
            {
                refresh();
                m_Refresh = false;
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
                        draw_folder(m_BaseProjectDir, true);
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
                    if(move_file(*file, m_MovePath))
                    {
                        DS_LOG_INFO("Moved File: {0} to {1}", (const char*)((*file).str), (const char*)m_MovePath.str);
                    }
                    m_IsDragging = false;
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
       
                    m_Filter.Draw("##Filter", ImGui::GetContentRegionAvail().x - ImGui::GetStyle().IndentSpacing - 30);

                    ImGui::SameLine();
                    // Button for advanced settings
                    {
                        ImGuiHelper::ScopedColour buttonColour(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                        if (ImGui::Button(U8CStr2CStr(ICON_MDI_COGS)))
                            ImGui::OpenPopup("SettingsPopup");
                    }
                    if (ImGui::BeginPopup("SettingsPopup"))
                    {
                        if (m_IsInListView)
                        {
                            if (ImGui::Button(U8CStr2CStr(ICON_MDI_VIEW_LIST " Switch to Grid View")))
                            {
                                m_IsInListView = !m_IsInListView;
                            }
                        }
                        else
                        {
                            if (ImGui::Button(U8CStr2CStr(ICON_MDI_VIEW_GRID " Switch to List View")))
                            {
                                m_IsInListView = !m_IsInListView;
                            }
                        }

                        if (ImGui::Selectable("Refresh"))
                        {
                            queue_refresh();
                        }

                        if (ImGui::Selectable("New folder"))
                        {
                            std::filesystem::create_directory(std::filesystem::path(std::string((char*)m_CurrentDir->Path.str, m_CurrentDir->Path.size)) / "NewFolder");
                            queue_refresh();
                        }

                        if (!m_IsInListView)
                        {
                            ImGui::SliderFloat("##GridSize", &grid_size, 40.0f, 400.0f);
                        }

                        ImGui::EndPopup();
                    }
              
                    if(ImGui::Button(U8CStr2CStr(ICON_MDI_ARROW_LEFT)))
                    {
                        if(m_CurrentDir != m_BaseProjectDir)
                        {
                            change_directory(m_CurrentDir->Parent);
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::Button(U8CStr2CStr(ICON_MDI_ARROW_RIGHT)))
                    {
                        m_PreviousDirectory = m_CurrentDir;
                        // m_CurrentDir = m_LastNavPath;
                        m_UpdateNavigationPath = true;
                    }
                    ImGui::SameLine();

                    if(m_UpdateNavigationPath)
                    {
                        m_BreadCrumbData.clear();
                        auto current = m_CurrentDir;
                        while(current)
                        {
                            if(current->Parent != nullptr)
                            {
                                m_BreadCrumbData.push_back(current);
                                current = current->Parent;
                            }
                            else
                            {
                                m_BreadCrumbData.push_back(m_BaseProjectDir);
                                current = nullptr;
                            }
                        }

                        std::reverse(m_BreadCrumbData.begin(), m_BreadCrumbData.end());
                        m_UpdateNavigationPath = false;
                    }
                    {
                        int secIdx = 0, newPwdLastSecIdx = -1;

                        // String8 AssetsDir = m_CurrentDir->Path;

                        // String8List dirList = PathPartsFromStr8(scratch, m_AssetPath);
                        int dirIndex = 0;
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.7f, 0.0f));

                        for(auto& directory : m_BreadCrumbData)
                        {
                            auto RootPath = Str8StdS(Application::get().get_project_settings().m_ProjectRoot);
                            String8 RelativePath = { directory->Path.str + RootPath.size, directory->Path.size - RootPath.size};
                            if(ImGui::SmallButton((const char*)stringutility::get_file_name(RelativePath).str))
                                change_directory(directory);

                            ImGui::SameLine();
                        }

                        ImGui::PopStyleColor();

                        if(newPwdLastSecIdx >= 0)
                        {
                            int i        = 0;
                            auto stdPath = std::filesystem::path(std::string((const char*)m_CurrentDir->Path.str, m_CurrentDir->Path.size));

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

                            m_PreviousDirectory    = m_CurrentDir;
                            m_CurrentDir           = m_Directories[Str8C((char*)stdPath.string().c_str())];
                            m_UpdateNavigationPath = true;
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

                    if(m_IsInListView)
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
                        m_GridItemsPerRow = (int)floor(xAvail / (grid_size + ImGui::GetStyle().ItemSpacing.x));
                        m_GridItemsPerRow = maths::Max(1, m_GridItemsPerRow);

                        textureCreated = false;

                        ImGuiHelper::PushID();

                        if(m_IsInListView)
                        {
                            for(int i = 0; i < m_CurrentDir->Children.size(); i++)
                            {
                                if(m_CurrentDir->Children.size() > 0)
                                {
                                    if(!m_ShowHiddenFiles && m_CurrentDir->Children[i]->Hidden)
                                    {
                                        continue;
                                    }

                                    if(m_Filter.IsActive())
                                    {
                                        if(!m_Filter.PassFilter((const char*)(stringutility::str8_path_skip_last_slash(m_CurrentDir->Children[i]->Path).str)))
                                        {
                                            continue;
                                        }
                                    }

                                    ImGui::TableNextColumn();
                                    bool doubleClicked = render_file(i, !m_CurrentDir->Children[i]->IsFile, shownIndex, !m_IsInListView);

                                    if(doubleClicked)
                                        break;
                                    shownIndex++;
                                }
                            }
                        }
                        else
                        {
                            for(int i = 0; i < m_CurrentDir->Children.size(); i++)
                            {
                                if(!m_ShowHiddenFiles && m_CurrentDir->Children[i]->Hidden)
                                {
                                    continue;
                                }

                                if(m_Filter.IsActive())
                                {
                                    if(!m_Filter.PassFilter((const char*)(stringutility::str8_path_skip_last_slash(m_CurrentDir->Children[i]->Path).str)))
                                    {
                                        continue;
                                    }
                                }

                                ImGui::TableNextColumn();
                                bool doubleClicked = render_file(i, !m_CurrentDir->Children[i]->IsFile, shownIndex, !m_IsInListView);

                                if(doubleClicked)
                                    break;
                                shownIndex++;
                            }
                        }

                        ImGuiHelper::PopID();

                        if(ImGui::BeginPopupContextWindow("AssetPanelHierarchyContextWindow", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
                        {
                            if(std::filesystem::exists(ToStdString(m_CopiedPath)) && ImGui::Selectable("Paste"))
                            {
                                if(m_CutFile)
                                {
                                    const std::filesystem::path& fullPath = ToStdString(m_CopiedPath);
                                    std::string filename                  = fullPath.stem().string();
                                    std::string extension                 = fullPath.extension().string();
                                    std::filesystem::path destinationPath = std::filesystem::path(ToStdString(m_CurrentDir->Path)) / (filename + extension);

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
                                    const std::filesystem::path& fullPath = ToStdString(m_CopiedPath);
                                    std::string filename                  = fullPath.stem().string();
                                    std::string extension                 = fullPath.extension().string();
                                    std::filesystem::path destinationPath = std::filesystem::path(ToStdString(m_CurrentDir->Path)) / (filename + extension);
                                    {
                                        while(std::filesystem::exists(destinationPath))
                                        {
                                            filename += "_copy";
                                            destinationPath = destinationPath.parent_path() / (filename + extension);
                                        }
                                    }
                                    std::filesystem::copy(fullPath, destinationPath);
                                }
                                m_CopiedPath.str  = nullptr;
                                m_CopiedPath.size = 0;

                                m_CutFile = false;
                                refresh();
                            }

                            if(ImGui::Selectable("Open Location"))
                            {
                                diverse::OS::instance()->openFileLocation(ToStdString(m_CurrentDir->Path));
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
                                std::filesystem::create_directory(std::filesystem::path(ToStdString(m_CurrentDir->Path)) / "NewFolder");
                                refresh();
                            }

                            if(!m_IsInListView)
                            {
                                ImGui::SliderFloat("##GridSize", &grid_size, MinGridSize, MaxGridSize);
                            }
                            ImGui::EndPopup();
                        }
                        if (m_RenameFile)
                        {
                            const std::filesystem::path& fullPath = std::filesystem::path(ToStdString(m_BasePath)) / m_RenamePath;
                            const std::filesystem::path& srcPath = std::filesystem::path(ToStdString(m_CurrentSelected->Path));
                            std::string filename = fullPath.stem().string();
                            std::string extension = fullPath.extension().string();
                            std::filesystem::path destinationPath = std::filesystem::path(ToStdString(m_CurrentDir->Path)) / (filename + extension);
                            std::filesystem::rename(srcPath, destinationPath);
                            m_RenamePath = "";
                            m_RenameFile = false;
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
                    if(move_file(*a, m_MovePath))
                    {
                        DS_LOG_INFO("Moved File: {0} to {1}", (const char*)((*a).str), (const char*)m_MovePath.str);
                    }
                    m_IsDragging = false;
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
            auto& CurrentEnty              = m_CurrentDir->Children[dirIndex];
            auto textureId = m_FolderIcon;

            auto cursorPos = ImGui::GetCursorPos();

            if(CurrentEnty->IsFile)
            {
                if(CurrentEnty->Type == FileType::Texture)
                {
                    if(CurrentEnty->Thumbnail)
                    {
                        textureId = CurrentEnty->Thumbnail;
                    }
                    else if(!textureCreated)
                    {
                        textureCreated         = true;
                        CurrentEnty->Thumbnail = m_TextureLibrary.get_resource(ToStdString(CurrentEnty->Path));
                        textureId              = CurrentEnty->Thumbnail ? CurrentEnty->Thumbnail : m_FileIcon;
                    }
                    else
                    {
                        textureId = m_FileIcon;
                    }
                }
                else if(CurrentEnty->Type == FileType::Scene)
                {
                    if(CurrentEnty->Thumbnail)
                    {
                        textureId = CurrentEnty->Thumbnail;
                    }
                    else if(!textureCreated)
                    {
                        ArenaTemp scratch = ArenaTempBegin(m_Arena);
                        String8 sceneScreenShotPath = PushStr8F(scratch.arena, "%s/scenes/cache/%s.png", m_BasePath.str, CurrentEnty->Path);
                        const auto stdPath = ToStdString(sceneScreenShotPath);
                        if(std::filesystem::exists(std::filesystem::path(stdPath)))
                        {
                            textureCreated         = true;
                            CurrentEnty->Thumbnail = m_TextureLibrary.get_resource(stdPath);
                            textureId              = CurrentEnty->Thumbnail ? CurrentEnty->Thumbnail : m_FileIcon;
                        }
                        else
                            textureId = m_ArchiveIcon;
                        ArenaTempEnd(scratch);
                    }
                    else
                    {
                        textureId = m_ArchiveIcon;
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
					else if (!textureCreated)
					{
						textureCreated         = true;
						CurrentEnty->Thumbnail = m_AudioIcon;
						textureId              = CurrentEnty->Thumbnail ? CurrentEnty->Thumbnail : m_FileIcon;
					}
					else
					{
						textureId = m_AudioIcon;
					}
                }
                else if (CurrentEnty->Type == FileType::Video)
                {
                    if (CurrentEnty->Thumbnail)
                    {
                        textureId = CurrentEnty->Thumbnail;
                    }
					else if(!textureCreated)
					{
						textureCreated         = true;
						CurrentEnty->Thumbnail = m_VideoIcon;
						textureId              = CurrentEnty->Thumbnail ? CurrentEnty->Thumbnail : m_FileIcon;
					}
					else
					{
						textureId = m_VideoIcon;
					}
                }
                else
                {
                    textureId = m_FileIcon;
                }
            }
            bool flipImage = false;

            bool highlight = false;
            {
                highlight = m_CurrentDir->Children[dirIndex] == m_CurrentSelected;
            }

            // Background button
            bool const clicked = ImGuiHelper::ToggleButton(ImGuiHelper::GenerateID(), highlight, backgroundThumbnailSize, 0.0f, 1.0f, ImGuiButtonFlags_AllowOverlap);
            if(clicked)
            {
                m_CurrentSelected = m_CurrentDir->Children[dirIndex];
                //String8 RelativePath = { m_CurrentSelected->Path.str + m_BasePath.size + 1, m_CurrentSelected->Path.size - m_BasePath.size };
                //IsRenameFile = Str8StdS(m_RenamePath) == RelativePath;
            }
            //whether F2 key is pressed
            if (highlight && Input::get().get_key_pressed(diverse::InputCode::Key::F2))
            {
                String8 RelativePath = { m_CurrentSelected->Path.str + m_BasePath.size + 1, m_CurrentSelected->Path.size - m_BasePath.size };
                m_RenamePath = ToStdString(RelativePath);
                m_CurrentSelected->IsRenaming = true;
            }
            if (highlight && Input::get().get_key_pressed(diverse::InputCode::Key::Delete))
            {
                auto fullPath = m_CurrentDir->Children[dirIndex]->Path;
                std::filesystem::remove_all(std::string((const char*)fullPath.str, fullPath.size));
                queue_refresh();
            }

            if(ImGui::BeginPopupContextItem())
            {
                m_CurrentSelected = m_CurrentDir->Children[dirIndex];

                if(ImGui::Selectable("Cut"))
                {
                    m_CopiedPath = m_CurrentDir->Children[dirIndex]->Path;
                    m_CutFile    = true;
                }

                if(ImGui::Selectable("Copy"))
                {
                    m_CopiedPath = m_CurrentDir->Children[dirIndex]->Path;
                    m_CutFile    = false;
                }

                if(ImGui::Selectable("Delete"))
                {
                    auto fullPath = m_CurrentDir->Children[dirIndex]->Path;
                    std::filesystem::remove_all(std::string((const char*)fullPath.str, fullPath.size));
                    queue_refresh();
                }

                if(ImGui::Selectable("Duplicate"))
                {
                    std::filesystem::path fullPath        = std::string((const char*)m_CurrentDir->Children[dirIndex]->Path.str, m_CurrentDir->Children[dirIndex]->Path.size);
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
                    String8 RelativePath = { m_CurrentSelected->Path.str + m_BasePath.size + 1, m_CurrentSelected->Path.size - m_BasePath.size };
                    m_RenamePath = ToStdString(RelativePath);
                    m_CurrentSelected->IsRenaming = true;
				}

                if(ImGui::Selectable("Open Location"))
                {
                    auto fullPath = m_CurrentDir->Children[dirIndex]->Path;
                    diverse::OS::instance()->openFileLocation(std::string((const char*)fullPath.str, fullPath.size));
                }

                if(m_CurrentDir->Children[dirIndex]->IsFile && ImGui::Selectable("Open External"))
                {
                    auto fullPath = m_CurrentDir->Children[dirIndex]->Path;
                    diverse::OS::instance()->openFileExternal(std::string((const char*)fullPath.str, fullPath.size));
                }

                ImGui::Separator();

                if(ImGui::Selectable("Import New Asset"))
                {
                    m_Editor->open_file();
                }

                if(ImGui::Selectable("Refresh"))
                {
                    queue_refresh();
                }

                if(ImGui::Selectable("New folder"))
                {
                    std::filesystem::create_directory(std::filesystem::path(std::string((const char*)m_CurrentDir->Path.str, m_CurrentDir->Path.size) + "/NewFolder"));
                    queue_refresh();
                }

                if(!m_IsInListView)
                {
                    ImGui::SliderFloat("##GridSize", &grid_size, MinGridSize, MaxGridSize);
                }
                ImGui::EndPopup();
            }

            if(ImGui::IsItemHovered() && m_CurrentDir->Children[dirIndex]->Thumbnail)
            {
                ImGuiHelper::Tooltip(m_CurrentDir->Children[dirIndex]->Thumbnail->gpu_texture, { 512, 512 }, (const char*)(m_CurrentDir->Children[dirIndex]->Path.str));
            }
            else
                ImGuiHelper::Tooltip((const char*)(m_CurrentDir->Children[dirIndex]->Path.str));

            if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::TextUnformatted(m_Editor->get_iconfont_icon(ToStdString(m_CurrentDir->Children[dirIndex]->Path)));

                ImGui::SameLine();
                m_MovePath = m_CurrentDir->Children[dirIndex]->Path;
                ImGui::TextUnformatted((const char*)m_MovePath.str);

                size_t size = sizeof(const char*) + m_MovePath.size;
                ImGui::SetDragDropPayload("AssetFile", m_MovePath.str, size);
                m_IsDragging = true;
                ImGui::EndDragDropSource();
            }

            if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                doubleClicked = true;
            }

            ImGui::SetCursorPos({ cursorPos.x + padding, cursorPos.y + padding });
            ImGui::SetNextItemAllowOverlap();
            ImGui::Image(reinterpret_cast<ImTextureID>(Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(m_TextureLibrary.get_default_resource("white")->gpu_texture)), {backgroundThumbnailSize.x - padding * 2.0f, backgroundThumbnailSize.y - padding * 2.0f}, {0, 0}, {1, 1}, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg) + ImVec4(0.04f, 0.04f, 0.04f, 0.04f));

            ImGui::SetCursorPos({ cursorPos.x + thumbnailPadding * 0.75f, cursorPos.y + thumbnailPadding });
            ImGui::SetNextItemAllowOverlap();
            // ImGui::Image(reinterpret_cast<ImTextureID>(textureId), { thumbnailSize, thumbnailSize }, ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
            ImGuiHelper::Image(textureId->gpu_texture, { thumbnailSize, thumbnailSize });

            const ImVec2 typeColorFrameSize = { scaledThumbnailSizeX, scaledThumbnailSizeX * 0.03f };
            ImGui::SetCursorPosX(cursorPos.x + padding);
            ImGui::Image(reinterpret_cast<ImTextureID>(Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(m_TextureLibrary.get_default_resource("white")->gpu_texture)), typeColorFrameSize, ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f), !CurrentEnty->IsFile ? ImVec4(0.0f, 0.0f, 0.0f, 0.0f) : CurrentEnty->FileTypeColour);

            if (m_CurrentSelected && m_CurrentSelected->IsRenaming && highlight)
            {
                ImGui::SetCursorPos(ImVec2(cursorPos.x + padding * 2, ImGui::GetCursorPosY() + padding * 4));
                if (!ImGuiHelper::InputText(m_RenamePath, "##FileNameChange"))
                {
                    const ImVec2 rectMin = ImGui::GetItemRectMin();
                    const ImVec2 rectSize = ImGui::GetItemRectSize();
                    const ImRect clipRect = ImRect({ rectMin.x, rectMin.y }, { rectMin.x + rectSize.x,rectMin.y + rectSize.y });

                    String8 RelativePath = { m_CurrentSelected->Path.str + m_BasePath.size + 1, m_CurrentSelected->Path.size - m_BasePath.size };
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyReleased(ImGuiKey_Enter))
                    {
                        if (!(Str8StdS(m_RenamePath) == RelativePath)) 
                        {
                            m_RenameFile = true;
                            m_CurrentSelected->IsRenaming = false;
                        }
                        else
                            m_CurrentSelected->IsRenaming = clipRect.Contains(ImGui::GetMousePos());
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
                ImGui::SetCursorPos({ cursorPos.x + padding * (float)m_Editor->get_window()->get_dpi_scale(), cursorPos.y + backgroundThumbnailSize.y - (ImGui::GetIO().Fonts->Fonts[2]->FontSize - padding * (float)m_Editor->get_window()->get_dpi_scale()) * 3.2f });
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
                ImGui::SetCursorPos({ cursorPos.x + padding * (float)m_Editor->get_window()->get_dpi_scale(), cursorPos.y - (ImGui::GetIO().Fonts->Fonts[2]->FontSize * 0.6f - padding * (float)m_Editor->get_window()->get_dpi_scale()) });
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
            ImGui::TextUnformatted(folder ? U8CStr2CStr(ICON_MDI_FOLDER) : m_Editor->get_iconfont_icon(std::string((const char*)m_CurrentDir->Children[dirIndex]->Path.str, m_CurrentDir->Children[dirIndex]->Path.size)));
            ImGui::SameLine();

            if(ImGui::Selectable((const char*)stringutility::get_file_name(m_CurrentDir->Children[dirIndex]->Path, !m_CurrentDir->Children[dirIndex]->IsFile).str, false, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    doubleClicked = true;
                }
            }

            ImGuiHelper::Tooltip((const char*)m_CurrentDir->Children[dirIndex]->Path.str);

            if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::TextUnformatted(m_Editor->get_iconfont_icon(ToStdString(m_CurrentDir->Children[dirIndex]->Path)));

                ImGui::SameLine();
                m_MovePath = m_CurrentDir->Children[dirIndex]->Path;
                ImGui::TextUnformatted((const char*)m_MovePath.str);

                size_t size = sizeof(const char*) + m_MovePath.size;
                ImGui::SetDragDropPayload("AssetFile", m_MovePath.str, size);
                m_IsDragging = true;
                ImGui::EndDragDropSource();
            }
        }

        if(doubleClicked)
        {
            if(folder)
            {
                change_directory(m_CurrentDir->Children[dirIndex]);
            }
            else
            {
                m_Editor->file_open_callback(std::string((const char*)m_CurrentDir->Children[dirIndex]->Path.str, m_CurrentDir->Children[dirIndex]->Path.size));
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
        m_BasePath = PushStr8F(m_Arena, "%sassets", Application::get().get_project_settings().m_ProjectRoot.c_str());

        std::string assetsBasePath;
        FileSystem::get().resolve_physical_path("//assets", assetsBasePath);
        m_AssetPath = PushStr8Copy(m_Arena, Str8C((char*)std::filesystem::path(assetsBasePath).string().c_str()));

        String8 baseDirectoryHandle = process_directory(m_BasePath, nullptr, true);
        m_BaseProjectDir = m_Directories[baseDirectoryHandle];
        change_directory(m_BaseProjectDir);

        m_CurrentDir = m_BaseProjectDir;
        queue_refresh();
    }

    void ResourcePanel::refresh()
    {
        m_TextureLibrary.destroy();

        Arena* temp         = ArenaAlloc(256);
        String8 currentPath = PushStr8Copy(temp, m_CurrentDir->Path);

        ArenaClear(m_Arena);
        m_Directories.clear();

        m_BasePath                  = PushStr8F(m_Arena, "%sassets", Application::get().get_project_settings().m_ProjectRoot.c_str());
        String8 baseDirectoryHandle = process_directory(m_BasePath, nullptr, true);
        m_BaseProjectDir            = m_Directories[baseDirectoryHandle];
        change_directory(m_BaseProjectDir);

        m_UpdateNavigationPath = true;

        m_BaseProjectDir    = m_Directories[baseDirectoryHandle];
        m_PreviousDirectory = nullptr;
        m_CurrentDir        = nullptr;

        bool dirFound = false;
        for(auto dir : m_Directories)
        {
            if(Str8Match(dir.first, currentPath))
            {
                m_CurrentDir = dir.second;
                dirFound     = true;
                break;
            }
        }
        if(dirFound)
            change_directory(m_CurrentDir);

        ArenaRelease(temp);
    }
}
