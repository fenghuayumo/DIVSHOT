#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244) // Conversion from 'double' to 'float'
#pragma warning(disable : 4702) // unreachable code
#endif

#include "maths/random.h"
#include "core/core.h"

#include <cstdlib>
#include <cmath>
#include <limits>
#include <type_traits>
#include <glm/glm.hpp>

namespace diverse
{
    /// Intersection test result.
    enum Intersection
    {
        OUTSIDE    = 0,
        INTERSECTS = 1,
        INSIDE     = 2
    };
    namespace maths
    {
#undef M_PI
        static constexpr float M_PI              = 3.14159265358979323846264338327950288f;
        static constexpr float M_HALF_PI         = M_PI * 0.5f;
        static constexpr int M_MIN_INT           = 0x80000000;
        static constexpr int M_MAX_INT           = 0x7fffffff;
        static constexpr unsigned M_MIN_UNSIGNED = 0x00000000;
        static constexpr unsigned M_MAX_UNSIGNED = 0xffffffff;

        static constexpr float M_EPSILON       = 0.000001f;
        static constexpr float M_LARGE_EPSILON = 0.00005f;
        static constexpr float M_MIN_NEARCLIP  = 0.01f;
        static constexpr float M_MAX_FOV       = 160.0f;
        static constexpr float M_LARGE_VALUE   = 100000000.0f;
        static constexpr float M_INFINITY      = std::numeric_limits<float>::infinity();
        static constexpr float M_DEGTORAD      = M_PI / 180.0f;
        static constexpr float M_DEGTORAD_2    = M_PI / 360.0f; // M_DEGTORAD / 2.f
        static constexpr float M_RADTODEG      = 1.0f / M_DEGTORAD;

        template <typename T>
        T Squared(T v)
        {
            return v * v;
        }

        /// Check whether two floating point values are equal within accuracy.
        template <class T>
        inline bool Equals(T lhs, T rhs, T eps = M_EPSILON)
        {
            return lhs + eps >= rhs && lhs - eps <= rhs;
        }

        /// Linear interpolation between two values.
        template <class T, class U>
        inline T Lerp(T lhs, T rhs, U t)
        {
            return lhs * (1.0 - t) + rhs * t;
        }

        /// Inverse linear interpolation between two values.
        template <class T>
        inline T InverseLerp(T lhs, T rhs, T x)
        {
            return (x - lhs) / (rhs - lhs);
        }

        /// Return the smaller of two values.
        template <class T, class U>
        inline T Min(T lhs, U rhs)
        {
            return lhs < rhs ? lhs : rhs;
        }

        /// Return the larger of two values.
        template <class T, class U>
        inline T Max(T lhs, U rhs)
        {
            return lhs > rhs ? lhs : rhs;
        }

        /// Return absolute value of a value
        template <class T>
        inline T Abs(T value)
        {
            return value >= 0.0 ? value : -value;
        }

        /// Return the sign of a float (-1, 0 or 1.)
        template <class T>
        inline T Sign(T value)
        {
            return value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0);
        }

        /// Convert degrees to radians.
        template <class T>
        inline T ToRadians(const T degrees)
        {
            return M_DEGTORAD * degrees;
        }

        /// Convert radians to degrees.
        template <class T>
        inline T ToDegrees(const T radians)
        {
            return M_RADTODEG * radians;
        }

        /// Return a representation of the specified floating-point value as a single format bit layout.
        inline unsigned FloatToRawIntBits(float value)
        {
            unsigned u = *((unsigned*)&value);
            return u;
        }

        /// Check whether a floating point value is NaN.
        template <class T>
        inline bool IsNaN(T value)
        {
            return std::isnan(value);
        }

        /// Check whether a floating point value is positive or negative infinity
        template <class T>
        inline bool IsInf(T value)
        {
            return std::isinf(value);
        }

        /// Clamp a number to a range.
        template <class T>
        inline T Clamp(T value, T min, T max)
        {
            if(value < min)
                return min;
            else if(value > max)
                return max;
            else
                return value;
        }

        /// Smoothly damp between values.
        template <class T>
        inline T SmoothStep(T lhs, T rhs, T t)
        {
            t = Clamp((t - lhs) / (rhs - lhs), T(0.0), T(1.0)); // Saturate t
            return t * t * (3.0 - 2.0 * t);
        }

        /// Return sine of an angle in degrees.
        template <class T>
        inline T Sin(T angle)
        {
            return sin(angle * M_DEGTORAD);
        }

        /// Return cosine of an angle in degrees.
        template <class T>
        inline T Cos(T angle)
        {
            return cos(angle * M_DEGTORAD);
        }

        /// Return tangent of an angle in degrees.
        template <class T>
        inline T Tan(T angle)
        {
            return tan(angle * M_DEGTORAD);
        }

        /// Return arc sine in degrees.
        template <class T>
        inline T Asin(T x)
        {
            return M_RADTODEG * asin(Clamp(x, T(-1.0), T(1.0)));
        }

        /// Return arc cosine in degrees.
        template <class T>
        inline T Acos(T x)
        {
            return M_RADTODEG * acos(Clamp(x, T(-1.0), T(1.0)));
        }

        /// Return arc tangent in degrees.
        template <class T>
        inline T Atan(T x)
        {
            return M_RADTODEG * atan(x);
        }

        /// Return arc tangent of y/x in degrees.
        template <class T>
        inline T Atan2(T y, T x)
        {
            return M_RADTODEG * atan2(y, x);
        }

        /// Return X in power Y.
        template <class T>
        inline T Pow(T x, T y)
        {
            return pow(x, y);
        }

        /// Return natural logarithm of X.
        template <class T>
        inline T Ln(T x)
        {
            return log(x);
        }

        /// Return square root of X.
        template <class T>
        inline T Sqrt(T x)
        {
            return sqrt(x);
        }

        /// Return remainder of X/Y.
        template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
        inline T Mod(T x, T y)
        {
            return fmod(x, y);
        }

        /// Return remainder of X/Y.
        template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
        inline T Mod(T x, T y)
        {
            return x % y;
        }

        /// Return positive remainder of X/Y.
        template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
        inline T AbsMod(T x, T y)
        {
            const T result = x % y;
            return result < 0 ? result + y : result;
        }

        /// Return fractional part of passed value in range [0, 1).
        template <class T>
        inline T Fract(T value)
        {
            return value - floor(value);
        }

        /// Round value down.
        template <class T>
        inline T Floor(T x)
        {
            return floor(x);
        }

        /// Round value down. Returns integer value.
        template <class T>
        inline int FloorToInt(T x)
        {
            return static_cast<int>(floor(x));
        }

        /// Round value to nearest integer.
        template <class T>
        inline T Round(T x)
        {
            return round(x);
        }
#ifndef SWIG
        /// Compute average value of the range.
        template <class Iterator>
        inline auto Average(Iterator begin, Iterator end) -> typename std::decay<decltype(*begin)>::type
        {
            using T = typename std::decay<decltype(*begin)>::type;

            T average {};
            unsigned size {};
            for(Iterator it = begin; it != end; ++it)
            {
                average += *it;
                ++size;
            }

            return size != 0 ? average / size : average;
        }
#endif
        /// Round value to nearest integer.
        template <class T>
        inline int RoundToInt(T x)
        {
            return static_cast<int>(round(x));
        }

        /// Round value to nearest multiple.
        template <class T>
        inline T RoundToNearestMultiple(T x, T multiple)
        {
            T mag       = Abs(x);
            multiple    = Abs(multiple);
            T remainder = Mod(mag, multiple);
            if(remainder >= multiple / 2)
                return (FloorToInt<T>(mag / multiple) * multiple + multiple) * Sign(x);
            else
                return (FloorToInt<T>(mag / multiple) * multiple) * Sign(x);
        }

        /// Round value up.
        template <class T>
        inline T Ceil(T x)
        {
            return ceil(x);
        }

        /// Round value up.
        template <class T>
        inline int CeilToInt(T x)
        {
            return static_cast<int>(ceil(x));
        }

        /// Check whether an unsigned integer is a power of two.
        inline bool IsPowerOfTwo(unsigned value)
        {
            return !(value & (value - 1));
        }

        /// Round up to next power of two.
        inline unsigned NextPowerOfTwo(unsigned value)
        {
            // http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
            --value;
            value |= value >> 1u;
            value |= value >> 2u;
            value |= value >> 4u;
            value |= value >> 8u;
            value |= value >> 16u;
            return ++value;
        }

        /// Round up or down to the closest power of two.
        inline unsigned ClosestPowerOfTwo(unsigned value)
        {
            unsigned next = NextPowerOfTwo(value);
            unsigned prev = next >> (unsigned)1;
            return (value - prev) > (next - value) ? next : prev;
        }

        /// Return log base two or the MSB position of the given value.
        inline unsigned LogBaseTwo(unsigned value)
        {
            // http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogObvious
            unsigned ret = 0;
            while(value >>= 1) // Unroll for more speed...
                ++ret;
            return ret;
        }

        /// Count the number of set bits in a mask.
        inline unsigned CountSetBits(unsigned value)
        {
            // Brian Kernighan's method
            unsigned count = 0;
            for(count = 0; value; count++)
                value &= value - 1;
            return count;
        }

        /// Update a hash with the given 8-bit value using the SDBM algorithm.
        inline constexpr unsigned SDBMHash(unsigned hash, unsigned char c)
        {
            return c + (hash << 6u) + (hash << 16u) - hash;
        }

        /// Convert float to half float
        inline unsigned short FloatToHalf(float value)
        {
            unsigned inu = FloatToRawIntBits(value);
            unsigned t1  = inu & 0x7fffffffu; // Non-sign bits
            unsigned t2  = inu & 0x80000000u; // Sign bit
            unsigned t3  = inu & 0x7f800000u; // Exponent

            t1 >>= 13; // Align mantissa on MSB
            t2 >>= 16; // Shift sign bit into position

            t1 -= 0x1c000; // Adjust bias

            t1 = (t3 < 0x38800000) ? 0 : t1;      // Flush-to-zero
            t1 = (t3 > 0x47000000) ? 0x7bff : t1; // Clamp-to-max
            t1 = (t3 == 0 ? 0 : t1);              // Denormals-as-zero

            t1 |= t2; // Re-insert sign bit

            return (unsigned short)t1;
        }

        /// Convert half float to float
        inline float HalfToFloat(unsigned short value)
        {
            unsigned t1 = value & 0x7fffu; // Non-sign bits
            unsigned t2 = value & 0x8000u; // Sign bit
            unsigned t3 = value & 0x7c00u; // Exponent

            t1 <<= 13; // Align mantissa on MSB
            t2 <<= 16; // Shift sign bit into position

            t1 += 0x38000000; // Adjust bias

            t1 = (t3 == 0 ? 0 : t1); // Denormals-as-zero

            t1 |= t2; // Re-insert sign bit

            float out;
            *((unsigned*)&out) = t1;
            return out;
        }

        /// Wrap a value fitting it in the range defined by [min, max)
        template <typename T>
        inline T Wrap(T value, T min, T max)
        {
            T range = max - min;
            return min + Mod(value, range);
        }

        /// Calculate both sine and cosine, with angle in degrees.
        void SinCos(float angle, float& sin, float& cos);
        uint32_t nChoosek(uint32_t n, uint32_t k);
        glm::vec3 ComputeClosestPointOnSegment(const glm::vec3& segPointA, const glm::vec3& segPointB, const glm::vec3& pointC);
        void ClosestPointBetweenTwoSegments(const glm::vec3& seg1PointA, const glm::vec3& seg1PointB,
                                            const glm::vec3& seg2PointA, const glm::vec3& seg2PointB,
                                            glm::vec3& closestPointSeg1, glm::vec3& closestPointSeg2);

        bool AreVectorsParallel(const glm::vec3& v1, const glm::vec3& v2);

        glm::vec2 WorldToScreen(const glm::vec3& worldPos, const glm::mat4& mvp, float width, float height, float winPosX = 0.0f, float winPosY = 0.0f);

        void set_scale(glm::mat4& transform, float scale);
        void set_scale(glm::mat4& transform, const glm::vec3& scale);
        void SetRotation(glm::mat4& transform, const glm::vec3& rotation);
        void SetTranslation(glm::mat4& transform, const glm::vec3& translation);

        glm::vec3 get_scale(const glm::mat4& transform);
        glm::vec3 GetRotation(const glm::mat4& transform);

        glm::mat4 Mat4FromTRS(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);


        inline auto divide_rounding_up(u32 elements, u32 bllock_size) -> u32
        {
            return (elements + bllock_size - 1) / bllock_size;
        }
        inline auto smoothstep(f32 edge0, f32 edge1, f32 x) -> f32
        {
            // Scale, bias and saturate x to 0..1 range
            x = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            // Evaluate polynomial
            return x * x * (3.0f - 2.0f * x);
        }

        template <typename T> inline T align_power_two(T value)
        {
            return value == 0 ? 0 : 1 << (u32)std::ceil(std::log2(value));
        }

        template <typename T> inline T ceil_log_two(T value)
        {
            return std::ceil(std::log2(value));
        }

        
        constexpr bool any(const glm::bvec2& a) { return a.x || a.y; }
        constexpr bool any(const glm::bvec3& a) { return a.x || a.y || a.z; }
        constexpr bool any(const glm::bvec4& a) { return a.x || a.y || a.z || a.w; }
        constexpr bool all(const glm::bvec2& a) { return a.x && a.y; }
        constexpr bool all(const glm::bvec3& a) { return a.x && a.y && a.z; }
        constexpr bool all(const glm::bvec4& a) { return a.x && a.y && a.z && a.w; }
    }
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace glm
{
    glm::vec3 operator*(const glm::mat4& a, const glm::vec3& b);
}
