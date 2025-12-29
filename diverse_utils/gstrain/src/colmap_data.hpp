#pragma once

#ifndef COLMAP_SPARSEPOINT
#define COLMAP_SPARSEPOINT
namespace colmap {

    template<typename T>
    struct vec3 {
        T x, y, z;
    };

    template <typename T>
    struct vec4 {
        T x, y, z, w;
    };

    struct SparsePoint {
        vec3<float> xyz;
        vec4<unsigned char> color;
    };
    struct CameraTrack {
        uint32_t camera_id;
        int model_id;
        size_t width = 0;
        size_t height = 0;
        std::vector<double> params;
    };

    struct ImageTrack {
        uint32_t image_id;
        std::string name;
        uint32_t camera_id;
        vec4<float> rotation;
        vec3<float>  translation;
    };
}
#endif
