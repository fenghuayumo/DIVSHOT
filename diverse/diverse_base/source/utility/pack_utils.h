#pragma once
#include <glm/glm.hpp>
namespace diverse
{
    inline auto f32_to_u32(f32 v,f32 scale) ->u32 {
        return static_cast<u32>(std::floor((v * scale + 0.5)));
    }

    inline auto float3_to_r8g8b8_unorm(f32 unpacked_inputX,
        f32 unpacked_inputY,
        f32 unpacked_inputZ)->u32 {
        auto code = f32_to_u32(glm::clamp(unpacked_inputX,0.0f, 1.0f), 255.0f) |
            f32_to_u32(glm::clamp(unpacked_inputY,0.0f, 1.0f), 255.0f) << 8 |
            f32_to_u32(glm::clamp(unpacked_inputZ,0.0f, 1.0f), 255.0f) << 16;
        return code;
    }

    inline auto pack_color_8888(const glm::vec4& v)->u32 {
        auto code = f32_to_u32(glm::clamp(v.x,0.0f, 1.0f), 255.0f) |
            f32_to_u32(glm::clamp(v.y,0.0f, 1.0f), 255.0f) << 8 |
            f32_to_u32(glm::clamp(v.z,0.0f, 1.0f), 255.0f) << 16 |
            f32_to_u32(glm::clamp(v.w,0.0f, 1.0f), 255.0f) << 24;
        return code;
    }
    
    inline float unpack_unorm(u32 pckd, u32 bitCount) 
    {
        u32 maxVal = (1u << bitCount) - 1;
        return float(pckd & maxVal) / maxVal;
    }

    inline u32 pack_unorm(float val, u32 bitCount) 
    {
        uint maxVal = (1u << bitCount) - 1;
        return u32(glm::clamp<f32>(val, 0.0, 1.0) * maxVal + 0.5);
    }

    inline u32 pack_normal_11_10_11(const glm::vec3& n)
    {
        u32 pckd = 0;
        pckd += pack_unorm(n.x * 0.5 + 0.5, 11);
        pckd += pack_unorm(n.y * 0.5 + 0.5, 10) << 11;
        pckd += pack_unorm(n.z * 0.5 + 0.5, 11) << 21;
        return pckd;
    }

    inline glm::vec3 unpack_normal_11_10_11(u32 pckd) {
        uint p = pckd;
        return glm::normalize(glm::vec3(
            unpack_unorm(p, 11),
            unpack_unorm(p >> 11, 10),
            unpack_unorm(p >> 21, 11)
        ) * 2.0f - 1.0f);
    }

    inline auto pack_unit_direction_11_10_11(const glm::vec3& xyz) -> u32
    {
        f32 f1 = ((1 << 11) - 1);
        f32 f2 = ((1 << 10) - 1);
        f32 f3 = ((1 << 11) - 1);

        u32 x = (xyz.x * 0.5 + 0.5) * f1;
        u32 y = (xyz.y * 0.5 + 0.5) * f2;
        u32 z = (xyz.z * 0.5 + 0.5) * f3;

        return (z << 21) | (y << 11) | x;
    }
    inline auto unpack_direction_11_10_11(u32 pckd) -> glm::vec3 {
        return glm::vec3(
            unpack_unorm(pckd, 11),
            unpack_unorm(pckd >> 11, 10),
            unpack_unorm(pckd >> 21, 11)
        ) * 2.0f - 1.0f;
    }
    inline float u32_to_f32(u32 v) 
    {
        // Multiplying by 2^-112 causes exponents below -14 to denormalize
        union FU {
            u32 ui;
            float f;
        }; // 2**-112
        FU BiasedFloat;
        BiasedFloat.ui = v ;
        return BiasedFloat.f;
    }
    inline u16 f32_to_f16(f32 v)
    {
        // Multiplying by 2^-112 causes exponents below -14 to denormalize
        static const union FU {
            u32 ui;
            float f;
        } multiple = { 0x07800000 }; // 2**-112

        FU BiasedFloat;
        BiasedFloat.f = v * multiple.f;
        const u32 u = BiasedFloat.ui;

        const u32 sign = u & 0x80000000;
        u32 body = u & 0x0fffffff;

        return (u16)(sign >> 16 | body >> 13) & 0xFFFF;
    }

    inline auto unpack_color_8888(u32 packed) -> glm::vec4 {
        return glm::vec4(
            unpack_unorm(packed, 8),
            unpack_unorm(packed >> 8, 8),
            unpack_unorm(packed >> 16, 8),
            unpack_unorm(packed >> 24, 8)
        );
    }
}