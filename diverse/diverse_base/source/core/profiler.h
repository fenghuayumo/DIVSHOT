#pragma once

#if DS_PROFILE_ENABLED && !defined(DS_PROFILE)
#define DS_PROFILE 1
#endif
#if DS_PROFILE
#ifdef DS_PLATFORM_WINDOWS
#define TRACY_CALLSTACK 1
#endif

#define DS_TRACK_MEMORY 1
#define DS_PROFILE_LOW 0
#define DS_PROFILE_GPU_TIMINGS 0
#define DS_VULKAN_MARKERS 0 // Disable when using OpenGL

#include <Tracy/public/tracy/Tracy.hpp>
#define DS_PROFILE_SCOPE(name) ZoneScopedN(name)
#define DS_PROFILE_FUNCTION() ZoneScoped
#define DS_PROFILE_FRAMEMARKER() FrameMark
#define DS_PROFILE_LOCK(type, var, name) TracyLockableN(type, var, name)
#define DS_PROFILE_LOCKMARKER(var) LockMark(var)
#define DS_PROFILE_SETTHREADNAME(name) tracy::SetThreadName(name)

#if DS_PROFILE_LOW
#define DS_PROFILE_FUNCTION_LOW() ZoneScoped
#define DS_PROFILE_SCOPE_LOW(name) ZoneScopedN(name)
#else
#define DS_PROFILE_FUNCTION_LOW()
#define DS_PROFILE_SCOPE_LOW(name)
#endif

#else
#define DS_PROFILE_SCOPE(name)
#define DS_PROFILE_FUNCTION()
#define DS_PROFILE_FRAMEMARKER()
#define DS_PROFILE_LOCK(type, var, name) type var
#define DS_PROFILE_LOCKMARKER(var)
#define DS_PROFILE_SETTHREADNAME(name)
#define DS_PROFILE_FUNCTION_LOW()
#define DS_PROFILE_SCOPE_LOW(name)

#endif
