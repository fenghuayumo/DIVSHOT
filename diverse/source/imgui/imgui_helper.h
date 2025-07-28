#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/fwd.hpp>
#include <iostream>

namespace diverse
{
    namespace rhi
    {
        struct GpuTexture;
    }

    namespace ImGuiHelper
    {
        enum class PropertyFlag
        {
            None = 0,
            ColourProperty = 1,
            ReadOnly = 2,
            DragValue = 3,
            SliderValue = 4,
        };

        enum Theme
        {
            Black = 0,
            Dark,
            Dracula,
            Grey,
            Light,
            Blue,
            ClassicLight,
            ClassicDark,
            Classic,
            Cherry,
            Cinder,
            Cosy
        };

        bool Property(const char* name, std::string& value, PropertyFlag flags = PropertyFlag::ReadOnly);
        void PropertyConst(const char* name, const char* value);
        bool Property(const char* name, bool& value, const char* text_tooltip = nullptr,PropertyFlag flags = PropertyFlag::None);
        bool Property(const char* name, int& value, PropertyFlag flags);
        bool Property(const char* name, uint32_t& value, const char* text_tooltip = nullptr, PropertyFlag flags = PropertyFlag::None);
        bool PropertyMultiline(const char* label, std::string& value);

        bool Property(const char* name, double& value, double min = -1.0, double max = 1.0, PropertyFlag flags = PropertyFlag::None);

        bool Property(const char* name, int& value, int min = 0, int max = 100,const char* text_tooltip = nullptr,PropertyFlag flags = PropertyFlag::None);

        bool Property(const char* name, float& value, float min = -1.0f, float max = 1.0f, float delta = 1.0f, PropertyFlag flags = PropertyFlag::None);
        bool Property(const char* name, glm::vec2& value, PropertyFlag flags);
        bool Property(const char* name, glm::vec2& value, float min = -1.0f, float max = 1.0f, PropertyFlag flags = PropertyFlag::None);
        bool Property(const char* name, glm::vec3& value, PropertyFlag flags);
        bool Property(const char* name, glm::vec3& value, float min = -1.0f, float max = 1.0f, PropertyFlag flags = PropertyFlag::None);
        bool Property(const char* name, glm::vec4& value, bool exposeW, PropertyFlag flags);
        bool Property(const char* name, glm::vec4& value, float min = -1.0f, float max = 1.0f, bool exposeW = false, PropertyFlag flags = PropertyFlag::None);
        bool PropertyTransform(const char* name, glm::vec3& vector, float width, float defaultElementValue = 0.0f);
        bool PropertyVector3(const char* label, glm::vec3& values, float columnWidth = 100.0f, float resetValue = 0.0f);
        bool Property(const char* name, glm::quat& value, PropertyFlag flags);

        bool PropertyDropdown(const char* label, const char** options, int32_t optionCount, int32_t* selected);

        void Tooltip(const char* text);

        void Tooltip(const std::shared_ptr<rhi::GpuTexture>& texture, const glm::vec2& size);
        void Tooltip(const std::shared_ptr<rhi::GpuTexture>& texture, const glm::vec2& size, const char* text);
        void Tooltip(const std::shared_ptr<rhi::GpuTexture>& texture, uint32_t index, const glm::vec2& size);

        void Image(const std::shared_ptr<rhi::GpuTexture>& texture, const glm::vec2& size, bool flip = false, const ImVec4& col = ImVec4(1,1,1,1));
        void Image(const std::shared_ptr<rhi::GpuTexture>& texture, uint32_t index, const glm::vec2& size, bool flip = false);

        void TextCentred(const char* text);

        void SetTheme(Theme theme);

        bool BufferingBar(const char* label, float value, const glm::vec2& size_arg, const uint32_t& bg_col, const uint32_t& fg_col);
        bool Spinner(const char* label, float radius, int thickness, const uint32_t& colour);

        void DrawRowsBackground(int row_count, float line_height, float x1, float x2, float y_offset, ImU32 col_even, ImU32 col_odd);
        glm::vec4 GetSelectedColour();
        glm::vec4 GetIconColour();

        void DrawItemActivityOutline(float rounding = 0.0f, bool drawWhenInactive = false, ImColor colourWhenActive = ImColor(80, 80, 80));
        bool InputText(std::string& currentText, const char* ID);
        void ClippedText(const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_end, const ImVec2* text_size_if_known, const ImVec2& align, const ImRect* clip_rect, float wrap_width);
        void ClippedText(ImDrawList* draw_list, const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_display_end, const ImVec2* text_size_if_known, const ImVec2& align, const ImRect* clip_rect, float wrap_width);

        void AlternatingRowsBackground(float lineHeight = -1.0f);

        ImRect GetItemRect();

        ImRect RectExpanded(const ImRect& rect, float x, float y);
        ImRect RectOffset(const ImRect& rect, float x, float y);

        ImRect RectOffset(const ImRect& rect, ImVec2 xy);

        void DrawBorder(ImVec2 rectMin, ImVec2 rectMax, const ImVec4& borderColour, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f);

        void DrawBorder(const ImVec4& borderColour, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f);
        void DrawBorder(float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f);
        void DrawBorder(ImVec2 rectMin, ImVec2 rectMax, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f);

        void DrawBorder(ImRect rect, float thickness = 1.0f, float rounding = 0.0f, float offsetX = 0.0f, float offsetY = 0.0f);
        const char* GenerateID();
        const char* GenerateLabelID(std::string_view label);
        void PushID();
        void PopID();

        bool ToggleButton(const char* label, bool state, ImVec2 size, float alpha, float pressedAlpha, ImGuiButtonFlags buttonFlags);
        std::string MillisecToString(int64_t millisec, int show_millisec);

        class ScopedStyle
        {
        public:
            ScopedStyle(const ScopedStyle&) = delete;
            ScopedStyle operator=(const ScopedStyle&) = delete;
            template <typename T>
            ScopedStyle(ImGuiStyleVar styleVar, T value) { ImGui::PushStyleVar(styleVar, value); }
            ~ScopedStyle() { ImGui::PopStyleVar(); }
        };

        class ScopedColour
        {
        public:
            ScopedColour(const ScopedColour&) = delete;
            ScopedColour operator=(const ScopedColour&) = delete;
            template <typename T>
            ScopedColour(ImGuiCol colourId, T colour) { ImGui::PushStyleColor(colourId, colour); }
            ~ScopedColour() { ImGui::PopStyleColor(); }
        };

        class ScopedFont
        {
        public:
            ScopedFont(const ScopedFont&) = delete;
            ScopedFont operator=(const ScopedFont&) = delete;
            ScopedFont(ImFont* font) { ImGui::PushFont(font); }
            ~ScopedFont() { ImGui::PopFont(); }
        };

        class ScopedID
        {
        public:
            ScopedID(const ScopedID&) = delete;
            ScopedID operator=(const ScopedID&) = delete;
            template <typename T>
            ScopedID(T id) { ImGui::PushID(id); }
            ~ScopedID() { ImGui::PopID(); }
        };

        float EmSize();
        float EmSize(float nbLines);
        ImVec2 EmToVec2(float x, float y);
        ImVec2 EmToVec2(ImVec2 v);

        //asset
        void ImageFromAsset(const char *assetPath, const ImVec2& size = ImVec2(0, 0), const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1,1), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0));
        bool ImageButtonFromAsset(const char *assetPath, const ImVec2& size = ImVec2(0, 0), const ImVec2& uv0 = ImVec2(0, 0),  const ImVec2& uv1 = ImVec2(1,1), const ImVec4& bg_col = ImVec4(0,0,0,0), const ImVec4& tint_col = ImVec4(1,1,1,1));
        ImVec2 ImageSizeFromAsset(const char *assetPath);
        ImVec2 ImageProportionalSize(const ImVec2& askedSize, const ImVec2& imageSize);

        int messageBox(const char* title, const char* message,std::function<void()> f = {});
        bool Button(const char* label,int id = 0, const ImVec2& size_arg = ImVec2(0, 0));
        bool MenuItem(const char* label,const char* iconPath=nullptr,const char* keyshort = nullptr);
        bool MenuItem(const char* label, const std::string& iconPath, const char* keyshort = nullptr);
        bool BeginMenu(const char* label, const char* iconPath = nullptr,bool enbaled = true);
        bool BeginMenu(const char* label, const std::string& iconPath, bool enbaled = true);
        void EndMenu();

        bool ProgressWindow(const char* label,f32 progress, const ImVec2& progress_size, bool bModal = false);
    }
}

namespace ImGui
{
    // Dupe of DragFloatN with a tweak to add coloured lines
    bool DragFloatN_Coloured(const char* label, float* v, int components, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* display_format = "%.2f");

    bool DragFloat3Coloured(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f);
    bool DragFloat4Coloured(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f);
    bool DragFloat2Coloured(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f);

    void PushMultiItemsWidthsAndLabels(const char* labels[], int components, float w_full);
    bool DragFloatNEx(const char* labels[], float* v, int components, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* display_format = "%.3f");

    void LoadingIndicatorCircle(const char* label,
        const float radius,
        int thickness,
        const ImU32& color,
        const ImVec2& pos);
    void LoadingIndicatorCircle(const char* label,
        const float indicator_radius,
        const ImVec2& pos,
        const ImVec4& main_color,
        const ImVec4& backdrop_color,
        const float speed);
}
static inline ImVec2 operator*(const ImVec2& lhs, const float rhs)
{
    return ImVec2(lhs.x * rhs, lhs.y * rhs);
}
static inline ImVec2 operator/(const ImVec2& lhs, const float rhs)
{
    return ImVec2(lhs.x / rhs, lhs.y / rhs);
}
static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
    return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs)
{
    return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}
static inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs)
{
    return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y);
}
static inline ImVec2 operator/(const ImVec2& lhs, const ImVec2& rhs)
{
    return ImVec2(lhs.x / rhs.x, lhs.y / rhs.y);
}
static inline ImVec2& operator+=(ImVec2& lhs, const ImVec2& rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}
static inline ImVec2& operator-=(ImVec2& lhs, const ImVec2& rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    return lhs;
}
static inline ImVec2& operator*=(ImVec2& lhs, const float rhs)
{
    lhs.x *= rhs;
    lhs.y *= rhs;
    return lhs;
}
static inline ImVec2& operator/=(ImVec2& lhs, const float rhs)
{
    lhs.x /= rhs;
    lhs.y /= rhs;
    return lhs;
}
static inline ImVec4 operator-(const ImVec4& lhs, const ImVec4& rhs)
{
    return ImVec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
}
static inline ImVec4 operator+(const ImVec4& lhs, const ImVec4& rhs)
{
    return ImVec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
}
static inline ImVec4 operator*(const ImVec4& lhs, const float rhs)
{
    return ImVec4(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs);
}
static inline ImVec4 operator/(const ImVec4& lhs, const float rhs)
{
    return ImVec4(lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs);
}
static inline ImVec4 operator*(const ImVec4& lhs, const ImVec4& rhs)
{
    return ImVec4(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w);
}
static inline ImVec4 operator/(const ImVec4& lhs, const ImVec4& rhs)
{
    return ImVec4(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w);
}
static inline ImVec4& operator+=(ImVec4& lhs, const ImVec4& rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    lhs.z += rhs.z;
    lhs.w += rhs.w;
    return lhs;
}
static inline ImVec4& operator-=(ImVec4& lhs, const ImVec4& rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    lhs.z -= rhs.z;
    lhs.w -= rhs.w;
    return lhs;
}
static inline ImVec4& operator*=(ImVec4& lhs, const float rhs)
{
    lhs.x *= rhs;
    lhs.y *= rhs;
    return lhs;
}
static inline ImVec4& operator/=(ImVec4& lhs, const float rhs)
{
    lhs.x /= rhs;
    lhs.y /= rhs;
    return lhs;
}
static inline std::ostream& operator<<(std::ostream& ostream, const ImVec2 a)
{
    ostream << "{ " << a.x << ", " << a.y << " }";
    return ostream;
}
static inline std::ostream& operator<<(std::ostream& ostream, const ImVec4 a)
{
    ostream << "{ " << a.x << ", " << a.y << ", " << a.z << ", " << a.w << " }";
    return ostream;
}
