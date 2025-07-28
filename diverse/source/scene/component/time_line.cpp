#include "time_line.h"
#define TIMELINE_OVER_LENGTH    5000        // add 5 seconds end of timeline

namespace diverse
{
    CameraKeyFrameVar lerp(const CameraKeyFrameVar& p0, const CameraKeyFrameVar& p1, float t, float t0, float t1) 
    {
        t = (t - t0) / (t1 - t0);
        glm::quat R1 = p1.R;

        // take the short path
        if (glm::dot(R1, p0.R) < 0.0f)  {
            R1 = -R1;
        }

        return {
            slerp(p0.R, R1, t),
            p0.T + (p1.T - p0.T) * t,
            p0.slice + (p1.slice - p0.slice) * t,
            p0.scale + (p1.scale - p0.scale) * t,
            p0.fov + (p1.fov - p0.fov) * t,
            p0.aperture_size + (p1.aperture_size - p0.aperture_size) * t,
        };
    }

    CameraKeyFrameVar spline(float t, const CameraKeyFrameVar& p0, const CameraKeyFrameVar& p1, const CameraKeyFrameVar& p2, const CameraKeyFrameVar& p3) {
        if (1) {
            // catmull rom spline
            CameraKeyFrameVar q0 = lerp(p0, p1, t, -1.f, 0.f);
            CameraKeyFrameVar q1 = lerp(p1, p2, t,  0.f, 1.f);
            CameraKeyFrameVar q2 = lerp(p2, p3, t,  1.f, 2.f);
            CameraKeyFrameVar r0 = lerp(q0, q1, t, -1.f, 1.f);
            CameraKeyFrameVar r1 = lerp(q1, q2, t,  0.f, 2.f);
            return lerp(r0, r1, t, 0.f, 1.f);
        } else {
            // cubic bspline
            float tt = t*t;
            float ttt = t*t*t;
            float a = (1-t)*(1-t)*(1-t)*(1.f/6.f);
            float b = (3.f*ttt-6.f*tt+4.f)*(1.f/6.f);
            float c = (-3.f*ttt+3.f*tt+3.f*t+1.f)*(1.f/6.f);
            float d = ttt*(1.f/6.f);
            return p0 * a + p1 * b + p2 * c + p3 * d;
        }
    }

    KeyFrame lerp(const KeyFrame& p0, const KeyFrame& p1, float t, float t0, float t1) 
    {
        t = (t - t0) / (t1 - t0);
        int64 timepoint = p0.timePoint + (p1.timePoint - p0.timePoint) * t;
        return {timepoint, lerp(p0.cameraElement,p1.cameraElement,t,t0,t1)};
    }

    KeyFrame spline(float t, const KeyFrame& p0, const KeyFrame& p1, const KeyFrame& p2, const KeyFrame& p3) {
        float tt = t*t;
        float ttt = t*t*t;
        float a = (1-t)*(1-t)*(1-t)*(1.f/6.f);
        float b = (3.f*ttt-6.f*tt+4.f)*(1.f/6.f);
        float c = (-3.f*ttt+3.f*tt+3.f*t+1.f)*(1.f/6.f);
        float d = ttt*(1.f/6.f);
        int64 timepoint = p0.timePoint * a + p1.timePoint * b + p2.timePoint * c + p3.timePoint * d;

        return {timepoint, spline(t,p0.cameraElement,p1.cameraElement,p2.cameraElement,p3.cameraElement)};
    }

    KeyFrameTimeLine::KeyFrameTimeLine()
    {}

    KeyFrameTimeLine::~KeyFrameTimeLine()
    {
    }

    void KeyFrameTimeLine::update()
    {
        int64_t start_min = INT64_MAX;
        int64_t end_max = INT64_MIN;
        for (const auto& keyframe : keyframes)
        {
            if (keyframe.timePoint < start_min)
                start_min = keyframe.timePoint;
            if (keyframe.timePoint > end_max)
                end_max = keyframe.timePoint;
        }
        if (!keyframes.empty() )
        {
            start = start_min;
            end = end_max;
        }
    }

    KeyFrame KeyFrameTimeLine::evalKeyFrame(float t)
    {
        if (keyframes.empty())
            return {};
        // make room for last frame == first frame when looping
        t *=  keyframes.size() - 1;
        int t1 = (int)floorf(t);
        return spline(t - floorf(t), getKeyFrame(t1 - 1), getKeyFrame(t1), getKeyFrame(t1 + 1), getKeyFrame(t1 + 2));
    }
}