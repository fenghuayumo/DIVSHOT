#include <string>

#include "core/core.h"
#include "windows_utilities.h"

#include <codecvt>
#include <locale>

namespace diverse
{
    using convert_t = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_t, wchar_t> strconverter;

    std::string WindowsUtilities::WStringToString(std::wstring wstr)
    {
        return strconverter.to_bytes(wstr);
    }

    std::wstring WindowsUtilities::StringToWString(std::string str)
    {
        return strconverter.from_bytes(str);
    }
}