#include "Language.h"
#include <fstream>
#include <nlohmann/json.hpp>  // 项目本身已经带了 nlohmann/json

std::unordered_map<std::string, std::string> Language::strings;
std::string Language::current = "en";

void Language::Load(const std::string& lang) {
    current = lang;
    std::ifstream f("locales/" + lang + ".json");
    if (!f.is_open()) { current = "en"; f.open("locales/en.json"); }
    nlohmann::json j;
    f >> j;
    for (auto& [k, v] : j.items()) strings[k] = v.get<std::string>();
}

std::string Language::Get(const std::string& key) {
    auto it = strings.find(key);
    return it != strings.end() ? it->second : key;
}