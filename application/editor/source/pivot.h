#pragma once
#include <maths/transform.h>

namespace diverse
{
    class Pivot
    {
    public:
        Pivot(const maths::Transform& t)
        : transform(t)
        {
        }

        void set_transform(const maths::Transform& t){transform = t;}
        maths::Transform& get_transform() {return transform;}
        maths::Transform transform;
    };
}