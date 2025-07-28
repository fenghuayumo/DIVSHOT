#pragma once
#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif
#include <vector>

struct PlyElement
{
    uint8_t* data;
    std::vector<size_t> shapes;
};
void write_output_ply(const  std::filesystem::path& file_path, const std::vector<PlyElement>& datas, const std::vector<std::string>& attribute_names);

void read_ply(const  std::filesystem::path& file_path,std::vector<PlyElement>& datas, const std::vector<std::string>& attribute_names);