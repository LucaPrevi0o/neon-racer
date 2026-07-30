#include "car_renderer.hpp"

#include "../ui/neon.hpp"
#include <rlgl.h>

namespace {
RaceVector3 Cross(RaceVector3 a, RaceVector3 b) {
    return RaceVector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

void SetTransform(const RaceCar& car) {
    const RaceVector3 bodyCenter = RaceCarVisualCenter(car);
    rlTranslatef(bodyCenter.x, bodyCenter.y, bodyCenter.z);
    const RaceVector3 right = Cross(car.forward, car.up);
    // rlMultMatrixf takes a column-major float array. The mesh's local X/Y/Z
    // axes map to the car's forward/up/right road frame respectively.
    const float basis[16] = {car.forward.x, car.forward.y, car.forward.z, 0.0f,
                             car.up.x, car.up.y, car.up.z, 0.0f,
                             right.x, right.y, right.z, 0.0f,
                             0.0f, 0.0f, 0.0f, 1.0f};
    rlMultMatrixf(basis);
}

} // namespace

void DrawRaceCar(const RaceCar& car) {
    rlPushMatrix();
    SetTransform(car);
    DrawCube(Vector3{0.0f, 0.0f, 0.0f}, 1.1f, 0.28f, 0.8f, Neon::Pink);
    DrawCube(Vector3{-0.05f, 0.22f, 0.0f}, 0.50f, 0.22f, 0.56f, Neon::Cyan);
    DrawCubeWires(Vector3{0.0f, 0.0f, 0.0f}, 1.12f, 0.30f, 0.82f, Neon::Yellow);
    DrawLine3D(Vector3{0.0f, 0.17f, 0.0f}, Vector3{0.65f, 0.17f, 0.0f}, Neon::Yellow);
    rlPopMatrix();
}

void DrawGhostRaceCar(const RaceCar& car) {
    rlPushMatrix();
    SetTransform(car);
    DrawCube(Vector3{0.0f, 0.0f, 0.0f}, 1.1f, 0.28f, 0.8f, Fade(Neon::Cyan, 0.25f));
    DrawCubeWires(Vector3{0.0f, 0.0f, 0.0f}, 1.12f, 0.30f, 0.82f, Fade(Neon::Cyan, 0.75f));
    rlPopMatrix();
}
