#pragma once

namespace diverse
{
    struct ImageLoadDesc
    {
        const char* filePath;
        uint32_t outWidth;
        uint32_t outHeight;
        uint32_t outBits;
        bool isHDR;
        bool flipY         = false;
        bool srgb          = true;
        uint32_t maxWidth  = 2048;
        uint32_t maxHeight = 2048;
        uint8_t* outPixels;
    };

    DS_EXPORT uint8_t* load_image_from_file(const char* filename, uint32_t* width = nullptr, uint32_t* height = nullptr, uint32_t* bits = nullptr, bool* isHDR = nullptr, bool flipY = false, bool srgb = true);
    DS_EXPORT uint8_t* load_image_from_file(const std::string& filename, uint32_t* width = nullptr, uint32_t* height = nullptr, uint32_t* bits = nullptr, bool* isHDR = nullptr, bool flipY = false, bool srgb = true);

    DS_EXPORT bool load_image_from_file(ImageLoadDesc& desc);

    DS_EXPORT void set_max_image_dimensions(uint32_t width, uint32_t height);

    DS_EXPORT void get_max_image_dimensions(uint32_t& width, uint32_t& height);
}
