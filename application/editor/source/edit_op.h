#pragma once
#include <maths/transform.h>
#include <vector>
#include <unordered_map>
namespace diverse
{
    class EditOperation
    {
    public:
        EditOperation() = default;
        virtual ~EditOperation() = default;
        virtual void apply() = 0;
        virtual void undo() = 0;
    };
}