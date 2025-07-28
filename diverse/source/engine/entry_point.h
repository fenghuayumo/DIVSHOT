#if defined(DS_PLATFORM_WINDOWS)

#include "engine/core_system.h"
#include "platform/windows/windows_os.h"

#ifndef NOMINMAX
#define NOMINMAX // For windows.h
#endif

#include <windows.h>

extern diverse::Application* diverse::createApplication();

#ifdef DS_PRODUCTION
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    if (!diverse::coresystem::init(__argc, __argv))
        return 0;
#else
#pragma comment(linker, "/subsystem:console")

int main(int argc, char** argv)
{
    if (!diverse::coresystem::init(argc, argv))
        return 0;
#endif

    auto windowsOS = new diverse::WindowsOS();
    diverse::OS::setInstance(windowsOS);

    windowsOS->init();

    diverse::createApplication();

    windowsOS->run();
    delete windowsOS;

    diverse::coresystem::shut_down();
    return 0;
}

#elif defined(DS_PLATFORM_LINUX)

#include "core/core_system.h"
#include "platform/Unix/UnixOS.h"

extern diverse::Application* diverse::CreateApplication();

int main(int argc, char** argv)
{
    if (!diverse::coresystem::init(argc, argv))
        return 0;

    auto unixOS = new diverse::UnixOS();
    diverse::OS::SetInstance(unixOS);
    unixOS->Init();

    diverse::createApplication();

    unixOS->run();
    delete unixOS;

    diverse::coresystem::shut_down();
}

#elif defined(DS_PLATFORM_MACOS)

#include "platform/macos/mac_os.h"

int main(int argc, char** argv)
{
    if (!diverse::coresystem::init(argc, argv))
        return 0;

    auto macOSOS = new diverse::MacOSOS();
    diverse::OS::setInstance(macOSOS);
    macOSOS->init();

    diverse::createApplication();

    macOSOS->run();
    delete macOSOS;

    diverse::coresystem::shut_down();
}

#elif defined(DS_PLATFORM_IOS)

#endif
