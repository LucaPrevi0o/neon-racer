#include "track_renderer.hpp"

#include <cmath>

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

void DrawStraightSurface(const TrackPiece& piece, Color surfaceColor, Color edgeColor) {
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

    rlBegin(RL_QUADS);
    SetColor(surfaceColor);
    rlVertex3f(fromRight.x, fromRight.y, fromRight.z);
    rlVertex3f(toRight.x, toRight.y, toRight.z);
    rlVertex3f(toLeft.x, toLeft.y, toLeft.z);
    rlVertex3f(fromLeft.x, fromLeft.y, fromLeft.z);
    rlEnd();

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

} // namespace

void DrawTrackPieceSurface(const TrackPiece& piece, Color surfaceColor, Color edgeColor) {
    if (piece.type == TrackPieceType::Branch) {
        TrackPiece rightArm = piece;
        rightArm.type = TrackPieceType::Straight;
        TrackPiece leftArm = rightArm;
        leftArm.lateralOffset = -leftArm.lateralOffset;
        DrawStraightSurface(rightArm, surfaceColor, edgeColor);
        DrawStraightSurface(leftArm, Fade(surfaceColor, 0.88f), edgeColor);
        return;
    }
    if (piece.type == TrackPieceType::Merge) {
        const std::vector<TrackConnector> entries = piece.EntryConnectors();
        for (std::size_t index = 0; index < entries.size(); ++index) {
            TrackPiece arm = piece;
            arm.type = TrackPieceType::Straight;
            arm.entryPosition = entries[index].position;
            arm.lateralOffset = index == 0 ? -std::abs(piece.lateralOffset) : std::abs(piece.lateralOffset);
            DrawStraightSurface(arm, Fade(surfaceColor, index == 0 ? 1.0f : 0.88f), edgeColor);
        }
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
        const Vector3 fromRight = Vector3{from.tangentY * from.normalZ - from.tangentZ * from.normalY,
                                          from.tangentZ * from.normalX - from.tangentX * from.normalZ,
                                          from.tangentX * from.normalY - from.tangentY * from.normalX};
        const Vector3 toRight = Vector3{to.tangentY * to.normalZ - to.tangentZ * to.normalY,
                                        to.tangentZ * to.normalX - to.tangentX * to.normalZ,
                                        to.tangentX * to.normalY - to.tangentY * to.normalX};
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
        const Vector3 fromRight = Vector3{from.tangentY * from.normalZ - from.tangentZ * from.normalY,
                                          from.tangentZ * from.normalX - from.tangentX * from.normalZ,
                                          from.tangentX * from.normalY - from.tangentY * from.normalX};
        const Vector3 toRight = Vector3{to.tangentY * to.normalZ - to.tangentZ * to.normalY,
                                        to.tangentZ * to.normalX - to.tangentX * to.normalZ,
                                        to.tangentX * to.normalY - to.tangentY * to.normalX};
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
