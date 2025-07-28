#pragma once
#include "core/base_type.h"
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <memory>
#include <vector>
#include <list>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/cereal.hpp>
namespace diverse
{
    struct IDGenerator
    {
        int64 GenerateID();

        void SetState(int64 state);
        int64 State() const;

    private:

    };
    
    struct KeyFrameVariable
    {
        int     type;
        bool    bExpand = true;
        int     itemHeight = 32;
    };

    struct CameraKeyFrameVar : public KeyFrameVariable
    {
        glm::quat R;
        glm::vec3 T;
        float slice = 1.0f;
        float scale = 1.0f; // not a scale factor as in scaling the world, but the value of m_scale (setting the focal plane along with slice)
        float fov = 60.0f;
        float aperture_size = 1.0f;
        glm::mat4x3 m() const 
        {
            auto rot = glm::toMat3(glm::normalize(glm::quat(R)));
            return glm::mat4x3(rot[0], rot[1], rot[2], T);
        }

        void from_m(const glm::mat4x3& rv) 
        {
            T = rv[3];
            R = glm::quat(glm::mat3(rv));
        }

        CameraKeyFrameVar() = default;
        CameraKeyFrameVar(const glm::quat& r, const glm::vec3& t, float sl, float sc, float fv, float df) : R(r), T(t), slice(sl), scale(sc), fov(fv), aperture_size(df) {}
        CameraKeyFrameVar(glm::mat4x3 m, float sl, float sc, float fv, float df) : slice(sl), scale(sc), fov(fv), aperture_size(df){ from_m(m); }
        CameraKeyFrameVar operator*(float f) const { return {R*f, T*f, slice*f, scale*f, fov*f, aperture_size*f}; }
        CameraKeyFrameVar operator+(const CameraKeyFrameVar& rhs) const {
            glm::quat Rr = rhs.R;
            if (dot(Rr, R) < 0.0f) Rr = -Rr;
            return {R+Rr, T+rhs.T,slice+rhs.slice, scale+rhs.scale, fov+rhs.fov, aperture_size+rhs.aperture_size};
        }

        bool samePosAs(const CameraKeyFrameVar& rhs) const {
            return distance(T, rhs.T) < 0.0001f && fabsf(dot(R, rhs.R)) >= 0.999f;
        }

        template <typename Archive>
        void save(Archive& archive) const
        {
            archive(cereal::make_nvp("fov", fov), cereal::make_nvp("aperture_size", aperture_size), cereal::make_nvp("scale", scale), cereal::make_nvp("slice", slice));
            archive(cereal::make_nvp("rotation", R), cereal::make_nvp("translation", T));
        }
        template <typename Archive>
        void load(Archive& archive)
        {
            archive(cereal::make_nvp("fov", fov), cereal::make_nvp("aperture_size", aperture_size), cereal::make_nvp("scale", scale), cereal::make_nvp("slice", slice));
            archive(cereal::make_nvp("rotation", R), cereal::make_nvp("translation", T));
        }
    };

    struct KeyFrame
    {
        int64 timePoint;
        CameraKeyFrameVar   cameraElement;

        template <typename Archive>
        void save(Archive& archive) const
        {
            archive(cereal::make_nvp("timepoint", timePoint), cereal::make_nvp("CameraKeyFrameVar", cameraElement));
        }
        template <typename Archive>
        void load(Archive& archive)
        {
            archive(cereal::make_nvp("timepoint", timePoint), cereal::make_nvp("CameraKeyFrameVar", cameraElement));
        }
    };

    CameraKeyFrameVar lerp(const CameraKeyFrameVar& p0, const CameraKeyFrameVar& p1, float t, float t0, float t1);
    CameraKeyFrameVar spline(float t, const CameraKeyFrameVar& p0, const CameraKeyFrameVar& p1, const CameraKeyFrameVar& p2, const CameraKeyFrameVar& p3);

    KeyFrame lerp(const KeyFrame& p0, const KeyFrame& p1, float t, float t0, float t1);
    KeyFrame spline(float t, const KeyFrame& p0, const KeyFrame& p1, const KeyFrame& p2, const KeyFrame& p3);

    // KeyFrame evalKeyFrame();

    struct KeyFrameTimeLine
    {
        KeyFrameTimeLine();
        ~KeyFrameTimeLine();

        std::vector<std::shared_ptr<KeyFrameVariable>>  keyframeVars;
        std::vector<KeyFrame> keyframes;
        int32 currentTime = 0;
        int32 startShowTime = 0;                //
        int32 endShowTime = 100;                 //
        int32 visibleTime = 0;                //
        int32 start   {0};                   // whole timeline start in ms, project saved
        int32 end     {0};                   // whole timeline end in ms, project saved
        bool bSeeking = false;
        void update();
        void updateRange();

        KeyFrame    evalKeyFrame(float t);

        const KeyFrame& getKeyFrame(int i) {return keyframes[glm::clamp(i, 0, (int)keyframes.size() - 1)];}
        template <typename Archive>
        void save(Archive& archive) const
        {
            archive(cereal::make_nvp("keyframes", keyframes.size()));
            for(int i=0; i<keyframes.size(); i++)
            {
                archive(cereal::make_nvp("keyframe"+ std::to_string(i), keyframes[i]));
            }
            archive(cereal::make_nvp("timline_start", start), cereal::make_nvp("timline_end", end));
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            int n = 0;
            archive(cereal::make_nvp("keyframes", n));
            keyframes.resize(n);
            for(int i=0; i<keyframes.size(); i++)
            {
                archive(cereal::make_nvp("keyframe"+ std::to_string(i), keyframes[i]));
            }
            archive(cereal::make_nvp("timline_start", start), cereal::make_nvp("timline_end", end));
        }
    };
}