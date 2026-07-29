#include "track.hpp"

#include <algorithm>
#include <cmath>

namespace {

const float kPi = 3.14159265359f;

GridPosition Add(GridPosition position, int dx, int dz) {
    position.x += dx;
    position.z += dz;
    return position;
}

GridPosition Forward(Heading heading) {
    switch (heading) {
    case Heading::North: return GridPosition{0, 0, -1};
    case Heading::East: return GridPosition{1, 0, 0};
    case Heading::South: return GridPosition{0, 0, 1};
    case Heading::West: return GridPosition{-1, 0, 0};
    }
    return GridPosition{0, 0, 0};
}

Heading Turn(Heading heading, CurveTurn turn) {
    const int direction = turn == CurveTurn::Right ? 1 : 3;
    return static_cast<Heading>((static_cast<int>(heading) + direction) % 4);
}

GridPosition Right(Heading heading) {
    return Forward(Turn(heading, CurveTurn::Right));
}

} // namespace

bool GridPosition::operator==(const GridPosition& other) const {
    return x == other.x && y == other.y && z == other.z;
}

bool GridPosition::operator<(const GridPosition& other) const {
    if (x != other.x) return x < other.x;
    if (y != other.y) return y < other.y;
    return z < other.z;
}

TrackConnector TrackPiece::EntryConnector() const {
    if (type == TrackPieceType::Merge) return EntryConnectors().front();
    return TrackConnector{entryPosition, entryHeading, width};
}

TrackConnector TrackPiece::ExitConnector() const {
    if (type == TrackPieceType::Branch) return ExitConnectors().front();
    if (type == TrackPieceType::Merge) {
        const GridPosition forward = Forward(entryHeading);
        GridPosition exit = Add(entryPosition, forward.x * length, forward.z * length);
        exit.y += elevationDelta;
        return TrackConnector{exit, entryHeading, exitWidth};
    }
    if (type == TrackPieceType::Straight || type == TrackPieceType::Twist) {
        const GridPosition forward = Forward(entryHeading);
        const GridPosition side = Right(entryHeading);
        GridPosition exit = Add(entryPosition, forward.x * length + side.x * lateralOffset,
                                forward.z * length + side.z * lateralOffset);
        exit.y += elevationDelta;
        return TrackConnector{exit, entryHeading, exitWidth};
    }

    if (type == TrackPieceType::Loop) {
        GridPosition exit = entryPosition;
        const GridPosition side = Right(entryHeading);
        exit.x += side.x * lateralOffset;
        exit.z += side.z * lateralOffset;
        exit.y += elevationDelta;
        return TrackConnector{exit, entryHeading, exitWidth};
    }

    const GridPosition forward = Forward(entryHeading);
    const GridPosition side = Right(entryHeading);
    const float sideSign = curveTurn == CurveTurn::Right ? 1.0f : -1.0f;
    // Arc progress is always forward along the entry heading. The turn side
    // only changes the lateral displacement; signing the angle made left
    // curves exit behind their entry, unlike their rendered path.
    const float angle = static_cast<float>(curveDegrees) * kPi / 180.0f;
    GridPosition exit = Add(entryPosition,
                            static_cast<int>(std::round(static_cast<float>(forward.x) * curveRadius * std::sin(angle) +
                                                        static_cast<float>(side.x) * curveRadius * (1.0f - std::cos(angle)) * sideSign)),
                            static_cast<int>(std::round(static_cast<float>(forward.z) * curveRadius * std::sin(angle) +
                                                        static_cast<float>(side.z) * curveRadius * (1.0f - std::cos(angle)) * sideSign)));
    exit.y += elevationDelta;
    const int quarterTurns = (curveDegrees / 90) % 4;
    const int signedTurns = curveTurn == CurveTurn::Right ? quarterTurns : (4 - quarterTurns) % 4;
    const Heading exitHeading = static_cast<Heading>((static_cast<int>(entryHeading) + signedTurns) % 4);
    return TrackConnector{exit, exitHeading, exitWidth};
}

std::vector<TrackConnector> TrackPiece::EntryConnectors() const {
    if (type == TrackPieceType::Merge) {
        const GridPosition side = Right(entryHeading);
        GridPosition right = Add(entryPosition, side.x * std::abs(lateralOffset),
                                 side.z * std::abs(lateralOffset));
        GridPosition left = Add(entryPosition, -side.x * std::abs(lateralOffset),
                                -side.z * std::abs(lateralOffset));
        return std::vector<TrackConnector>{TrackConnector{right, entryHeading, width}, TrackConnector{left, entryHeading, width}};
    }
    return std::vector<TrackConnector>(1, EntryConnector());
}

std::vector<TrackConnector> TrackPiece::ExitConnectors() const {
    if (type == TrackPieceType::Branch) {
        const GridPosition forward = Forward(entryHeading);
        const GridPosition side = Right(entryHeading);
        const GridPosition centre = Add(entryPosition, forward.x * length, forward.z * length);
        const int spread = std::abs(lateralOffset);
        GridPosition right = Add(centre, side.x * spread, side.z * spread);
        GridPosition left = Add(centre, -side.x * spread, -side.z * spread);
        right.y += elevationDelta;
        left.y += elevationDelta;
        return std::vector<TrackConnector>{TrackConnector{right, entryHeading, exitWidth},
                                           TrackConnector{left, entryHeading, exitWidth}};
    }
    return std::vector<TrackConnector>(1, ExitConnector());
}

std::vector<GridPosition> TrackPiece::CenterlineCells() const {
    std::vector<GridPosition> cells;
    cells.push_back(entryPosition);

    if (type == TrackPieceType::Straight || type == TrackPieceType::Twist) {
        const GridPosition forward = Forward(entryHeading);
        const GridPosition side = Right(entryHeading);
        for (int step = 1; step <= length; ++step) {
            const float progress = static_cast<float>(step) / static_cast<float>(length);
            GridPosition cell = Add(entryPosition,
                                    static_cast<int>(std::round(static_cast<float>(forward.x * step) +
                                                                static_cast<float>(side.x * lateralOffset) * progress)),
                                    static_cast<int>(std::round(static_cast<float>(forward.z * step) +
                                                                static_cast<float>(side.z * lateralOffset) * progress)));
            cell.y += static_cast<int>(std::round(static_cast<float>(elevationDelta) * progress));
            cells.push_back(cell);
        }
        return cells;
    }

    const std::vector<TrackPathPoint> points = PathPoints(type == TrackPieceType::Loop ? std::max(24, curveRadius * 12) :
        std::max(12, curveRadius * curveDegrees / 15));
    for (std::vector<TrackPathPoint>::const_iterator point = points.begin(); point != points.end(); ++point) {
        const GridPosition cell = GridPosition{static_cast<int>(std::round(point->x)),
                                               static_cast<int>(std::round(point->y)),
                                               static_cast<int>(std::round(point->z))};
        if (cells.empty() || !(cells.back() == cell)) cells.push_back(cell);
    }
    return cells;
}

std::vector<TrackPathPoint> TrackPiece::PathPoints(int curveSubdivisions) const {
    std::vector<TrackPathPoint> points;
    if (type == TrackPieceType::Straight || type == TrackPieceType::Twist) {
        const TrackConnector entry = EntryConnector();
        const TrackConnector exit = ExitConnector();
        const int segments = curveSubdivisions > 0 ? curveSubdivisions : 1;
        for (int index = 0; index <= segments; ++index) {
            const float progress = static_cast<float>(index) / static_cast<float>(segments);
            points.push_back(TrackPathPoint{
                static_cast<float>(entry.position.x) +
                    (static_cast<float>(exit.position.x) - static_cast<float>(entry.position.x)) * progress,
                static_cast<float>(entry.position.y) +
                    (static_cast<float>(exit.position.y) - static_cast<float>(entry.position.y)) * progress,
                static_cast<float>(entry.position.z) +
                    (static_cast<float>(exit.position.z) - static_cast<float>(entry.position.z)) * progress});
        }
        return points;
    }

    if (type == TrackPieceType::Loop) {
        const int segments = curveSubdivisions > 0 ? curveSubdivisions : std::max(24, curveRadius * 12);
        const GridPosition forward = Forward(entryHeading);
        const GridPosition side = Right(entryHeading);
        for (int index = 0; index <= segments; ++index) {
            const float progress = static_cast<float>(index) / static_cast<float>(segments);
            const float angle = 2.0f * kPi * progress;
            points.push_back(TrackPathPoint{
                static_cast<float>(entryPosition.x) + static_cast<float>(forward.x) * curveRadius * std::sin(angle),
                static_cast<float>(entryPosition.y) + curveRadius * (1.0f - std::cos(angle)) +
                    static_cast<float>(elevationDelta) * progress,
                static_cast<float>(entryPosition.z) + static_cast<float>(forward.z) * curveRadius * std::sin(angle) +
                    static_cast<float>(side.z * lateralOffset) * progress});
            points.back().x += static_cast<float>(side.x * lateralOffset) * progress;
        }
        return points;
    }

    const int segments = curveSubdivisions > 0 ? curveSubdivisions :
        std::max(12, curveRadius * curveDegrees / 15);
    const GridPosition forward = Forward(entryHeading);
    const GridPosition side = Right(entryHeading);
    const float sideSign = curveTurn == CurveTurn::Right ? 1.0f : -1.0f;
    for (int index = 0; index <= segments; ++index) {
        const float angle = (static_cast<float>(curveDegrees) * kPi / 180.0f * static_cast<float>(index)) /
            static_cast<float>(segments);
        const float forwardDistance = static_cast<float>(curveRadius) * std::sin(angle);
        const float sideDistance = static_cast<float>(curveRadius) * (1.0f - std::cos(angle)) * sideSign;
        points.push_back(TrackPathPoint{
            static_cast<float>(entryPosition.x) + static_cast<float>(forward.x) * forwardDistance +
                static_cast<float>(side.x) * sideDistance,
                static_cast<float>(entryPosition.y) + static_cast<float>(elevationDelta) *
                    static_cast<float>(index) / static_cast<float>(segments),
            static_cast<float>(entryPosition.z) + static_cast<float>(forward.z) * forwardDistance +
                static_cast<float>(side.z) * sideDistance});
    }
    return points;
}
