#include "editor_camera.hpp"

#include <algorithm>
#include <cmath>

namespace EditorCamera {

void Pan(Camera3D& camera, Vector2 mouseDelta) {
    Vector3 forward = Vector3{camera.target.x - camera.position.x, 0.0f,
                              camera.target.z - camera.position.z};
    const float forwardLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);
    if (forwardLength < 0.001f) return;
    forward.x /= forwardLength;
    forward.z /= forwardLength;
    const Vector3 right = Vector3{-forward.z, 0.0f, forward.x};
    const float scale = 0.035f;
    const Vector3 offset = Vector3{right.x * -mouseDelta.x * scale + forward.x * mouseDelta.y * scale,
                                   0.0f,
                                   right.z * -mouseDelta.x * scale + forward.z * mouseDelta.y * scale};
    camera.position.x += offset.x;
    camera.position.z += offset.z;
    camera.target.x += offset.x;
    camera.target.z += offset.z;
}

void Zoom(Camera3D& camera, float wheelMovement) {
    const Vector3 offset = Vector3{camera.position.x - camera.target.x, camera.position.y - camera.target.y,
                                   camera.position.z - camera.target.z};
    const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
    if (distance < 0.001f) return;
    const float nextDistance = std::max(7.0f, std::min(55.0f, distance - wheelMovement * 2.0f));
    const float scale = nextDistance / distance;
    camera.position = Vector3{camera.target.x + offset.x * scale, camera.target.y + offset.y * scale,
                              camera.target.z + offset.z * scale};
}

void Orbit(Camera3D& camera, Vector2 mouseDelta) {
    Vector3 offset = Vector3{camera.position.x - camera.target.x, camera.position.y - camera.target.y,
                             camera.position.z - camera.target.z};
    const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
    if (distance < 0.001f) return;

    float yaw = std::atan2(offset.z, offset.x);
    float pitch = std::asin(offset.y / distance);
    yaw -= mouseDelta.x * 0.010f;
    pitch = std::max(0.12f, std::min(1.45f, pitch + mouseDelta.y * 0.010f));
    const float horizontalDistance = distance * std::cos(pitch);
    camera.position = Vector3{camera.target.x + horizontalDistance * std::cos(yaw),
                              camera.target.y + distance * std::sin(pitch),
                              camera.target.z + horizontalDistance * std::sin(yaw)};
}

void Raise(Camera3D& camera, float amount) {
    camera.position.y += amount;
    camera.target.y += amount;
}

void Reset(Camera3D& camera) {
    camera.position = Vector3{18.0f, 18.0f, 18.0f};
    camera.target = Vector3{0.0f, 0.0f, 0.0f};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
}

} // namespace EditorCamera
