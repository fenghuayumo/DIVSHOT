#pragma once
#include <string>
#include <unordered_map>

class Language {
public:
    static std::unordered_map<std::string, std::string> strings;
    static std::string current;  // "en" "zh-CN" "ja" 等

    static void Load(const std::string& lang);
    static std::string Get(const std::string& key);
}