#pragma once
#include <cstdlib>
#include <glm/detail/type_half.hpp>

namespace diverse
{
    struct HalfFloat
    {
        HalfFloat(){}
        HalfFloat(float x)
        {
            //auto f32_2_f16 = [](float v)->unsigned short
            //{
            //    // Multiplying by 2^-112 causes exponents below -14 to denormalize
            //    static const union FU {
            //        unsigned int ui;
            //        float f;
            //    } multiple = { 0x07800000 }; // 2**-112

            //    FU BiasedFloat;
            //    BiasedFloat.f = v * multiple.f;
            //    const unsigned int u = BiasedFloat.ui;

            //    const unsigned int sign = u & 0x80000000;
            //    unsigned int body = u & 0x0fffffff;

            //    return (unsigned short)(sign >> 16 | body >> 13) & 0xFFFF;
            //};
            //data = f32_2_f16(x);
            data = glm::detail::toFloat16(x);
        }

        inline auto operator+(const HalfFloat& rhs)->HalfFloat
        {
            auto res = glm::detail::toFloat32(data) + glm::detail::toFloat32(rhs.data);
            return HalfFloat(res);
        }

        inline auto operator-(const HalfFloat& rhs)->HalfFloat
        {
            auto res = glm::detail::toFloat32(data) - glm::detail::toFloat32(rhs.data);
            return HalfFloat(res);
        }

        inline auto operator*(const HalfFloat& rhs)->HalfFloat
        {
            auto res = glm::detail::toFloat32(data) * glm::detail::toFloat32(rhs.data);
            return HalfFloat(res);
        }

        inline auto operator/(const HalfFloat& rhs)->HalfFloat
        {
            auto res = glm::detail::toFloat32(data) / glm::detail::toFloat32(rhs.data);
            return HalfFloat(res);
        }

        inline auto operator+=(const HalfFloat& rhs)->HalfFloat&
        {
            auto res = glm::detail::toFloat32(data) + glm::detail::toFloat32(rhs.data);
            data = glm::detail::toFloat16(res);
            return *this;
        }

        inline auto operator-=(const HalfFloat& rhs)->HalfFloat&
        {
            auto res = glm::detail::toFloat32(data) - glm::detail::toFloat32(rhs.data);
            data = glm::detail::toFloat16(res);
            return *this;
        }

        inline auto operator*=(const HalfFloat& rhs)->HalfFloat&
        {
            auto res = glm::detail::toFloat32(data) * glm::detail::toFloat32(rhs.data);
            data = glm::detail::toFloat16(res);
            return *this;
        }

        inline auto operator/=(const HalfFloat& rhs)->HalfFloat&
        {
            auto res = glm::detail::toFloat32(data) / glm::detail::toFloat32(rhs.data);
            data = glm::detail::toFloat16(res);
            return *this;
        }

        inline auto operator/(f32 rhs)const->f32
        {
            auto res = glm::detail::toFloat32(data) / rhs;
            return res;
        }
        inline auto operator*(f32 rhs)const->f32
        {
            auto res = glm::detail::toFloat32(data) * rhs;
            return res;
        }
        inline auto to_f32() const->f32
        {
            auto res = glm::detail::toFloat32(data);
            return res;
        }
    protected:
        unsigned short data = 0;
    };

    using f16 = HalfFloat;
}