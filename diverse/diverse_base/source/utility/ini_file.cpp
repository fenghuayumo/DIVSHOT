#include "ini_file.h"
#include "utility/string_utils.h"
#include "utility/file_utils.h"
#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif

#include <fstream>

diverse::IniFile::IniFile(const std::string& filePath)
    : file_path(filePath)
{
    load();
}

void diverse::IniFile::reload()
{
    remove_all();
    load();
}

bool diverse::IniFile::remove(const std::string& key)
{
    if (is_key_existing(key))
    {
        data.erase(key);
        return true;
    }

    return false;
}

void diverse::IniFile::remove_all()
{
    data.clear();
}

bool diverse::IniFile::is_key_existing(const std::string& key) const
{
    return data.find(key) != data.end();
}

void diverse::IniFile::register_pair(const std::string& key, const std::string& value)
{
    register_pair(std::make_pair(key, value));
}

void diverse::IniFile::register_pair(const std::pair<std::string, std::string>& pair)
{
    data.insert(pair);
}

std::vector<std::string> diverse::IniFile::get_formatted_content() const
{
    std::vector<std::string> result;

    for (const auto& [key, value] : data)
        result.push_back(key + "=" + value);

    return result;
}

void diverse::IniFile::load()
{
    if (file_path.empty())
        return;

    std::string fileString;
    diverse::loadText(file_path, fileString);
    auto lines = diverse::stringutility::get_lines(fileString);

    for (auto& line : lines)
    {
        if (is_valid_line(line))
        {
            // line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
            register_pair(extract_key_and_value(line));
        }
    }
}

void diverse::IniFile::rewrite() const
{
    if (file_path.empty())
    {
        DS_LOG_WARN("Ini file path empty");
        return;
    }

    std::stringstream stream;
    for (const auto& [key, value] : data)
        stream << key << "=" << value << std::endl;

    diverse::write_text(file_path, stream.str());
}

std::pair<std::string, std::string> diverse::IniFile::extract_key_and_value(const std::string& p_line) const
{
    std::string key;
    std::string value;

    std::string* currentBuffer = &key;

    for (auto& c : p_line)
    {
        if (c == '=')
            currentBuffer = &value;
        else
            currentBuffer->push_back(c);
    }

    return std::make_pair(key, value);
}

bool diverse::IniFile::is_valid_line(const std::string& attributeLine) const
{
    if (attributeLine.size() == 0)
        return false;

    if (attributeLine[0] == '#' || attributeLine[0] == ';' || attributeLine[0] == '[')
        return false;

    if (std::count(attributeLine.begin(), attributeLine.end(), '=') != 1)
        return false;

    return true;
}

bool diverse::IniFile::string_to_boolean(const std::string& value) const
{
    return (value == "1" || value == "T" || value == "t" || value == "True" || value == "true");
}
