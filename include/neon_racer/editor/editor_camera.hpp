#pragma once

#include <raylib.h>

// Stateless transforms used by the track editor's free-look camera. Input
// polling and editor-specific bindings stay in TrackEditor.
namespace EditorCamera {

void Pan(Camera3D& camera, Vector2 mouseDelta);
void Zoom(Camera3D& camera, float wheelMovement);
void Orbit(Camera3D& camera, Vector2 mouseDelta);
void Raise(Camera3D& camera, float amount);
void Reset(Camera3D& camera);

} // namespace EditorCamera
