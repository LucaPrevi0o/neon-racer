#include "car_renderer.hpp"

#include "../ui/neon.hpp"
#include <rlgl.h>

namespace {
Vector3 Cross(Vector3 a, Vector3 b) { return Vector3{a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
void SetTransform(const RaceCar& car) {
    rlTranslatef(car.position.x, car.position.y, car.position.z); const Vector3 right = Cross(car.forward, car.up);
    const Matrix basis = Matrix{car.forward.x,car.forward.y,car.forward.z,0, car.up.x,car.up.y,car.up.z,0, right.x,right.y,right.z,0, 0,0,0,1};
    rlMultMatrixf(reinterpret_cast<const float*>(&basis));
}
}

void DrawRaceCar(const RaceCar& car) {
    rlPushMatrix(); SetTransform(car);
    DrawCube(Vector3{0,0,0},1.1f,0.28f,0.8f,Neon::Pink); DrawCube(Vector3{-0.05f,0.22f,0},0.50f,0.22f,0.56f,Neon::Cyan);
    DrawCubeWires(Vector3{0,0,0},1.12f,0.30f,0.82f,Neon::Yellow); DrawLine3D(Vector3{0,0.17f,0},Vector3{0.65f,0.17f,0},Neon::Yellow); rlPopMatrix();
}
void DrawGhostRaceCar(const RaceCar& car) {
    rlPushMatrix(); SetTransform(car); DrawCube(Vector3{0,0,0},1.1f,0.28f,0.8f,Fade(Neon::Cyan,0.25f));
    DrawCubeWires(Vector3{0,0,0},1.12f,0.30f,0.82f,Fade(Neon::Cyan,0.75f)); rlPopMatrix();
}
