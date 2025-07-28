#include "precompile.h"
#include "core_system.h"
#include "file_system.h"
#include "core/job_system.h"
#include "core/version.h"
#include "core/command_line.h"
#include "core/thread.h"
#include "core/memory_manager.h"

namespace diverse
{
    static Arena* s_Arena;
    static CommandLine s_CommandLine;

    bool coresystem::init(int argc, char** argv)
    {
        debug::Log::init();
        ThreadContext& mainThread = *GetThreadContext();
        mainThread = ThreadContextAlloc();
        SetThreadName(Str8Lit("Main"));

        s_Arena = ArenaAlloc(Megabytes(2));

        DS_LOG_INFO("Diverse Engine - Version {0}.{1}.{2}", DiverseVersion.major, DiverseVersion.minor, DiverseVersion.patch);

        String8List argsList = {};
        for (uint64_t argumentIndex = 1; argumentIndex < argc; argumentIndex += 1)
        {
            Str8ListPush(s_Arena, &argsList, Str8C(argv[argumentIndex]));
        }
        s_CommandLine = {};
        s_CommandLine.init(s_Arena, argsList);

        if (s_CommandLine.option_bool(Str8Lit("help")))
        {
            DS_LOG_INFO("Print this help. This help message is actually so long "
                "that it requires a line break!");
        }

        // Init Jobsystem. Reserve 2 threads for main and render threads
        System::JobSystem::init(2);
        DS_LOG_INFO("Initialising System");
        FileSystem::get();

        return true;
    }

    void coresystem::shut_down()
    {
        DS_LOG_INFO("Shutting down System");
        FileSystem::release();
        diverse::Memory::LogMemoryInformation();

        debug::Log::release();
        System::JobSystem::release();

        ArenaClear(s_Arena);

        MemoryManager::on_shutdown();
        ThreadContextRelease(GetThreadContext());
    }

    CommandLine* coresystem::get_command_line()
    {
        return &s_CommandLine;
    }
}
