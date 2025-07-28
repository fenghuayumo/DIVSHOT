#include "splat_edit_history.h"

auto diverse::SplatEditHistory::can_undo() -> bool
{
    return cur_op_index > 0;
}

auto diverse::SplatEditHistory::can_redo() -> bool
{
    return cur_op_index < op_history.size();
}

void diverse::SplatEditHistory::undo()
{
    auto editOp = op_history[--cur_op_index];
    editOp->undo();
}

void diverse::SplatEditHistory::redo()
{
	auto editOp = op_history[cur_op_index++];
	editOp->apply();
}

void diverse::SplatEditHistory::add(const std::shared_ptr<SplatEditOperation>& editOp)
{
    while (cur_op_index  < op_history.size()) {
        op_history.pop_back();
    }
    op_history.push_back(editOp);
    redo();
}

void diverse::SplatEditHistory::clear()
{
    cur_op_index = 0;
    op_history.clear();
}
