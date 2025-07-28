#include "redo_undo_system.h"
#include <utility/thread_pool.h>

namespace diverse
{
    auto UndoRedoSystem::can_undo() -> bool
    {
        return cur_op_index > 0;
    }

    auto UndoRedoSystem::can_redo() -> bool
    {
        return cur_op_index < op_history.size();
    }

    void UndoRedoSystem::undo()
    {
        if(can_undo())
        { 
            auto editOp = op_history[--cur_op_index];
            editOp->undo();
        }
    }

    void UndoRedoSystem::redo()
    {
        if(can_redo())
        {
            auto editOp = op_history[cur_op_index++];
            editOp->apply();
        }
    }

    void UndoRedoSystem::add(const std::shared_ptr<EditOperation>& editOp)
    {
        while (cur_op_index < op_history.size()) {
            op_history.pop_back();
        }
        op_history.push_back(editOp);
        redo();
    }

    void UndoRedoSystem::clear()
    {
        cur_op_index = 0;
        op_history.clear();
    }

}