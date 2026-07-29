#include "track_renderer.hpp"

#include "../track/track_road_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <rlgl.h>

namespace {

void SetColor(Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
}

Vector3 RightVector(Heading heading) {
    switch (heading) {
    case Heading::North: return Vector3{1.0f, 0.0f, 0.0f};
    case Heading::East: return Vector3{0.0f, 0.0f, 1.0f};
    case Heading::South: return Vector3{-1.0f, 0.0f, 0.0f};
    case Heading::West: return Vector3{0.0f, 0.0f, -1.0f};
    }
    return Vector3{0.0f, 0.0f, 0.0f};
}

Vector3 SurfaceRight(const TrackSurfaceSample& sample) {
    const TrackRoadAxis axis = TrackRoadGeometry::SurfaceRightAxis(sample);
    return Vector3{axis.x, axis.y, axis.z};
}

struct RoadQuad {
    Vector3 corners[4];
};

struct RoadBoundarySegment {
    Vector3 from;
    Vector3 to;
    bool hasGuardrail;
};

Vector3 AddHeight(const Vector3& point, float height) {
    return Vector3{point.x, point.y + height, point.z};
}

Vector3 Interpolate(const Vector3& from, const Vector3& to, float progress) {
    return Vector3{from.x + (to.x - from.x) * progress,
                   from.y + (to.y - from.y) * progress,
                   from.z + (to.z - from.z) * progress};
}

RoadQuad StraightRoadQuad(const TrackPiece& piece) {
    // Offset straights are road parallelograms: their ends stay perpendicular
    // to the cardinal connector heading instead of perpendicular to the
    // diagonal centreline. This makes adjacent pieces meet cleanly.
    const TrackConnector entry = piece.EntryConnector();
    const TrackConnector exit = piece.ExitConnector();
    const Vector3 right = RightVector(piece.entryHeading);
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

    RoadQuad road = {{fromRight, toRight, toLeft, fromLeft}};
    return road;
}

void DrawRoadSurface(const RoadQuad& road, Color surfaceColor) {
    const Vector3& fromRight = road.corners[0];
    const Vector3& toRight = road.corners[1];
    const Vector3& toLeft = road.corners[2];
    const Vector3& fromLeft = road.corners[3];

    rlBegin(RL_QUADS);
    SetColor(surfaceColor);
    rlVertex3f(fromRight.x, fromRight.y, fromRight.z);
    rlVertex3f(toRight.x, toRight.y, toRight.z);
    rlVertex3f(toLeft.x, toLeft.y, toLeft.z);
    rlVertex3f(fromLeft.x, fromLeft.y, fromLeft.z);
    rlEnd();
}

void DrawStraightSurface(const TrackPiece& piece, Color surfaceColor, Color edgeColor) {
    const RoadQuad road = StraightRoadQuad(piece);
    const Vector3& fromRight = road.corners[0];
    const Vector3& toRight = road.corners[1];
    const Vector3& toLeft = road.corners[2];
    const Vector3& fromLeft = road.corners[3];

    DrawRoadSurface(road, surfaceColor);

    const float edgeLift = 0.02f;
    DrawLine3D(Vector3{fromRight.x, fromRight.y + edgeLift, fromRight.z},
               Vector3{toRight.x, toRight.y + edgeLift, toRight.z}, edgeColor);
    DrawLine3D(Vector3{fromLeft.x, fromLeft.y + edgeLift, fromLeft.z},
               Vector3{toLeft.x, toLeft.y + edgeLift, toLeft.z}, edgeColor);
    DrawLine3D(Vector3{fromLeft.x, fromLeft.y + edgeLift, fromLeft.z},
               Vector3{fromRight.x, fromRight.y + edgeLift, fromRight.z}, edgeColor);
    DrawLine3D(Vector3{toLeft.x, toLeft.y + edgeLift, toLeft.z},
               Vector3{toRight.x, toRight.y + edgeLift, toRight.z}, edgeColor);
    DrawLine3D(Vector3{fromRight.x, fromRight.y + 0.52f, fromRight.z},
               Vector3{toRight.x, toRight.y + 0.52f, toRight.z}, edgeColor);
    DrawLine3D(Vector3{fromLeft.x, fromLeft.y + 0.52f, fromLeft.z},
               Vector3{toLeft.x, toLeft.y + 0.52f, toLeft.z}, edgeColor);
    DrawLine3D(fromRight, Vector3{fromRight.x, fromRight.y + 0.52f, fromRight.z}, edgeColor);
    DrawLine3D(toRight, Vector3{toRight.x, toRight.y + 0.52f, toRight.z}, edgeColor);
    DrawLine3D(fromLeft, Vector3{fromLeft.x, fromLeft.y + 0.52f, fromLeft.z}, edgeColor);
    DrawLine3D(toLeft, Vector3{toLeft.x, toLeft.y + 0.52f, toLeft.z}, edgeColor);
}

float CrossXZ(const Vector3& first, const Vector3& second) {
    return first.x * second.z - first.z * second.x;
}

Vector3 Subtract(const Vector3& first, const Vector3& second) {
    return Vector3{first.x - second.x, first.y - second.y, first.z - second.z};
}

bool PointLiesInRoadQuad(const Vector3& point, const RoadQuad& road) {
    // All branch/merge arms are flat, convex road quads. A point belongs to
    // the quad when it remains on the same side of every X/Z boundary edge.
    const float tolerance = 0.0001f;
    bool hasPositive = false;
    bool hasNegative = false;
    for (int index = 0; index < 4; ++index) {
        const Vector3& from = road.corners[index];
        const Vector3& to = road.corners[(index + 1) % 4];
        const float cross = CrossXZ(Subtract(to, from), Subtract(point, from));
        if (cross > tolerance) hasPositive = true;
        if (cross < -tolerance) hasNegative = true;
    }
    return !(hasPositive && hasNegative);
}

bool EdgeIntersectionProgress(const Vector3& from, const Vector3& to,
                              const Vector3& otherFrom, const Vector3& otherTo,
                              float& progress) {
    const Vector3 direction = Subtract(to, from);
    const Vector3 otherDirection = Subtract(otherTo, otherFrom);
    const float denominator = CrossXZ(direction, otherDirection);
    if (std::fabs(denominator) < 0.0001f) return false;

    const Vector3 between = Subtract(otherFrom, from);
    const float alongThis = CrossXZ(between, otherDirection) / denominator;
    const float alongOther = CrossXZ(between, direction) / denominator;
    if (alongThis < -0.0001f || alongThis > 1.0001f ||
        alongOther < -0.0001f || alongOther > 1.0001f) return false;
    progress = std::max(0.0f, std::min(1.0f, alongThis));
    return true;
}

bool IsRoadBoundarySegment(const Vector3& from, const Vector3& to, const RoadQuad& road,
                           const RoadQuad& otherRoad) {
    const Vector3 midpoint = Interpolate(from, to, 0.5f);
    Vector3 centre = Vector3{0.0f, 0.0f, 0.0f};
    for (int index = 0; index < 4; ++index) {
        centre.x += road.corners[index].x;
        centre.y += road.corners[index].y;
        centre.z += road.corners[index].z;
    }
    centre.x *= 0.25f;
    centre.y *= 0.25f;
    centre.z *= 0.25f;

    // Probe just outside this arm. If the other arm still contains that
    // point, this edge lies inside their union and must not receive a rail.
    const float outwardX = midpoint.x - centre.x;
    const float outwardZ = midpoint.z - centre.z;
    const float length = std::sqrt(outwardX * outwardX + outwardZ * outwardZ);
    if (length < 0.0001f) return false;
    const Vector3 outside = Vector3{midpoint.x + outwardX / length * 0.001f,
                                    midpoint.y,
                                    midpoint.z + outwardZ / length * 0.001f};
    return !PointLiesInRoadQuad(outside, otherRoad);
}

void AddUniqueProgress(std::vector<float>& progress, float value) {
    const float tolerance = 0.0001f;
    for (std::vector<float>::const_iterator current = progress.begin(); current != progress.end(); ++current) {
        if (std::fabs(*current - value) < tolerance) return;
    }
    progress.push_back(value);
}

void AppendExposedRoadEdge(const RoadQuad& road, int edgeIndex, const RoadQuad& otherRoad,
                           bool hasGuardrail, std::vector<RoadBoundarySegment>& boundary) {
    const Vector3& from = road.corners[edgeIndex];
    const Vector3& to = road.corners[(edgeIndex + 1) % 4];
    std::vector<float> progress;
    progress.push_back(0.0f);
    progress.push_back(1.0f);
    for (int otherEdge = 0; otherEdge < 4; ++otherEdge) {
        float intersection = 0.0f;
        if (EdgeIntersectionProgress(from, to, otherRoad.corners[otherEdge],
                                     otherRoad.corners[(otherEdge + 1) % 4], intersection)) {
            AddUniqueProgress(progress, intersection);
        }
    }
    std::sort(progress.begin(), progress.end());

    for (std::size_t index = 0; index + 1 < progress.size(); ++index) {
        const float begin = progress[index];
        const float end = progress[index + 1];
        if (end - begin < 0.0001f) continue;
        const Vector3 segmentFrom = Interpolate(from, to, begin);
        const Vector3 segmentTo = Interpolate(from, to, end);
        if (!IsRoadBoundarySegment(segmentFrom, segmentTo, road, otherRoad)) continue;
        boundary.push_back(RoadBoundarySegment{segmentFrom, segmentTo, hasGuardrail});
    }
}

bool SamePoint(const Vector3& first, const Vector3& second) {
    const float tolerance = 0.0001f;
    return std::fabs(first.x - second.x) < tolerance && std::fabs(first.y - second.y) < tolerance &&
           std::fabs(first.z - second.z) < tolerance;
}

bool SameRoadSegment(const RoadBoundarySegment& first, const RoadBoundarySegment& second) {
    return (SamePoint(first.from, second.from) && SamePoint(first.to, second.to)) ||
           (SamePoint(first.from, second.to) && SamePoint(first.to, second.from));
}

void AddUniqueBoundarySegment(std::vector<RoadBoundarySegment>& boundary, const RoadBoundarySegment& candidate) {
    for (std::vector<RoadBoundarySegment>::iterator current = boundary.begin(); current != boundary.end(); ++current) {
        if (!SameRoadSegment(*current, candidate)) continue;
        current->hasGuardrail = current->hasGuardrail || candidate.hasGuardrail;
        return;
    }
    boundary.push_back(candidate);
}

std::vector<RoadBoundarySegment> ExposedRoadBoundary(const RoadQuad& first, const RoadQuad& second) {
    std::vector<RoadBoundarySegment> candidates;
    // Edges 0 and 2 run along the road's length; their exposed portions are
    // guardrails. Connector-end edges keep the existing low neon outline but
    // intentionally do not become walls across a join.
    for (int edge = 0; edge < 4; ++edge) {
        AppendExposedRoadEdge(first, edge, second, edge == 0 || edge == 2, candidates);
        AppendExposedRoadEdge(second, edge, first, edge == 0 || edge == 2, candidates);
    }

    std::vector<RoadBoundarySegment> boundary;
    for (std::vector<RoadBoundarySegment>::const_iterator candidate = candidates.begin();
         candidate != candidates.end(); ++candidate) {
        AddUniqueBoundarySegment(boundary, *candidate);
    }
    return boundary;
}

void DrawBoundarySegment(const RoadBoundarySegment& segment, Color edgeColor) {
    const float edgeLift = 0.02f;
    DrawLine3D(AddHeight(segment.from, edgeLift), AddHeight(segment.to, edgeLift), edgeColor);
    if (!segment.hasGuardrail) return;
    DrawLine3D(AddHeight(segment.from, 0.52f), AddHeight(segment.to, 0.52f), edgeColor);
}

void AddUniquePost(std::vector<Vector3>& posts, const Vector3& candidate) {
    for (std::vector<Vector3>::const_iterator post = posts.begin(); post != posts.end(); ++post) {
        if (SamePoint(*post, candidate)) return;
    }
    posts.push_back(candidate);
}

void DrawBranchMergeSurface(const std::vector<TrackPiece>& arms, Color surfaceColor, Color edgeColor) {
    if (arms.size() != 2) return;
    const RoadQuad first = StraightRoadQuad(arms[0]);
    const RoadQuad second = StraightRoadQuad(arms[1]);
    DrawRoadSurface(first, surfaceColor);
    DrawRoadSurface(second, Fade(surfaceColor, 0.88f));

    const std::vector<RoadBoundarySegment> boundary = ExposedRoadBoundary(first, second);
    std::vector<Vector3> posts;
    for (std::vector<RoadBoundarySegment>::const_iterator segment = boundary.begin(); segment != boundary.end(); ++segment) {
        DrawBoundarySegment(*segment, edgeColor);
        if (!segment->hasGuardrail) continue;
        AddUniquePost(posts, segment->from);
        AddUniquePost(posts, segment->to);
    }
    for (std::vector<Vector3>::const_iterator post = posts.begin(); post != posts.end(); ++post) {
        DrawLine3D(*post, AddHeight(*post, 0.52f), edgeColor);
    }
}

} // namespace

void DrawTrackPieceSurface(const TrackPiece& piece, Color surfaceColor, Color edgeColor) {
    if (piece.type == TrackPieceType::Branch) {
        DrawBranchMergeSurface(TrackRoadGeometry::PhysicalRoadArms(piece), surfaceColor, edgeColor);
        return;
    }
    if (piece.type == TrackPieceType::Merge) {
        DrawBranchMergeSurface(TrackRoadGeometry::PhysicalRoadArms(piece), surfaceColor, edgeColor);
        return;
    }
    if (piece.type == TrackPieceType::Straight) {
        DrawStraightSurface(piece, surfaceColor, edgeColor);
        return;
    }
    const std::vector<TrackSurfaceSample> samples = piece.SurfaceSamples();
    if (samples.size() < 2) return;

    rlBegin(RL_QUADS);
    SetColor(surfaceColor);
    for (std::size_t index = 0; index + 1 < samples.size(); ++index) {
        const TrackSurfaceSample& from = samples[index];
        const TrackSurfaceSample& to = samples[index + 1];
        const Vector3 fromRight = SurfaceRight(from);
        const Vector3 toRight = SurfaceRight(to);
        rlVertex3f(from.x + fromRight.x * from.halfWidth, from.y + fromRight.y * from.halfWidth + 0.08f,
                   from.z + fromRight.z * from.halfWidth);
        rlVertex3f(to.x + toRight.x * to.halfWidth, to.y + toRight.y * to.halfWidth + 0.08f,
                   to.z + toRight.z * to.halfWidth);
        rlVertex3f(to.x - toRight.x * to.halfWidth, to.y - toRight.y * to.halfWidth + 0.08f,
                   to.z - toRight.z * to.halfWidth);
        rlVertex3f(from.x - fromRight.x * from.halfWidth, from.y - fromRight.y * from.halfWidth + 0.08f,
                   from.z - fromRight.z * from.halfWidth);
    }
    rlEnd();

    for (std::size_t index = 0; index + 1 < samples.size(); ++index) {
        const TrackSurfaceSample& from = samples[index];
        const TrackSurfaceSample& to = samples[index + 1];
        const Vector3 fromRight = SurfaceRight(from);
        const Vector3 toRight = SurfaceRight(to);
        DrawLine3D(Vector3{from.x + fromRight.x * from.halfWidth, from.y + fromRight.y * from.halfWidth + 0.10f,
                           from.z + fromRight.z * from.halfWidth},
                   Vector3{to.x + toRight.x * to.halfWidth, to.y + toRight.y * to.halfWidth + 0.10f,
                           to.z + toRight.z * to.halfWidth}, edgeColor);
        DrawLine3D(Vector3{from.x - fromRight.x * from.halfWidth, from.y - fromRight.y * from.halfWidth + 0.10f,
                           from.z - fromRight.z * from.halfWidth},
                   Vector3{to.x - toRight.x * to.halfWidth, to.y - toRight.y * to.halfWidth + 0.10f,
                           to.z - toRight.z * to.halfWidth}, edgeColor);
        DrawLine3D(Vector3{from.x + fromRight.x * from.halfWidth + from.normalX * 0.55f,
                           from.y + fromRight.y * from.halfWidth + from.normalY * 0.55f,
                           from.z + fromRight.z * from.halfWidth + from.normalZ * 0.55f},
                   Vector3{to.x + toRight.x * to.halfWidth + to.normalX * 0.55f,
                           to.y + toRight.y * to.halfWidth + to.normalY * 0.55f,
                           to.z + toRight.z * to.halfWidth + to.normalZ * 0.55f}, edgeColor);
        DrawLine3D(Vector3{from.x - fromRight.x * from.halfWidth + from.normalX * 0.55f,
                           from.y - fromRight.y * from.halfWidth + from.normalY * 0.55f,
                           from.z - fromRight.z * from.halfWidth + from.normalZ * 0.55f},
                   Vector3{to.x - toRight.x * to.halfWidth + to.normalX * 0.55f,
                           to.y - toRight.y * to.halfWidth + to.normalY * 0.55f,
                           to.z - toRight.z * to.halfWidth + to.normalZ * 0.55f}, edgeColor);
        if (index % 3 == 0) {
            DrawLine3D(Vector3{from.x + fromRight.x * from.halfWidth, from.y + fromRight.y * from.halfWidth,
                               from.z + fromRight.z * from.halfWidth},
                       Vector3{from.x + fromRight.x * from.halfWidth + from.normalX * 0.55f,
                               from.y + fromRight.y * from.halfWidth + from.normalY * 0.55f,
                               from.z + fromRight.z * from.halfWidth + from.normalZ * 0.55f}, edgeColor);
            DrawLine3D(Vector3{from.x - fromRight.x * from.halfWidth, from.y - fromRight.y * from.halfWidth,
                               from.z - fromRight.z * from.halfWidth},
                       Vector3{from.x - fromRight.x * from.halfWidth + from.normalX * 0.55f,
                               from.y - fromRight.y * from.halfWidth + from.normalY * 0.55f,
                               from.z - fromRight.z * from.halfWidth + from.normalZ * 0.55f}, edgeColor);
        }
    }
}
