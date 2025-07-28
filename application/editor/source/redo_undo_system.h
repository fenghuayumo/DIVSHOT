#pragma once
#include <vector>
#include <deque>
#include <utility/singleton.h>
#include "edit_op.h"
namespace diverse
{
    class UndoRedoSystem : public ThreadSafeSingleton<UndoRedoSystem>
    {
    public:

        void undo();
        void redo();
        void add(const std::shared_ptr<EditOperation>& editOp);
        auto can_undo() -> bool;
        auto can_redo() -> bool;
        void clear();
    private:
        u32 cur_op_index = 0;

        std::deque<std::shared_ptr<EditOperation>> op_history;
    };
}