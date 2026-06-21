#pragma once

#include "scene/schedule/schedule.h"

namespace diverse
{
    class Editor;

    namespace schedule
    {
        class EditorSystemsPlugin : public Plugin
        {
        public:
            explicit EditorSystemsPlugin(Editor* editor);

            void build(Schedule& schedule) override;
            const char* name() const override { return "EditorSystems"; }

        private:
            Editor* m_editor = nullptr;
        };
    }
}
