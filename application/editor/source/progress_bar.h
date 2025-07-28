#pragma once
#include "editor_panel.h"
#include "editor.h"
#include <maths/frustum.h>
#include <maths/transform.h>
#include <core/string.h>
#include <utility/string_utils.h>
#include <imgui/imgui_helper.h>
#include <maths/maths_utils.h>

#include <imgui/imgui.h>
namespace diverse
{
    class ProgressBarPanel : public EditorPanel
    {
    public:
        ProgressBarPanel();
        ~ProgressBarPanel() = default;

        void on_imgui_render() override;
        void on_new_scene(Scene* scene) override;
    };
}