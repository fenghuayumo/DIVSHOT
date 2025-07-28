#pragma once
#include "assets/asset.h"
#include "assets/texture.h"
#include "core/reference.h"
#include <string>

namespace diverse
{
    struct MSDFData;
    class Texture;

    class Font : public Asset
    {
        class FontHolder;

    public:
        Font(const std::string& filepath);
        Font(uint8_t* data, uint32_t dataSize, const std::string& name);

        virtual ~Font();

        SharedPtr<asset::Texture> get_font_atlas() const { return texture_atlas; }
        const MSDFData* get_msdf_data() const { return msdf_data; }
        const std::string& get_file_path() const { return file_path; }

        void init();

        static void init_default_font();
        static void shutdown_default_font();
        static SharedPtr<Font> get_default_font();

        SET_ASSET_TYPE(AssetType::Font);

    private:
        std::string file_path;
        SharedPtr<asset::Texture> texture_atlas;
        MSDFData* msdf_data = nullptr;
        uint8_t* font_data;
        uint32_t font_data_size;

    private:
        static SharedPtr<Font> s_DefaultFont;
    };
}
