#include "utils.hpp"

float fov2focal(float fov, float pixels)
{
    return pixels / (2 * tan(fov / 2));
}

float focal2fov(float focal, float pixels)
{
    return 2 * atan(pixels / (2 * focal));
}

glm::mat4 getProjectionMatrix(float znear, float zfar, float fovX, float fovY)
{
    //GLM_CLIP_CONTROL_LH_ZO glm::perspectiveLH_ZO(fovY, aspect, znear, zfar);
    auto tanHalfFovY = tan((fovY / 2));
    auto tanHalfFovX = tan((fovX / 2));

    auto top = tanHalfFovY * znear;
    auto bottom = -top;
    auto right = tanHalfFovX * znear;
    auto left = -right;

    auto P = glm::mat4(0);

    auto z_sign = 1.0;

    P[0][0] = 2.0 * znear / (right - left);
    P[1][1] = 2.0 * znear / (top - bottom);
    P[0][2] = (right + left) / (right - left);
    P[1][2] = (top + bottom) / (top - bottom);
    P[3][2] = z_sign;
    P[2][2] = z_sign * zfar / (zfar - znear);
    P[2][3] = -(zfar * znear) / (zfar - znear);
    return P;
}
