#include "embed_resources.h"
#include <filesystem>
#include <utility/file_utils.h>
#include <utility/string_utils.h>
#include <core/ds_log.h>


namespace diverse
{
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
