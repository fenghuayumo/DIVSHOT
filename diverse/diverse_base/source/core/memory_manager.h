#pragma once

namespace diverse
{
    struct SystemMemoryInfo
    {
        int64_t availablePhysicalMemory;
        int64_t totalPhysicalMemory;

        int64_t availableVirtualMemory;
        int64_t totalVirtualMemory;

        void log();
    };

    struct MemoryStats
    {
        int64_t totalAllocated;
        int64_t totalFreed;
        int64_t currentUsed;
        int64_t totalAllocations;

        MemoryStats()
            : totalAllocated(0)
            , totalFreed(0)
            , currentUsed(0)
            , totalAllocations(0)
        {
        }
    };

    class MemoryManager
    {
    public:
        static MemoryManager* s_Instance;

    public:
        MemoryStats m_MemoryStats;

    public:
        MemoryManager();

        static void on_init();
        static void on_shutdown();

        static MemoryManager* get();
        inline MemoryStats get_memory_stats() const
        {
            return m_MemoryStats;
        }

    public:
        SystemMemoryInfo get_system_info();

    public:
        static std::string bytes_to_string(int64_t bytes);
    };
}
