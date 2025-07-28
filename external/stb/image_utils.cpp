
#include "image_utils.h"


#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize2.h"
#include <fstream>


inline bool equals_case_insensitive(std::string str1, std::string str2) 
{
	
	std::transform(std::begin(str1), std::end(str1), std::begin(str1), [](unsigned char c) { return (char)std::tolower(c); });
	std::transform(std::begin(str2), std::end(str2), std::begin(str2), [](unsigned char c) { return (char)std::tolower(c); });
	return str1 == str2;
}

static const stbi_io_callbacks istream_stbi_callbacks = {
	// Read
	[](void* context, char* data, int size) {
		auto stream = reinterpret_cast<std::istream*>(context);
		stream->read(data, size);
		return (int)stream->gcount();
	},
	// Seek
	[](void* context, int size) {
		reinterpret_cast<std::istream*>(context)->seekg(size, std::ios_base::cur);
	},
	// EOF
	[](void* context) {
		return (int)!!(*reinterpret_cast<std::istream*>(context));
	},
};

void istream_stbi_write_func(void* context, void* data, int size) {
	reinterpret_cast<std::ostream*>(context)->write(reinterpret_cast<char*>(data), size);
}

uint8_t* load_stbi(const std::filesystem::path& path, int* width, int* height, int* comp, int req_comp) {
	std::ifstream f{path.string(), std::ios::in | std::ios::binary};
	return stbi_load_from_callbacks(&istream_stbi_callbacks, &f, width, height, comp, req_comp);
}

float* load_stbi_float(const std::filesystem::path& path, int* width, int* height, int* comp, int req_comp) {
	std::ifstream f{path.string(), std::ios::in | std::ios::binary};
	return stbi_loadf_from_callbacks(&istream_stbi_callbacks, &f, width, height, comp, req_comp);
}

uint16_t* load_stbi_16(const std::filesystem::path& path, int* width, int* height, int* comp, int req_comp) {
	std::ifstream f{path.string(), std::ios::in | std::ios::binary};
	return stbi_load_16_from_callbacks(&istream_stbi_callbacks, &f, width, height, comp, req_comp);
}

bool is_hdr_stbi(const std::filesystem::path& path) {
	std::ifstream f{path.string(), std::ios::in | std::ios::binary};
	return stbi_is_hdr_from_callbacks(&istream_stbi_callbacks, &f);
}

int write_stbi(const std::filesystem::path& path, int width, int height, int comp, const uint8_t* pixels, int quality) {
	std::ofstream f{path.string(), std::ios::out | std::ios::binary};

	if (equals_case_insensitive(path.extension().string(), ".jpg") || equals_case_insensitive(path.extension().string(), ".jpeg")) {
		return stbi_write_jpg_to_func(istream_stbi_write_func, &f, width, height, comp, pixels, quality);
	} else if (equals_case_insensitive(path.extension().string(), ".png")) {
		return stbi_write_png_to_func(istream_stbi_write_func, &f, width, height, comp, pixels, width * comp);
	} else if (equals_case_insensitive(path.extension().string(), ".tga")) {
		return stbi_write_tga_to_func(istream_stbi_write_func, &f, width, height, comp, pixels);
	} else if (equals_case_insensitive(path.extension().string(), ".bmp")) {
		return stbi_write_bmp_to_func(istream_stbi_write_func, &f, width, height, comp, pixels);
	} else {
		//throw std::runtime_error{fmt::format("write_stbi: unknown image extension '{}'", path.extension())};
		return -1;
	}
}

void stb_resize(const uint8_t* pixels, int width, int height, uint8_t* new_pixel, int new_width, int new_height, Stb_Layout pixel_layout) {
	stbir_resize_uint8_linear(pixels, width, height, 0, new_pixel, new_width, new_height, 0, (stbir_pixel_layout)pixel_layout);
}

void stb_resize(const float* pixels, int width, int height, float* new_pixel, int new_width, int new_height, Stb_Layout pixel_layout)
{
	stbir_resize_float_linear(pixels, width, height, 0, new_pixel, new_width, new_height, 0, (stbir_pixel_layout)pixel_layout);
}
