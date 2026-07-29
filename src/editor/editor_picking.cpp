#include "editor_picking.hpp"

#include "../track/track_road_geometry.hpp"

#include <cmath>

namespace {

Vector3 HeadingVector(Heading heading) {
    switch (heading) {
    case Heading::North: return Vector3{0.0f, 0.0f, -1.0f};
    case Heading::East: return Vector3{1.0f, 0.0f, 0.0f};
    case Heading::South: return Vector3{0.0f, 0.0f, 1.0f};
    case Heading::West: return Vector3{-1.0f, 0.0f, 0.0f};
    }
    return Vector3{0.0f, 0.0f, 0.0f};
}

Vector3 SurfaceRight(const TrackSurfaceSample& sample) {
    const TrackRoadAxis axis = TrackRoadGeometry::SurfaceRightAxis(sample);
    return Vector3{axis.x, axis.y, axis.z};
}

void ConsiderTriangle(const Ray& ray, Vector3 first, Vector3 second, Vector3 third,
                      std::uint32_t pieceId, float& nearestDistance, std::uint32_t& pickedId) {
    const RayCollision hit = GetRayCollisionTriangle(ray, first, second, third);
    if (hit.hit && hit.distance < nearestDistance) {
        nearestDistance = hit.distance;
        pickedId = pieceId;
    }
}

void ConsiderStraightSurface(const Ray& ray, const TrackPiece& piece, float& nearestDistance,
                             std::uint32_t& pickedId) {
    const TrackConnector entry = piece.EntryConnector();
    const TrackConnector exit = piece.ExitConnector();
    const Vector3 right = HeadingVector(static_cast<Heading>((static_cast<int>(piece.entryHeading) + 1) % 4));
    const float entryHalfWidth = piece.WidthAt(0.0f) * 0.5f;
    const float exitHalfWidth = piece.WidthAt(1.0f) * 0.5f;
    const Vector3 from = Vector3{static_cast<float>(entry.position.x), static_cast<float>(entry.position.y) + 0.08f,
                                 static_cast<float>(entry.position.z)};
    const Vector3 to = Vector3{static_cast<float>(exit.position.x), static_cast<float>(exit.position.y) + 0.08f,
                               static_cast<float>(exit.position.z)};
    const Vector3 fromRight = Vector3{from.x + right.x * entryHalfWidth, from.y, from.z + right.z * entryHalfWidth};
    const Vector3 fromLeft = Vector3{from.x - right.x * entryHalfWidth, from.y, from.z - right.z * entryHalfWidth};
    const Vector3 toRight = Vector3{to.x + right.x * exitHalfWidth, to.y, to.z + right.z * exitHalfWidth};
    const Vector3 toLeft = Vector3{to.x - right.x * exitHalfWidth, to.y, to.z - right.z * exitHalfWidth};
    ConsiderTriangle(ray, fromRight, toRight, toLeft, piece.id, nearestDistance, pickedId);
    ConsiderTriangle(ray, fromRight, toLeft, fromLeft, piece.id, nearestDistance, pickedId);
}

void ConsiderCurvedSurface(const Ray& ray, const TrackPiece& piece, float& nearestDistance,
                           std::uint32_t& pickedId) {
    const std::vector<TrackSurfaceSample> samples = piece.SurfaceSamples();
    for (std::size_t index = 0; index + 1 < samples.size(); ++index) {
        const TrackSurfaceSample& from = samples[index];
        const TrackSurfaceSample& to = samples[index + 1];
        const Vector3 fromRightAxis = SurfaceRight(from);
        const Vector3 toRightAxis = SurfaceRight(to);
        const Vector3 fromRight = Vector3{from.x + fromRightAxis.x * from.halfWidth,
                                          from.y + fromRightAxis.y * from.halfWidth + 0.08f,
                                          from.z + fromRightAxis.z * from.halfWidth};
        const Vector3 fromLeft = Vector3{from.x - fromRightAxis.x * from.halfWidth,
                                         from.y - fromRightAxis.y * from.halfWidth + 0.08f,
                                         from.z - fromRightAxis.z * from.halfWidth};
        const Vector3 toRight = Vector3{to.x + toRightAxis.x * to.halfWidth,
                                        to.y + toRightAxis.y * to.halfWidth + 0.08f,
                                        to.z + toRightAxis.z * to.halfWidth};
        const Vector3 toLeft = Vector3{to.x - toRightAxis.x * to.halfWidth,
                                       to.y - toRightAxis.y * to.halfWidth + 0.08f,
                                       to.z - toRightAxis.z * to.halfWidth};
        ConsiderTriangle(ray, fromRight, toRight, toLeft, piece.id, nearestDistance, pickedId);
        ConsiderTriangle(ray, fromRight, toLeft, fromLeft, piece.id, nearestDistance, pickedId);
    }
}

void ConsiderRoadSurface(const Ray& ray, const TrackPiece& piece, float& nearestDistance,
                         std::uint32_t& pickedId) {
    const std::vector<TrackPiece> arms = TrackRoadGeometry::PhysicalRoadArms(piece);
    for (std::vector<TrackPiece>::const_iterator arm = arms.begin(); arm != arms.end(); ++arm) {
        if (arm->type == TrackPieceType::Straight) {
            ConsiderStraightSurface(ray, *arm, nearestDistance, pickedId);
        } else {
            ConsiderCurvedSurface(ray, *arm, nearestDistance, pickedId);
        }
    }
}

} // namespace

namespace EditorPicking {

bool GridPositionFromRay(const Ray& ray, GridPosition& position) {
    if (std::fabs(ray.direction.y) < 0.0001f) return false;
    const float distance = -ray.position.y / ray.direction.y;
    if (distance < 0.0f) return false;
    const Vector3 hit = Vector3{ray.position.x + ray.direction.x * distance, 0.0f,
                                ray.position.z + ray.direction.z * distance};
    position = GridPosition{static_cast<int>(std::round(hit.x)), 0, static_cast<int>(std::round(hit.z))};
    return true;
}

std::uint32_t PickPieceFromRay(const Ray& ray, const std::vector<TrackPiece>& pieces) {
    float nearestDistance = 1000000.0f;
    std::uint32_t picked = 0;
    for (std::vector<TrackPiece>::const_iterator piece = pieces.begin(); piece != pieces.end(); ++piece) {
        ConsiderRoadSurface(ray, *piece, nearestDistance, picked);
    }
    return picked;
}

} // namespace EditorPicking
