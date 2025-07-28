#pragma once
#include "core.h"
#include <stdint.h>
#include <atomic>

namespace diverse
{
    struct DS_EXPORT ReferenceCounter
    {
        std::atomic<int> count = 0;

    public:
        inline bool sharedPtr()
        {
            count++;
            return count != 0;
        }

        inline int refValue()
        {
            count++;
            return count;
        }

        inline bool unref()
        {
            --count;
            bool del = count == 0;
            return del;
        }

        inline int get() const
        {
            return count;
        }

        inline void init(int p_value = 1)
        {
            count = p_value;
        }
    };
}
