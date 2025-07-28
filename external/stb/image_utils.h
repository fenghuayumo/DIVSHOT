#pragma once

#include <chrono>
#include <functional>
#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif

enum Stb_Layout
{
	STB_BGR = 0,               // 3-chan, with order specified (for channel flipping)
	STB_1CHANNEL = 1,
	STB_2CHANNEL = 2,
	STB_RGB = 3,               // 3-chan, with order specified (for channel flipping) 
	STB_RGBA = 4,               // alpha formats, alpha is NOT premultiplied into color channels

	STB_4CHANNEL = 5,
	STB_BGRA = 6,
	STB_ARGB = 7,
	STB_ABGR = 8,
	STB_RA = 9,
	STB_AR = 10,

	STB_RGBA_PM = 11,               // alpha formats, alpha is premultiplied into color channels
	STB_BGRA_PM = 12,
	STB_ARGB_PM = 13,
	STB_ABGR_PM = 14,
	STB_RA_PM = 15,
	STB_AR_PM = 16,
};

uint8_t* load_stbi(const std::filesystem::path& path, int* width, int* height, int* comp, int req_comp);
float* load_stbi_float(const std::filesystem::path& path, int* width, int* height, int* comp, int req_comp);
uint16_t* load_stbi_16(const std::filesystem::path& path, int* width, int* height, int* comp, int req_comp);
bool  is_hdr_stbi(const std::filesystem::path& path);
int write_stbi(const std::filesystem::path& path, int width, int height, int comp, const uint8_t* pixels, int quality = 100);

void stb_resize(const uint8_t* pixels, int width, int height, uint8_t* new_pixel, int new_width, int new_height, Stb_Layout pixel_layout);
void stb_resize(const float* pixels, int width, int height, float* new_pixel, int new_width, int new_height, Stb_Layout pixel_layout);


