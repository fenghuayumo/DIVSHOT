#include "core/core.h"
#include <string>
#include <cstdlib>
#include "default_allocator.h"

#include "core/memory_manager.h"
// #define TRACK_ALLOCATIONS

#define DS_MEMORY_ALIGNMENT 16
#define DS_ALLOC(size) _aligned_malloc(size, DS_MEMORY_ALIGNMENT)
#define DS_FREE(block) _aligned_free(block);

namespace diverse
{
    void* DefaultAllocator::Malloc(size_t size, const char* file, int line)
    {
#ifdef TRACK_ALLOCATIONS
        DS_ASSERT(size < 1024 * 1024 * 1024, "Allocation more than max size");

        diverse::MemoryManager::Get()->m_MemoryStats.totalAllocated += size;
        diverse::MemoryManager::Get()->m_MemoryStats.currentUsed += size;
        diverse::MemoryManager::Get()->m_MemoryStats.totalAllocations++;

        size_t actualSize = size + sizeof(size_t);
        void* mem         = malloc(actualSize + sizeof(void*));

        uint8_t* result = (uint8_t*)mem;
        if(result == NULL)
        {
            DS_LOG_ERROR("Aligned malloc failed");
            return NULL;
        }

        memset(result, 0, actualSize);
        memcpy(result, &size, sizeof(size_t));
        result += sizeof(size_t);

        return result;

#else
        return malloc(size);
#endif
    }

    void DefaultAllocator::Free(void* location)
    {
#ifdef TRACK_ALLOCATIONS
        uint8_t* memory = ((uint8_t*)location) - sizeof(size_t);
        if(location && memory)
        {
            uint8_t* memory = ((uint8_t*)location) - sizeof(size_t);
            size_t size     = *(size_t*)memory;
            free(((void**)memory));
            diverse::MemoryManager::Get()->m_MemoryStats.totalFreed += size;
            diverse::MemoryManager::Get()->m_MemoryStats.currentUsed -= size;
        }
        else
        {
            free(location);
        }

#else
        free(location);
#endif
    }
}
