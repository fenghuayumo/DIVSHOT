#include "core/core.h"
#include "core/profiler.h"
#include "core/ds_log.h"
#include "memory.h"
#include "core/allocators/bin_allocator.h"
#include "core/allocators/default_allocator.h"
#include "core/allocators/stb_allocator.h"

namespace diverse
{
#ifndef DS_PRODUCTION
    static Arena* s_Arenas[256]; // For Stats
    static int s_CurrentArenaCount = 0;
#endif

    int GetArenaCount()
    {
#ifndef DS_PRODUCTION
        return s_CurrentArenaCount;
#else
        return 0;
#endif
    }

    Arena* GetArena(int index)
    {
#ifndef DS_PRODUCTION
        return s_Arenas[index];
#else
        return nullptr;
#endif
    }

    Allocator* const Memory::MemoryAllocator = nullptr; // new DefaultAllocator();

    void* Memory::AlignedAlloc(size_t size, size_t alignment)
    {
        void* data;
#if defined(DS_PLATFORM_WINDOWS)
        data = _aligned_malloc(size, alignment);
#else
        int res = posix_memalign(&data, alignment, size);
        if(res != 0)
            data = nullptr;
#endif
        return data;
    }

    void Memory::AlignedFree(void* data)
    {
#if defined(DS_PLATFORM_WINDOWS)
        _aligned_free(data);
#else
        free(data);
#endif
    }

    void* Memory::NewFunc(std::size_t size, const char* file, int line)
    {
        void* memory;
        if(MemoryAllocator)
            memory = MemoryAllocator->Malloc(size, file, line);
        else
            memory = malloc(size);

        if(memory == nullptr)
        {
            throw std::bad_alloc();
        }

//#if defined(DS_PROFILE) && defined(TRACY_ENABLE) && DS_TRACK_MEMORY
//        TracyAlloc(memory, size);
//#endif

        return memory;
    }

    void Memory::DeleteFunc(void* p)
    {
//#if defined(DS_PROFILE) && defined(TRACY_ENABLE) && DS_TRACK_MEMORY
//        TracyFree(p);
//#endif

        if(MemoryAllocator)
            return MemoryAllocator->Free(p);
        else
            return free(p);
    }

    void Memory::LogMemoryInformation()
    {
        if(MemoryAllocator)
            return MemoryAllocator->Print();
    }

    // Arenas
    Arena* ArenaAlloc(uint64_t size)
    {
        Arena* arena          = (Arena*)malloc(sizeof(Arena) + size);
        arena->Position       = sizeof(Arena);
        arena->CommitPosition = sizeof(Arena);
        arena->Align          = alignof(std::max_align_t);
        arena->Size           = size;
        arena->Ptr            = arena;

#ifndef DS_PRODUCTION
        if(s_CurrentArenaCount < 256)
            s_Arenas[s_CurrentArenaCount++] = arena;
#endif
//#if defined(DS_PROFILE) && defined(TRACY_ENABLE) && DS_TRACK_MEMORY
//        TracyAlloc(arena, size);
//#endif

        return arena;
    }

    Arena* ArenaAllocDefault()
    {
        return ArenaAlloc(4096);
    }

    void ArenaRelease(Arena* arena)
    {
        if(arena)
        {
#ifndef DS_PRODUCTION
            s_CurrentArenaCount--;
#endif
            free(arena);

//#if defined(DS_PROFILE) && defined(TRACY_ENABLE) && DS_TRACK_MEMORY
//            TracyFree(arena);
//#endif
        }
    }

    void* ArenaPushNoZero(Arena* arena, uint64_t size)
    {
        DS_ASSERT(arena != nullptr);
        uint64_t alignedSize = (size + arena->Align - 1) & ~(arena->Align - 1);
        uint64_t newPos      = arena->Position + alignedSize;

        if(newPos > arena->Size)
        {
            DS_LOG_ERROR("Not enough space in the arena");
            return nullptr;
        }

        void* ptr       = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(arena->Ptr) + arena->Position);
        arena->Position = newPos;
        return ptr;
    }

    void* ArenaPushAligner(Arena* arena, uint64_t alignment)
    {
        DS_ASSERT(arena != nullptr);
        DS_ASSERT((alignment & (alignment - 1)) == 0); // Ensure alignment is a power of 2

        uint64_t currentAddr      = reinterpret_cast<uintptr_t>(arena->Ptr) + arena->Position;
        uint64_t alignedAddr      = (currentAddr + alignment - 1) & ~(alignment - 1);
        uint64_t alignmentPadding = alignedAddr - currentAddr;
        uint64_t newPos           = arena->Position + alignmentPadding;

        if(newPos > arena->Size)
        {
            return nullptr; // Not enough space in the arena
        }

        void* ptr       = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(arena->Ptr) + newPos);
        arena->Position = newPos;
        return ptr;
    }

    void* ArenaPush(Arena* arena, uint64_t size)
    {
        void* ptr = ArenaPushNoZero(arena, size);
        if(ptr)
        {
            memset(ptr, 0, size); // Zero initialize the memory
        }
        return ptr;
    }

    void ArenaPopTo(Arena* arena, uint64_t pos)
    {
        DS_ASSERT(arena != nullptr);
        DS_ASSERT(pos <= arena->Position);
        arena->Position = pos;
    }

    void ArenaSetAutoAlign(Arena* arena, uint64_t align)
    {
        DS_ASSERT(arena != nullptr);
        arena->Align = align;
    }

    void ArenaPop(Arena* arena, uint64_t size)
    {
        DS_ASSERT(arena != nullptr);
        DS_ASSERT(size <= arena->Position);
        arena->Position -= size;
    }

    void ArenaClear(Arena* arena)
    {
        DS_ASSERT(arena != nullptr);
        arena->Position = sizeof(Arena);
    }

    uint64_t ArenaPos(Arena* arena)
    {
        DS_ASSERT(arena != nullptr);
        return arena->Position;
    }

    ArenaTemp ArenaTempBegin(Arena* arena)
    {
        DS_ASSERT(arena != nullptr);
        return { arena, arena->Position };
    }

    void ArenaTempEnd(ArenaTemp temp)
    {
        DS_ASSERT(temp.arena != nullptr);
        ArenaPopTo(temp.arena, temp.pos);
    }
}
#if defined(CUSTOM_MEMORY_ALLOCATOR)
#ifdef DS_PLATFORM_WINDOWS

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size)
{
    return diverse::Memory::NewFunc(size, __FILE__, __LINE__);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size)
{
    return diverse::Memory::NewFunc(size, __FILE__, __LINE__);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* desc)
{
    return diverse::Memory::NewFunc(size, desc, __LINE__);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* desc)
{
    return diverse::Memory::NewFunc(size, desc, __LINE__);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* file, int line)
{
    return diverse::Memory::NewFunc(size, file, line);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* file, int line)
{
    return diverse::Memory::NewFunc(size, file, line);
}

void __CRTDECL operator delete(void* memory)
{
    diverse::Memory::DeleteFunc(memory);
}

void __CRTDECL operator delete(void* memory, const char* desc)
{
    diverse::Memory::DeleteFunc(memory);
}

void __CRTDECL operator delete(void* memory, const char* file, int line)
{
    diverse::Memory::DeleteFunc(memory);
}

void __CRTDECL operator delete[](void* memory)
{
    diverse::Memory::DeleteFunc(memory);
}

void __CRTDECL operator delete[](void* memory, const char* desc)
{
    diverse::Memory::DeleteFunc(memory);
}

void __CRTDECL operator delete[](void* memory, const char* file, int line)
{
    diverse::Memory::DeleteFunc(memory);
}
#else
void* operator new(size_t size)
{
    return diverse::Memory::NewFunc(size, __FILE__, __LINE__);
}

void* operator new[](size_t size)
{
    return diverse::Memory::NewFunc(size, __FILE__, __LINE__);
}

void* operator new(size_t size, const char* desc)
{
    return diverse::Memory::NewFunc(size, desc, __LINE__);
}

void* operator new[](size_t size, const char* desc)
{
    return diverse::Memory::NewFunc(size, desc, __LINE__);
}

void* operator new(size_t size, const char* file, int line)
{
    return diverse::Memory::NewFunc(size, file, line);
}

void* operator new[](size_t size, const char* file, int line)
{
    return diverse::Memory::NewFunc(size, file, line);
}

void operator delete(void* memory)
{
    diverse::Memory::DeleteFunc(memory);
}

void operator delete(void* memory, const char* desc)
{
    diverse::Memory::DeleteFunc(memory);
}

void operator delete(void* memory, const char* file, int line)
{
    diverse::Memory::DeleteFunc(memory);
}

void operator delete[](void* memory)
{
    diverse::Memory::DeleteFunc(memory);
}

void operator delete[](void* memory, const char* desc)
{
    diverse::Memory::DeleteFunc(memory);
}

void operator delete[](void* memory, const char* file, int line)
{
    diverse::Memory::DeleteFunc(memory);
}
#endif
#endif