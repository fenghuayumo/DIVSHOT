#pragma once

namespace diverse
{
    class DS_EXPORT WindowsUtilities
    {
    public:
        static std::string WStringToString(std::wstring wstr);
        static std::wstring StringToWString(std::string str);
    };
}
