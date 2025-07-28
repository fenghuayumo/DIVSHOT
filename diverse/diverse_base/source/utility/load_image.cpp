#include "core/core.h"
#include "core/profiler.h"
#include "core/ds_log.h"
#include "load_image.h"

#include <filesystem>

#ifdef FREEIMAGE
#include <FreeImage.h>
#include <Utilities.h>
#else
//#define STB_IMAGE_RESIZE_IMPLEMENTATION
#ifdef DS_PLATFORM_LINUX
#define STBI_NO_SIMD
#endif
#include "stb_image.h"
#include "stb_image_resize2.h"
#include <stb/image_utils.h>
#endif

namespace diverse
{

    static uint32_t s_MaxWidth  = 0;
    static uint32_t s_MaxHeight = 0;

    uint8_t* load_image_from_file(const char* filename, uint32_t* width, uint32_t* height, uint32_t* bits, bool* isHDR, bool flipY, bool srgb)
    {
        DS_PROFILE_FUNCTION();
        std::string filePath = std::string(filename);
        if(!std::filesystem::exists(filePath))
            return nullptr;

        filename = filePath.c_str();

        int texWidth = 0, texHeight = 0, texChannels = 0;
        stbi_uc* pixels   = nullptr;
        int sizeOfChannel = 8;
        if(stbi_is_hdr(filename))
        {
            sizeOfChannel = 32;
            pixels        = (uint8_t*)stbi_loadf(filename, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

            if(isHDR)
                *isHDR = true;
        }
        else
        {
            pixels = stbi_load(filename, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

            if(isHDR)
                *isHDR = false;
        }

        // Resize the image if it exceeds the maximum width or height
        if(!isHDR && s_MaxWidth > 0 && s_MaxHeight > 0 && ((uint32_t)texWidth > s_MaxWidth || (uint32_t)texHeight > s_MaxHeight))
        {
            uint32_t texWidthOld = texWidth, texHeightOld = texHeight;
            float aspectRatio = static_cast<float>(texWidth) / static_cast<float>(texHeight);
            if((uint32_t)texWidth > s_MaxWidth)
            {
                texWidth  = s_MaxWidth;
                texHeight = static_cast<uint32_t>(s_MaxWidth / aspectRatio);
            }
            if((uint32_t)texHeight > s_MaxHeight)
            {
                texHeight = s_MaxHeight;
                texWidth  = static_cast<uint32_t>(s_MaxHeight * aspectRatio);
            }

            // Resize the image using stbir
            int resizedChannels    = texChannels;
            uint8_t* resizedPixels = (stbi_uc*)malloc(texWidth * texHeight * resizedChannels);

            if(isHDR)
            {
                stbir_resize_float_linear((float*)pixels, texWidthOld, texHeightOld, 0, (float*)resizedPixels, texWidth, texHeight, 0, STBIR_RGBA);
            }
            else
            {
                stbir_resize_uint8_linear(pixels, texWidthOld, texHeightOld, 0, resizedPixels, texWidth, texHeight, 0, STBIR_RGBA);
            }

            free(pixels); // Free the original image
            pixels = resizedPixels;
        }

        if(!pixels)
        {
            DS_LOG_ERROR("Could not load image '{0}'!", filename);
            // Return magenta checkerboad image

            texChannels = 4;

            if(width)
                *width = 2;
            if(height)
                *height = 2;
            if(bits)
                *bits = texChannels * sizeOfChannel;

            const int32_t size = (*width) * (*height) * texChannels;
            uint8_t* data      = new uint8_t[size];

            uint8_t datatwo[16] = {
                255, 0, 255, 255,
                0, 0, 0, 255,
                0, 0, 0, 255,
                255, 0, 255, 255
            };

            memcpy(data, datatwo, size);

            return data;
        }

        // TODO support different texChannels
        if(texChannels != 4)
            texChannels = 4;

        if(width)
            *width = texWidth;
        if(height)
            *height = texHeight;
        if(bits)
            *bits = texChannels * sizeOfChannel; // texChannels;	  //32 bits for 4 bytes r g b a

        const uint64_t size = uint64_t(texWidth) * uint64_t(texHeight) * uint64_t(texChannels) * uint64_t(sizeOfChannel / 8U);
        uint8_t* result     = new uint8_t[size];
        memcpy(result, pixels, size);

        stbi_image_free(pixels);
        return result;
    }

    uint8_t* load_image_from_file(const std::string& filename, uint32_t* width, uint32_t* height, uint32_t* bits, bool* isHDR, bool flipY, bool srgb)
    {
        return load_image_from_file(filename.c_str(), width, height, bits, isHDR, srgb, flipY);
    }

    bool load_image_from_file(ImageLoadDesc& desc)
    {
        DS_PROFILE_FUNCTION();
        std::string filePath = std::string(desc.filePath);
        stbi_uc* pixels = nullptr;
        int texWidth = 0, texHeight = 0, texChannels = 0;

        int sizeOfChannel = 8;
        if(std::filesystem::exists(filePath))
        {
            desc.filePath = filePath.c_str();

            if(stbi_is_hdr(desc.filePath))
            {
                sizeOfChannel = 32;
                pixels        = (uint8_t*)stbi_loadf(desc.filePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

                desc.isHDR = true;
            }
            else
            {
                pixels = stbi_load(desc.filePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

                desc.isHDR = false;
            }

            // Resize the image if it exceeds the maximum width or height
            if(!desc.isHDR && desc.maxWidth > 0 && desc.maxHeight > 0 && ((uint32_t)texWidth > desc.maxWidth || (uint32_t)texHeight > desc.maxHeight))
            {
                uint32_t texWidthOld = texWidth, texHeightOld = texHeight;
                float aspectRatio = static_cast<float>(texWidth) / static_cast<float>(texHeight);
                if((uint32_t)texWidth > desc.maxWidth)
                {
                    texWidth  = desc.maxWidth;
                    texHeight = static_cast<uint32_t>(desc.maxWidth / aspectRatio);
                }
                if((uint32_t)texHeight > desc.maxHeight)
                {
                    texHeight = desc.maxHeight;
                    texWidth  = static_cast<uint32_t>(desc.maxHeight * aspectRatio);
                }

                if(texChannels != 4)
                    texChannels = 4;

                // Resize the image using stbir
                int resizedChannels    = texChannels;
                uint8_t* resizedPixels = (stbi_uc*)malloc(texWidth * texHeight * resizedChannels);

                if(desc.isHDR)
                {
                    stb_resize((float*)pixels, texWidthOld, texHeightOld, (float*)resizedPixels, texWidth, texHeight, STB_RGBA);
                }
                else
                {
                    stb_resize(pixels, texWidthOld, texHeightOld,  resizedPixels, texWidth, texHeight, texChannels == 4 ? STB_RGBA : STB_RGB);
                }

                stbi_image_free(pixels); // Free the original image
                pixels = resizedPixels;
            }
        }

        if(!pixels)
        {
            DS_LOG_ERROR("Could not load image '{0}'!", desc.filePath);
            // Return magenta checkerboard image

            texChannels = 4;

            desc.outWidth  = 2;
            desc.outHeight = 2;
            desc.outBits   = texChannels * sizeOfChannel;

            const int32_t size = desc.outWidth * desc.outHeight * texChannels;
            uint8_t* data      = new uint8_t[size];

            uint8_t datatwo[16] = {
                255, 0, 255, 255,
                0, 0, 0, 255,
                0, 0, 0, 255,
                255, 0, 255, 255
            };

            memcpy(data, datatwo, size);

            desc.outPixels = data;
            return false;
        }

        // TODO support different texChannels
        if(texChannels != 4)
            texChannels = 4;

        desc.outWidth  = texWidth;
        desc.outHeight = texHeight;
        desc.outBits   = texChannels * sizeOfChannel; // texChannels;	  //32 bits for 4 bytes r g b a

        const uint64_t size = uint64_t(texWidth) * uint64_t(texHeight) * uint64_t(texChannels) * uint64_t(sizeOfChannel / 8U);
        uint8_t* result     = new uint8_t[size];
        memcpy(result, pixels, size);

        stbi_image_free(pixels);
        desc.outPixels = result;
        return true;
    }

    void set_max_image_dimensions(uint32_t width, uint32_t height)
    {
        s_MaxWidth  = width;
        s_MaxHeight = height;
    }

    void get_max_image_dimensions(uint32_t& width, uint32_t& height)
    {
        width  = s_MaxWidth;
        height = s_MaxHeight;
    }
}
