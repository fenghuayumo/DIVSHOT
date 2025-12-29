#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/ext.hpp>
#include "colmap_data.hpp"

inline glm::vec3 getCameraPosFromColmapCameraTrack(const colmap::ImageTrack &imgTrack)
{
    glm::quat q(imgTrack.rotation.w, imgTrack.rotation.x, imgTrack.rotation.y, imgTrack.rotation.z);
    // Create rotation matrix from quaternion
    glm::mat4 R = glm::mat4_cast(q);
    // Create translation matrix
    glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(imgTrack.translation.x, imgTrack.translation.y, imgTrack.translation.z));
    // Calculate inverse rotation matrix (transpose for rotation matrices)
    glm::mat4 Rinv = glm::transpose(R);
    // Calculate inverse translation (camera position in world coordinates)
    glm::vec4 Tinv = -Rinv * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); //Note:  This is different from the PyTorch code.  The PyTorch code calculates -Rinv * T, which is incorrect for obtaining the camera position.  This version correctly calculates the camera position.
    // Extract camera position
    glm::vec3 cameraPos = glm::vec3(Tinv);
    // COLMAP uses OpenCV's coordinate system (OpenCV's Z-axis points forward, while OpenGL's Z-axis points backward).  Adjust accordingly.
    cameraPos.z *= -1.0f;
    return cameraPos;

}

inline glm::mat3 getCameraRotationFromColmapCameraTrack(const colmap::ImageTrack& imgTrack)
{
    glm::quat q(imgTrack.rotation.w, imgTrack.rotation.x, imgTrack.rotation.y, imgTrack.rotation.z);
    glm::mat3 R = glm::mat3_cast(q);
    // COLMAP uses OpenCV's coordinate system (OpenCV's Z-axis points forward, while OpenGL's Z-axis points backward).  Adjust accordingly.
    R[1] = -R[1];
    return R;
}
