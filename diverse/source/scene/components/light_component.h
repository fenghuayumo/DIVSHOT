#pragma once

#include <glm/glm.hpp>
#include <cereal/cereal.hpp>

namespace diverse
{

    // Common light data - shared by all light types
    struct LightCommon
    {
        float intensity = 1.0f;
        glm::vec3 radiance = glm::vec3(1.0f, 1.0f, 1.0f);
        bool cast_shadow = true;
        bool enabled = true;

        // Serialization
        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(
                cereal::make_nvp("intensity", intensity),
                cereal::make_nvp("radiance", radiance),
                cereal::make_nvp("cast_shadow", cast_shadow),
                cereal::make_nvp("enabled", enabled)
            );
        }
    };

    // Directional light component (sun/moon)
    struct DirectionalLight
    {
        float angular_size = 0.1f;  // Angular size in degrees

        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(cereal::make_nvp("angular_size", angular_size));
        }
    };

    // Point light component (omnidirectional)
    struct PointLight
    {
        float radius = 1.0f;  // Radius of sphere light (0 = delta light)

        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(cereal::make_nvp("radius", radius));
        }
    };

    // Spot light component
    struct SpotLight
    {
        float radius = 0.01f;          // Radius of sphere light
        float inner_angle = 180.0f;    // Inner cone angle in degrees
        float outer_angle = 180.0f;    // Outer cone angle in degrees
        int profile_texture_index = -1; // IES profile texture index

        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(
                cereal::make_nvp("radius", radius),
                cereal::make_nvp("inner_angle", inner_angle),
                cereal::make_nvp("outer_angle", outer_angle),
                cereal::make_nvp("profile_texture_index", profile_texture_index)
            );
        }
    };

    // Rectangular area light component
    struct RectLight
    {
        float width = 1.0f;
        float height = 1.0f;
        float radius = 1.0f;

        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(
                cereal::make_nvp("width", width),
                cereal::make_nvp("height", height),
                cereal::make_nvp("radius", radius)
            );
        }
    };

    // Disk area light component
    struct DiskLight
    {
        float radius = 1.0f;

        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(cereal::make_nvp("radius", radius));
        }
    };

    // Cylinder area light component
    struct CylinderLight
    {
        float radius = 0.5f;
        float length = 1.0f;

        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(
                cereal::make_nvp("radius", radius),
                cereal::make_nvp("length", length)
            );
        }
    };

}
