#pragma once

#include <string>
#include <utility/file_utils.h>

namespace diverse
{
    inline std::string get_html_view_template()
    {
        std::string text;
        if(!diverse::loadText("3dgs_html_template.html", text))
            return std::string();
        return text;
    }
}
