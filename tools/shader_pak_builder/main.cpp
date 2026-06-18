#include "utility/shader_pak.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    struct Manifest
    {
        std::vector<std::string> required;
        std::vector<std::string> optional;
    };

    Manifest read_manifest(const std::filesystem::path& manifest_path)
    {
        std::ifstream input(manifest_path);
        Manifest manifest;
        std::string line;
        while (std::getline(input, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty() || line.front() == '#')
                continue;
            if (line.front() == '?')
            {
                line.erase(line.begin());
                manifest.optional.push_back(line);
            }
            else
            {
                manifest.required.push_back(line);
            }
        }
        return manifest;
    }
}

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage: shader_pak_builder <shader_cache_dir> <out_shaders.pak> [manifest.txt]" << std::endl;
        return 1;
    }

    const std::filesystem::path cache_dir = argv[1];
    const std::filesystem::path out_pak = argv[2];
    const auto manifest = argc == 4 ? read_manifest(argv[3]) : Manifest{};

    return diverse::build_shader_pak_from_cache(cache_dir, out_pak, manifest.required, manifest.optional) ? 0 : 1;
}
