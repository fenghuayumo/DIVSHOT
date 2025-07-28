#pragma once
#include "splat_edit_op.h"
#include <memory>
#include <vector>
#include <deque>
namespace diverse
{
    struct  SplatEditHistory
    {
        /* data */
        auto can_undo() -> bool;
        auto can_redo() -> bool;
        void undo();
        void redo();
        void add(const std::shared_ptr<SplatEditOperation>& op);
        void clear();
        u32 cur_op_index = 0;

        std::deque<std::shared_ptr<SplatEditOperation>> op_history;
    };
    
}