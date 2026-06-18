#include "shader_pak.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_set>

namespace diverse
{
    namespace
    {
        constexpr char kShaderPakMagic[8] = {'D', 'S', 'S', 'H', 'P', 'A', 'K', '1'};
        constexpr uint32_t kShaderPakVersion = 1;

        template <typename T>
        bool read_pod(std::istream& stream, T& value)
        {
            stream.read(reinterpret_cast<char*>(&value), sizeof(T));
            return static_cast<bool>(stream);
        }

        template <typename T>
        bool write_pod(std::ostream& stream, const T& value)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
            return static_cast<bool>(stream);
        }

        struct PendingShaderPakEntry
        {
            std::filesystem::path source_path;
            std::string key;
            uint64_t offset = 0;
            uint64_t size = 0;
        };

        uint64_t index_record_size(const std::string& key)
        {
            return sizeof(uint32_t) + key.size() + sizeof(uint64_t) + sizeof(uint64_t);
        }
    }

    std::string normalize_shader_pak_key(std::string shader_name)
    {
        std::replace(shader_name.begin(), shader_name.end(), '\\', '/');
        while (!shader_name.empty() && shader_name.front() == '/')
            shader_name.erase(shader_name.begin());
        return shader_name;
    }

    bool ShaderPak::load(const std::filesystem::path& pak_path)
    {
        loaded = false;
        entries.clear();
        pak_file = pak_path;

        std::ifstream input(pak_path, std::ios::binary);
        if (!input)
            return false;

        char magic[8] = {};
        input.read(magic, sizeof(magic));
        if (!input || !std::equal(std::begin(magic), std::end(magic), std::begin(kShaderPakMagic)))
        {
            std::cerr << "Invalid shader pak magic: " << pak_path.string() << std::endl;
            return false;
        }

        uint32_t version = 0;
        uint32_t entry_count = 0;
        if (!read_pod(input, version) || !read_pod(input, entry_count))
            return false;

        if (version != kShaderPakVersion)
        {
            std::cerr << "Unsupported shader pak version " << version << ": " << pak_path.string() << std::endl;
            return false;
        }

        for (uint32_t i = 0; i < entry_count; ++i)
        {
            uint32_t name_size = 0;
            ShaderPakEntry entry;
            if (!read_pod(input, name_size))
                return false;

            std::string key(name_size, '\0');
            input.read(key.data(), name_size);
            if (!input || !read_pod(input, entry.offset) || !read_pod(input, entry.size))
                return false;

            entries[normalize_shader_pak_key(std::move(key))] = entry;
        }

        loaded = true;
        return true;
    }

    bool ShaderPak::read(const std::string& shader_name, std::vector<uint8_t>& data) const
    {
        if (!loaded)
            return false;

        const auto key = normalize_shader_pak_key(shader_name);
        const auto it = entries.find(key);
        if (it == entries.end())
            return false;

        std::ifstream input(pak_file, std::ios::binary);
        if (!input)
            return false;

        const auto& entry = it->second;
        data.resize(static_cast<size_t>(entry.size));
        input.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
        input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(entry.size));
        return static_cast<bool>(input);
    }

    bool build_shader_pak_from_cache(
        const std::filesystem::path& cache_dir,
        const std::filesystem::path& out_pak,
        const std::vector<std::string>& required_keys,
        const std::vector<std::string>& optional_keys)
    {
        if (!std::filesystem::exists(cache_dir) || !std::filesystem::is_directory(cache_dir))
        {
            std::cerr << "Shader cache directory does not exist: " << cache_dir.string() << std::endl;
            return false;
        }

        std::unordered_set<std::string> required_set;
        for (auto key : required_keys)
        {
            key = normalize_shader_pak_key(std::move(key));
            if (!key.empty())
                required_set.insert(std::move(key));
        }

        std::unordered_set<std::string> optional_set;
        for (auto key : optional_keys)
        {
            key = normalize_shader_pak_key(std::move(key));
            if (!key.empty())
                optional_set.insert(std::move(key));
        }

        std::vector<PendingShaderPakEntry> pending_entries;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(cache_dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".cached")
                continue;

            auto key = normalize_shader_pak_key(std::filesystem::relative(entry.path(), cache_dir).string());
            const bool has_manifest = !required_set.empty() || !optional_set.empty();
            if (has_manifest && required_set.find(key) == required_set.end() && optional_set.find(key) == optional_set.end())
                continue;
            pending_entries.push_back({entry.path(), std::move(key), 0, entry.file_size()});
        }

        std::sort(pending_entries.begin(), pending_entries.end(), [](const auto& a, const auto& b) {
            return a.key < b.key;
        });

        uint64_t index_size = 0;
        for (const auto& entry : pending_entries)
            index_size += index_record_size(entry.key);

        uint64_t data_offset = sizeof(kShaderPakMagic) + sizeof(uint32_t) + sizeof(uint32_t) + index_size;
        for (auto& entry : pending_entries)
        {
            entry.offset = data_offset;
            data_offset += entry.size;
            required_set.erase(entry.key);
            optional_set.erase(entry.key);
        }

        for (const auto& missing_key : optional_set)
            std::cerr << "Optional cached shader is not present: " << missing_key << std::endl;

        if (!required_set.empty())
        {
            for (const auto& missing_key : required_set)
                std::cerr << "Missing cached shader for pak manifest entry: " << missing_key << std::endl;
            return false;
        }

        if (!out_pak.parent_path().empty())
            std::filesystem::create_directories(out_pak.parent_path());

        std::ofstream output(out_pak, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            std::cerr << "Failed to open shader pak for writing: " << out_pak.string() << std::endl;
            return false;
        }

        output.write(kShaderPakMagic, sizeof(kShaderPakMagic));
        const uint32_t version = kShaderPakVersion;
        const uint32_t entry_count = static_cast<uint32_t>(pending_entries.size());
        if (!write_pod(output, version) || !write_pod(output, entry_count))
            return false;

        for (const auto& entry : pending_entries)
        {
            const uint32_t name_size = static_cast<uint32_t>(entry.key.size());
            if (!write_pod(output, name_size))
                return false;
            output.write(entry.key.data(), static_cast<std::streamsize>(entry.key.size()));
            if (!write_pod(output, entry.offset) || !write_pod(output, entry.size))
                return false;
        }

        std::vector<char> buffer(1024 * 1024);
        for (const auto& entry : pending_entries)
        {
            std::ifstream input(entry.source_path, std::ios::binary);
            if (!input)
            {
                std::cerr << "Failed to open cached shader: " << entry.source_path.string() << std::endl;
                return false;
            }

            while (input)
            {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const auto read_size = input.gcount();
                if (read_size > 0)
                    output.write(buffer.data(), read_size);
            }
        }

        std::cout << "Wrote " << pending_entries.size() << " shaders to " << out_pak.string() << std::endl;
        return static_cast<bool>(output);
    }
}
