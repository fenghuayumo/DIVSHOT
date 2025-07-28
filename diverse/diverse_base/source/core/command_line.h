#pragma once
#include "core/string.h"

namespace diverse
{
    struct CommandLineOptionNode
    {
        CommandLineOptionNode* next;
        String8 name;
        String8List values;
        String8 value;
    };

    struct CommandLineOptionSlot
    {
        CommandLineOptionNode* first;
        CommandLineOptionNode* last;
    };

    class CommandLine
    {
    public:

        CommandLine() = default;
        ~CommandLine() = default;

        void init(Arena* arena, String8List strings);
        String8List option_strings(String8 name);
        String8 option_string(String8 name);
        bool option_bool(String8 name);
        double option_double(String8 name);
        int64_t option_int64(String8 name);
    private:
        uint64_t m_SlotCount;
        CommandLineOptionSlot* m_Slots;
        String8List m_Inputs;
    };


}
