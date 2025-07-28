#include "core/string.h"
#include "core/core.h"
#include "core/ds_log.h"
#include "memory_manager.h"
#include "utility/string_utils.h"

namespace diverse
{
    MemoryManager* MemoryManager::s_Instance = nullptr;

    MemoryManager::MemoryManager()
    {
    }

    void MemoryManager::on_init()
    {
    }

    void MemoryManager::on_shutdown()
    {
        if(s_Instance)
            delete s_Instance;
    }

    MemoryManager* MemoryManager::get()
    {
        if(s_Instance == nullptr)
        {
            s_Instance = new MemoryManager();
        }
        return s_Instance;
    }

    void SystemMemoryInfo::log()
    {
        std::string apm, tpm, avm, tvm;

        apm = stringutility::byte_2_string(availablePhysicalMemory);
        tpm = stringutility::byte_2_string(totalPhysicalMemory);
        avm = stringutility::byte_2_string(availableVirtualMemory);
        tvm = stringutility::byte_2_string(totalVirtualMemory);

        DS_LOG_INFO("Memory Info:");
        DS_LOG_INFO("\tPhysical Memory : {0} / {1}", apm, tpm);
        DS_LOG_INFO("\tVirtual Memory : {0} / {1}: ", avm, tvm);
    }
}
