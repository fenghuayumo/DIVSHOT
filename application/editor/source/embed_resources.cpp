#include "embed_resources.h"
#include <filesystem>
#include <utility/file_utils.h>
#include <utility/string_utils.h>
#include <core/ds_log.h>


namespace diverse
{
    void dump_subfolder_shaders(const std::string& path, const std::string& spv_Path)
    {
        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
                auto is_spv = ".cached" == ext;
                std::vector<u8> spirv;
                size_t data_size = 0;
                auto shader_cache_name = std::filesystem::relative(entry.path(), spv_Path).string();
                if (readByteData(entry.path().string(), spirv, data_size) && data_size > 0)
                {
                    dump_shader_with_append(shader_cache_name, "../../diverse/source/assets/embeded/embeded_shaders.hpp", spirv);
                }
                else
                {
                    DS_LOG_ERROR("loading shader {} error", shader_cache_name);
                }
            }
            else
            {
                dump_subfolder_shaders(entry.path().string(),spv_Path);
            }
        }
    }
    void embeded_editor_shaders()
    {
#ifdef DS_PLATFORM_MACOS
        auto spv_path = getExecutablePath() + "/cache/";
#else
        auto spv_path = getInstallDirectory() + "/cache/";
#endif
        auto shader_embeded_shader = "../../diverse/source/assets/embeded/embeded_shaders.hpp";
        if (std::filesystem::exists(shader_embeded_shader))
            std::filesystem::remove_all(shader_embeded_shader);
        dump_subfolder_shaders(spv_path, spv_path);
        DS_LOG_INFO("end generating embed shaders resource!");
    }

    void embed_editor_textures()
    {
        DS_LOG_INFO("begin generating embed resource");
        /*     const std::string resouce_path = "../../resource/";
             embed_texture(resouce_path + "images/bluenoise/256_256/LDR_RGBA_0.png", "../diverse/source/assets/embeded/bluenoise_256_256.inl", "bluenoise_256_256");
         */
         auto& embeded_asset_textures = get_embeded_asset_textures();
        for (const auto& [texture_path, texture] : embeded_asset_textures)
        {
            auto textureName = std::filesystem::path(texture_path).filename().string();
            textureName = stringutility::remove_file_extension(textureName);
            std::replace(textureName.begin(), textureName.end(), '-', '_');
            embed_texture(std::string(texture_path), "../../diverse/source/assets/embeded/" + textureName + ".inl", textureName);
        }
        DS_LOG_INFO("end generating embed resource!");
    }
}