#pragma once

struct JobDispatchArgs
{
    uint32_t jobIndex;
    uint32_t groupID;
    uint32_t groupIndex;    // group index relative to dispatch (like SV_GroupID in HLSL)
    bool isFirstJobInGroup; // is the current job the first one in the group?
    bool isLastJobInGroup;  // is the current job the last one in the group?
    void* sharedmemory;
};

namespace diverse
{
    namespace System
    {
        namespace JobSystem
        {
            void init(uint32_t reservedThreads = 1);
            void release();

            uint32_t get_thread_count();

            struct Context
            {
                std::atomic<uint32_t> counter { 0 };
            };

            // Add a job to execute asynchronously. Any idle thread will execute this job.
            void execute(Context& ctx, const std::function<void(JobDispatchArgs)>& task);

            // Divide a job onto multiple jobs and execute in parallel.
            //	jobCount	: how many jobs to generate for this task.
            //	groupSize	: how many jobs to execute per thread. Jobs inside a group execute serially. It might be worth to increase for small jobs
            //	func		: receives a JobDispatchArgs as parameter
            void dispatch(Context& ctx, uint32_t jobCount, uint32_t groupSize, const std::function<void(JobDispatchArgs)>& task, size_t sharedmemory_size = 0);

            uint32_t dispatch_group_count(uint32_t jobCount, uint32_t groupSize);

            // Check if any threads are working currently or not
            bool is_busy(const Context& ctx);

            // Wait until all threads become idle
            void wait(const Context& ctx);
        }
    }
}
