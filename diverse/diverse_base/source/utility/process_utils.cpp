#include "process_utils.hpp"
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <array>
#ifdef DS_PLATFORM_WINDOWS
#include <windows.h>
#endif
#include "core/string.h"
#include "utility/string_utils.h"

namespace diverse
{
    auto lauch_process(const std::string& commandLine) -> int
    {
#ifdef DS_PLATFORM_WINDOWS
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        // Construct the command line string.  Note the careful handling of spaces.
        // std::string commandLine = "my_program.exe arg1 \"arg with spaces\" arg3"; 
        std::wstring wCommandLine = std::wstring(commandLine.begin(), commandLine.end());
        const wchar_t* commandLinePtr = wCommandLine.c_str();

        // Convert to TCHAR for CreateProcess
        if (!CreateProcessW(NULL,  // Use the command line to get the executable name
            const_cast<LPWSTR>(commandLinePtr), // Command line with arguments
            NULL,  // Process security attributes
            NULL,  // Thread security attributes
            FALSE, // Inherit handles
            0,     // Creation flags
            NULL,  // Environment block
            NULL,  // Current directory
            &si,   // Startup info
            &pi)) { // Process information
            std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
            return 1;
        }
#elif defined(DS_PLATFORM_MACOS)

#elif defined(DS_PLATFORM_LINUX)

#endif
        return 0;
    }
}