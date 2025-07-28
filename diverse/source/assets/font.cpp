#include "font.h"
#include "msdf_data.h"
#include "engine/file_system.h"
#include "backend/drs_rhi/gpu_buffer.h"
#include "backend/drs_rhi/gpu_texture.h"
#include "backend/drs_rhi/gpu_device.h"
#include "engine/file_system.h"
#include "engine/application.h"
#include "core/profiler.h"
#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif

#include <imgui/Plugins/ImGuiAl/fonts/RobotoRegular.inl>
#include <stb/deprecated/stb.h>
#include <spdlog/fmt/bundled/format.h>

#define FONT_DEBUG_LOG 0
#if FONT_DEBUG_LOG
#define FONT_LOG(...) DS_LOG_INFO("Font", __VA_ARGS__)
#else
#define FONT_LOG(...) ((void)0)
#endif

using namespace msdf_atlas;

namespace diverse
{

    struct FontInput
    {
        const char* fontFilename;
        GlyphIdentifierType glyphIdentifierType;
        const char* charsetFilename;
        double fontScale;
        const char* fontName;
        uint32_t* data;
        uint32_t dataSize;
    };

    struct Configuration
    {
        ImageType imageType;
        msdf_atlas::ImageFormat imageFormat;
        YDirection yDirection;
        int width, height;
        double emSize;
        double pxRange;
        double angleThreshold;
        double miterLimit;
        void (*edgeColoring)(msdfgen::Shape&, double, unsigned long long);
        bool expensiveColoring;
        unsigned long long coloringSeed;
        GeneratorAttributes generatorAttributes;
    };

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define DEFAULT_MITER_LIMIT 1.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull
#define THREADS 8

    static std::filesystem::path GetCacheDirectory()
    {
        return "Resources/Cache/FontAtlases";
    }

    static void CreateCacheDirectoryIfNeeded()
    {
        std::filesystem::path cacheDirectory = GetCacheDirectory();
        if (!std::filesystem::exists(cacheDirectory))
            std::filesystem::create_directories(cacheDirectory);
    }

    struct AtlasHeader
    {
        uint32_t Type = 0;
        uint32_t Width, Height;
    };

    static std::shared_ptr<rhi::GpuBuffer> TryReadFontAtlasFromCache(const std::string& fontName, float fontSize, AtlasHeader& header, void*& pixels)
    {
        DS_PROFILE_FUNCTION();
        std::string filename = fmt::format("{0}-{1}.lfa", fontName, fontSize);
        std::filesystem::path filepath = GetCacheDirectory() / filename;
        if (std::filesystem::exists(filepath))
        {
            auto device = get_global_device();
            auto desc = rhi::GpuBufferDesc::new_cpu_to_gpu(uint32_t(FileSystem::get_file_size(filepath.string())), rhi::BufferUsageFlags::STORAGE_BUFFER);
            auto storageBuffer = g_device->create_buffer(desc, "font_storage", FileSystem::read_file(filepath.string()));
            auto data = storageBuffer->map(device);
			header = *(AtlasHeader*)data;
			pixels = (uint8_t*)data + sizeof(AtlasHeader);
            storageBuffer->unmap(device);
            return storageBuffer;
        }
        return {};
    }

    static void CacheFontAtlas(const std::string& fontName, float fontSize, AtlasHeader header, const void* pixels)
    {
        DS_PROFILE_FUNCTION();
        CreateCacheDirectoryIfNeeded();

        std::string filename = fmt::format("{0}-{1}.lfa", fontName, fontSize);
        std::filesystem::path filepath = GetCacheDirectory() / filename;

        std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            stream.close();
            DS_LOG_ERROR("Failed to cache font atlas to {0}", filepath.string());
            return;
        }

        stream.write((char*)&header, sizeof(AtlasHeader));
        stream.write((char*)pixels, header.Width * header.Height * sizeof(float) * 4);
    }

    static SharedPtr<asset::Texture> CreateCachedAtlas(AtlasHeader header, void* pixels)
    {
        DS_PROFILE_FUNCTION();
        std::vector<u8> data(header.Width * header.Height * 4 * sizeof(float));
        memcpy(data.data(), pixels, data.size());
        asset::RawImage img = {PixelFormat::R32G32B32A32_Float , {header.Width, header.Height}, data };
        return createSharedPtr<asset::Texture>(img);
    }

    template <typename T, typename S, int N, GeneratorFunction<S, N> GEN_FN>
    static SharedPtr<asset::Texture> CreateAndCacheAtlas(const std::string& fontName, float fontSize, const std::vector<GlyphGeometry>& glyphs, const FontGeometry& fontGeometry, const Configuration& config)
    {
        DS_PROFILE_FUNCTION();
        ImmediateAtlasGenerator<S, N, GEN_FN, BitmapAtlasStorage<T, N>> generator(config.width, config.height);
        generator.setAttributes(config.generatorAttributes);
        generator.setThreadCount(THREADS);
        generator.generate(glyphs.data(), int(glyphs.size()));

        msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>)generator.atlasStorage();

        AtlasHeader header;
        header.Width = bitmap.width;
        header.Height = bitmap.height;
        CacheFontAtlas(fontName, fontSize, header, bitmap.pixels);

        return CreateCachedAtlas(header, (void*)bitmap.pixels);
    }

    Font::Font(uint8_t* data, uint32_t dataSize, const std::string& name)
        : font_data(data)
        , font_data_size(dataSize)
        , msdf_data(new MSDFData())
        , file_path(name)
    {
        init();
    }

    Font::Font(const std::string& filepath)
        : file_path(filepath)
        , msdf_data(new MSDFData())
        , font_data(nullptr)
        , font_data_size(0)
    {
        init();
    }

    Font::~Font()
    {
        delete msdf_data;
    }

    void Font::init()
    {
        DS_PROFILE_FUNCTION();
        FontInput fontInput = {};
        fontInput.glyphIdentifierType = GlyphIdentifierType::UNICODE_CODEPOINT;
        fontInput.fontScale = -1;

        Configuration config = {};
        config.imageType = ImageType::MSDF;
        config.imageFormat = msdf_atlas::ImageFormat::BINARY_FLOAT;
        config.yDirection = YDirection::BOTTOM_UP;
        config.edgeColoring = msdfgen::edgeColoringInkTrap;
        config.generatorAttributes.config.overlapSupport = true;
        config.generatorAttributes.scanlinePass = true;
        config.angleThreshold = DEFAULT_ANGLE_THRESHOLD;
        config.miterLimit = DEFAULT_MITER_LIMIT;
        config.imageType = ImageType::MTSDF;
        config.emSize = 40;

        const char* imageFormatName = nullptr;
        int fixedWidth = -1;
        int fixedHeight = -1;
        double minEmSize = 0;
        double rangeValue = 2.0;
        TightAtlasPacker::DimensionsConstraint atlasSizeConstraint = TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE;

        // Load fonts
        bool anyCodepointsAvailable = false;
        class FontHolder
        {
            msdfgen::FreetypeHandle* ft;
            msdfgen::FontHandle* font;
            const char* fontFilename;

        public:
            FontHolder()
                : ft(msdfgen::initializeFreetype())
                , font(nullptr)
                , fontFilename(nullptr)
            {
            }
            ~FontHolder()
            {
                if (ft)
                {
                    if (font)
                        msdfgen::destroyFont(font);
                    msdfgen::deinitializeFreetype(ft);
                }
            }
            bool load(const char* fontFilename)
            {
                if (ft && fontFilename)
                {
                    if (this->fontFilename && !strcmp(this->fontFilename, fontFilename))
                        return true;
                    if (font)
                        msdfgen::destroyFont(font);
                    if ((font = msdfgen::loadFont(ft, fontFilename)))
                    {
                        this->fontFilename = fontFilename;
                        return true;
                    }
                    this->fontFilename = nullptr;
                }
                return false;
            }
            bool load(uint8_t* data, uint32_t dataSize)
            {
                if (ft && dataSize > 0)
                {
                    if (font)
                        msdfgen::destroyFont(font);
                    if ((font = msdfgen::loadFontData(ft, (const unsigned char*)data, int(dataSize))))
                    {
                        return true;
                    }
                    this->fontFilename = nullptr;
                }
                return false;
            }
            operator msdfgen::FontHandle* () const
            {
                return font;
            }
        } font;

        if (font_data_size == 0)
        {
            std::string outPath;
            if (!FileSystem::get().resolve_physical_path(file_path, outPath))
                return;

            FONT_LOG("Font: Loading Font {0}", file_path);
            fontInput.fontFilename = outPath.c_str();
            file_path = outPath;

            if (!font.load(fontInput.fontFilename))
            {
                FONT_LOG("Font: Failed to load font! - {0}", fontInput.fontFilename);
                return;
            }
        }
        else
        {
            if (!font.load(font_data, font_data_size))
            {
                FONT_LOG("Font: Failed to load font from data!");
                return;
            }
        }

        if (fontInput.fontScale <= 0)
            fontInput.fontScale = 1;

        // Load character set
        fontInput.glyphIdentifierType = GlyphIdentifierType::UNICODE_CODEPOINT;
        Charset charset;

        // From ImGui
        static const uint32_t charsetRanges[] = {
            0x0020,
            0x00FF, // Basic Latin + Latin Supplement
            0x0400,
            0x052F, // Cyrillic + Cyrillic Supplement
            0x2DE0,
            0x2DFF, // Cyrillic Extended-A
            0xA640,
            0xA69F, // Cyrillic Extended-B
            0,
        };

        for (int range = 0; range < 8; range += 2)
        {
            for (uint32_t c = charsetRanges[range]; c <= charsetRanges[range + 1]; c++)
                charset.add(c);
        }

        // Load glyphs
        msdf_data->FontGeometry = FontGeometry(&msdf_data->Glyphs);
        int glyphsLoaded = -1;
        switch (fontInput.glyphIdentifierType)
        {
        case GlyphIdentifierType::GLYPH_INDEX:
            glyphsLoaded = msdf_data->FontGeometry.loadGlyphset(font, fontInput.fontScale, charset);
            break;
        case GlyphIdentifierType::UNICODE_CODEPOINT:
            glyphsLoaded = msdf_data->FontGeometry.loadCharset(font, fontInput.fontScale, charset);
            anyCodepointsAvailable |= glyphsLoaded > 0;
            break;
        }

        DS_ASSERT(glyphsLoaded >= 0);
        FONT_LOG("Font: Loaded geometry of {0} out of {1} glyphs", glyphsLoaded, (int)charset.size());
        // List missing glyphs
        if (glyphsLoaded < (int)charset.size())
        {
            FONT_LOG("Font: Missing {0} {1}", (int)charset.size() - glyphsLoaded, fontInput.glyphIdentifierType == GlyphIdentifierType::UNICODE_CODEPOINT ? "codepoints" : "glyphs");
        }

        if (fontInput.fontName)
            msdf_data->FontGeometry.setName(fontInput.fontName);

        // Determine final atlas dimensions, scale and range, pack glyphs
        double pxRange = rangeValue;
        bool fixedDimensions = fixedWidth >= 0 && fixedHeight >= 0;
        bool fixedScale = config.emSize > 0;
        TightAtlasPacker atlasPacker;
        if (fixedDimensions)
            atlasPacker.setDimensions(fixedWidth, fixedHeight);
        else
            atlasPacker.setDimensionsConstraint(atlasSizeConstraint);
        atlasPacker.setPadding(config.imageType == ImageType::MSDF || config.imageType == ImageType::MTSDF ? 0 : -1);

        if (fixedScale)
            atlasPacker.setScale(config.emSize);
        else
            atlasPacker.setMinimumScale(minEmSize);
        atlasPacker.setPixelRange(pxRange);
        atlasPacker.setMiterLimit(config.miterLimit);
        if (int remaining = atlasPacker.pack(msdf_data->Glyphs.data(), int(msdf_data->Glyphs.size())))
        {
            DS_LOG_ERROR("Font: Could not fit {0} out of {1} glyphs into the atlas.", remaining, (int)msdf_data->Glyphs.size());
        }
        atlasPacker.getDimensions(config.width, config.height);
        DS_ASSERT(config.width > 0 && config.height > 0);
        config.emSize = atlasPacker.getScale();
        config.pxRange = atlasPacker.getPixelRange();
        if (!fixedScale)
            FONT_LOG("Font: Glyph size: {0} pixels/EM", config.emSize);
        if (!fixedDimensions)
            FONT_LOG("Font: Atlas dimensions: {0} x {1}", config.width, config.height);

        // Edge coloring
        if (config.imageType == ImageType::MSDF || config.imageType == ImageType::MTSDF)
        {
            if (config.expensiveColoring)
            {
                Workload([&glyphs = msdf_data->Glyphs, &config](int i, int threadNo) -> bool
                    {
                        unsigned long long glyphSeed = (LCG_MULTIPLIER * (config.coloringSeed ^ i) + LCG_INCREMENT) * !!config.coloringSeed;
                        glyphs[i].edgeColoring(config.edgeColoring, config.angleThreshold, glyphSeed);
                        return true; },
                    int(msdf_data->Glyphs.size()))
                    .finish(THREADS);
            }
            else
            {
                unsigned long long glyphSeed = config.coloringSeed;
                for (GlyphGeometry& glyph : msdf_data->Glyphs)
                {
                    glyphSeed *= LCG_MULTIPLIER;
                    glyph.edgeColoring(config.edgeColoring, config.angleThreshold, glyphSeed);
                }
            }
        }

        std::string fontName = file_path;

        // Check cache here
        AtlasHeader header;
        void* pixels;
        if (auto storageBuffer = TryReadFontAtlasFromCache(fontName, (float)config.emSize, header, pixels))
        {
            texture_atlas = CreateCachedAtlas(header, pixels);
        }
        else
        {
            bool floatingPointFormat = true;
            SharedPtr<asset::Texture> texture;
            switch (config.imageType)
            {
            case ImageType::MSDF:
                if (floatingPointFormat)
                    texture = CreateAndCacheAtlas<float, float, 3, msdfGenerator>(fontName, (float)config.emSize, msdf_data->Glyphs, msdf_data->FontGeometry, config);
                else
                    texture = CreateAndCacheAtlas<byte, float, 3, msdfGenerator>(fontName, (float)config.emSize, msdf_data->Glyphs, msdf_data->FontGeometry, config);
                break;
            case ImageType::MTSDF:
                if (floatingPointFormat)
                    texture = CreateAndCacheAtlas<float, float, 4, mtsdfGenerator>(fontName, (float)config.emSize, msdf_data->Glyphs, msdf_data->FontGeometry, config);
                else
                    texture = CreateAndCacheAtlas<byte, float, 4, mtsdfGenerator>(fontName, (float)config.emSize, msdf_data->Glyphs, msdf_data->FontGeometry, config);
                break;
            }

            texture_atlas = texture;
        }
    }

    SharedPtr<Font> Font::s_DefaultFont;

    void Font::init_default_font()
    {
        const unsigned int buf_decompressed_size = stb_decompress_length((unsigned char*)RobotoRegular_compressed_data);
        unsigned char* buf_decompressed_data = (unsigned char*)malloc(buf_decompressed_size);
        stb_decompress(buf_decompressed_data, (unsigned char*)RobotoRegular_compressed_data, (unsigned int)RobotoRegular_compressed_size);

        s_DefaultFont = createSharedPtr<Font>((uint8_t*)buf_decompressed_data, uint32_t(buf_decompressed_size), "RobotoRegular");
        free(buf_decompressed_data);
    }

    void Font::shutdown_default_font()
    {
        s_DefaultFont.reset();
    }

    SharedPtr<Font> Font::get_default_font()
    {
        return s_DefaultFont;
    }
}
