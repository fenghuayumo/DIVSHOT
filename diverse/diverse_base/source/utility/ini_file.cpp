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
    : m_FilePath(filePath)
{
    Load();
}

void diverse::IniFile::Reload()
{
    RemoveAll();
    Load();
}

bool diverse::IniFile::Remove(const std::string& key)
{
    if (IsKeyExisting(key))
    {
        m_Data.erase(key);
        return true;
    }

    return false;
}

void diverse::IniFile::RemoveAll()
{
    m_Data.clear();
}

bool diverse::IniFile::IsKeyExisting(const std::string& key) const
{
    return m_Data.find(key) != m_Data.end();
}

void diverse::IniFile::RegisterPair(const std::string& key, const std::string& value)
{
    RegisterPair(std::make_pair(key, value));
}

void diverse::IniFile::RegisterPair(const std::pair<std::string, std::string>& pair)
{
    m_Data.insert(pair);
}

std::vector<std::string> diverse::IniFile::GetFormattedContent() const
{
    std::vector<std::string> result;

    for (const auto& [key, value] : m_Data)
        result.push_back(key + "=" + value);

    return result;
}

void diverse::IniFile::Load()
{
    if (m_FilePath.empty())
        return;

    std::string fileString;
    diverse::loadText(m_FilePath, fileString);
    auto lines = diverse::stringutility::get_lines(fileString);

    for (auto& line : lines)
    {
        if (IsValidLine(line))
        {
            // line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
            RegisterPair(ExtractKeyAndValue(line));
        }
    }
}

void diverse::IniFile::Rewrite() const
{
    if (m_FilePath.empty())
    {
        DS_LOG_WARN("Ini file path empty");
        return;
    }

    std::stringstream stream;
    for (const auto& [key, value] : m_Data)
        stream << key << "=" << value << std::endl;

    diverse::write_text(m_FilePath, stream.str());
}

std::pair<std::string, std::string> diverse::IniFile::ExtractKeyAndValue(const std::string& p_line) const
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

bool diverse::IniFile::IsValidLine(const std::string& attributeLine) const
{
    if (attributeLine.size() == 0)
        return false;

    if (attributeLine[0] == '#' || attributeLine[0] == ';' || attributeLine[0] == '[')
        return false;

    if (std::count(attributeLine.begin(), attributeLine.end(), '=') != 1)
        return false;

    return true;
}

bool diverse::IniFile::StringToBoolean(const std::string& value) const
{
    return (value == "1" || value == "T" || value == "t" || value == "True" || value == "true");
}
