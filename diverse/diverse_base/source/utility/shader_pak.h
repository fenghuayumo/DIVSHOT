#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace diverse
{
    struct ShaderPakEntry
    {
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    class ShaderPak
    {
    public:
        bool load(const std::filesystem::path& pak_path);
        bool read(const std::string& shader_name, std::vector<uint8_t>& data) const;
        bool is_loaded() const { return loaded; }
        const std::filesystem::path& path() const { return pak_file; }

    private:
        std::filesystem::path pak_file;
        std::unordered_map<std::string, ShaderPakEntry> entries;
        bool loaded = false;
    };

    std::string normalize_shader_pak_key(std::string shader_name);
    bool build_shader_pak_from_cache(
        const std::filesystem::path& cache_dir,
        const std::filesystem::path& out_pak,
        const std::vector<std::string>& required_keys = {},
        const std::vector<std::string>& optional_keys = {});
}
