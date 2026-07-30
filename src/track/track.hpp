#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "track_contracts.hpp"

// The editor's world is intentionally large, but a finite envelope keeps
// connector and road-geometry arithmetic safely inside a signed int. These
// are domain invariants rather than viewport limits: ordinary editor movement
// and persisted layouts may use any value in this range.
namespace TrackLimits {

const int kMaximumGridCoordinate = 1000000;
const int kMaximumElevationDelta = 1000000;

// A Twist is a one-turn corkscrew. Its forward run and loop radius are
// controlled separately, allowing compact, tuned variants without changing
// its connector layout.
const int kMinimumTwistLength = 15;
const int kDefaultTwistLength = 25;
const int kMaximumTwistLength = 80;
// `TrackPiece::curveRadius` is shared by curves, loops, and twists. Zero is a
// Twist-only auto-radius representation: resolve it from the run length.
const float kMinimumTwistRadius = 3.0f;
const float kDefaultTwistRadius = 3.0f;
const float kMaximumTwistRadius = 10.0f;
// Used only when constructing a new Twist without an explicit radius. It is
// resolved before a TrackPiece is stored; zero remains a separate auto-radius
// representation for non-flat Twists.
const int kAutomaticTwistRadius = -1;
// Version 5 and earlier stored Twist components as a flat rolling road. New
// layouts retain an explicit radius, so this persisted sentinel keeps an old
// layout's path and connector geometry intact when it is loaded and re-saved.
const int kLegacyFlatTwistRadius = -2;

inline bool IsLegacyFlatTwistRadius(int curveRadius) {
    return curveRadius == kLegacyFlatTwistRadius;
}

inline int MinimumTwistLengthForRoadWidth(int entryWidth, int exitWidth) {
    const int widestRoad = entryWidth > exitWidth ? entryWidth : exitWidth;
    const int widthDrivenMinimum = widestRoad * 5;
    return widthDrivenMinimum > kMinimumTwistLength ? widthDrivenMinimum : kMinimumTwistLength;
}

inline int MinimumTwistRadiusForRoadWidth(int entryWidth, int exitWidth) {
    const int widestRoad = entryWidth > exitWidth ? entryWidth : exitWidth;
    const int widthDrivenMinimum = widestRoad / 2 + 1;
    const int globalMinimum = static_cast<int>(kMinimumTwistRadius);
    return widthDrivenMinimum > globalMinimum ? widthDrivenMinimum : globalMinimum;
}

inline int DefaultTwistRadiusForRoadWidth(int entryWidth, int exitWidth) {
    const int defaultRadius = static_cast<int>(kDefaultTwistRadius);
    const int minimum = MinimumTwistRadiusForRoadWidth(entryWidth, exitWidth);
    return defaultRadius > minimum ? defaultRadius : minimum;
}

inline float TwistRadiusForLength(int length) {
    const float lengthDrivenRadius = static_cast<float>(length) / 8.0f;
    return lengthDrivenRadius > kMinimumTwistRadius ? lengthDrivenRadius : kMinimumTwistRadius;
}

// A zero radius represents a non-flat Twist whose loop radius is derived from
// its run length. The older flat Twist format uses kLegacyFlatTwistRadius.
inline float ResolveTwistRadius(int length, int curveRadius) {
    return curveRadius == 0 ? TwistRadiusForLength(length) : static_cast<float>(curveRadius);
}

inline bool IsValidTwistRadius(int curveRadius) {
    return IsLegacyFlatTwistRadius(curveRadius) || curveRadius == 0 ||
           (static_cast<float>(curveRadius) >= kMinimumTwistRadius &&
            static_cast<float>(curveRadius) <= kMaximumTwistRadius);
}

inline bool IsValidTwistRadius(int curveRadius, int entryWidth, int exitWidth) {
    return IsLegacyFlatTwistRadius(curveRadius) || curveRadius == 0 ||
           (curveRadius >= MinimumTwistRadiusForRoadWidth(entryWidth, exitWidth) &&
            static_cast<float>(curveRadius) <= kMaximumTwistRadius);
}

// Blend the angular motion at each connector so a Twist meets a straight with
// a forward-facing tangent, while keeping the central corkscrew gentle.
inline float TwistAngle(float progress) {
    const float kPi = 3.14159265359f;
    const float blend = 0.15f;
    const float rate = 2.0f * kPi / (1.0f - blend);
    if (progress <= 0.0f) return 0.0f;
    if (progress >= 1.0f) return 2.0f * kPi;
    if (progress < blend) {
        return rate * 0.5f * (progress - blend / kPi * std::sin(kPi * progress / blend));
    }
    if (progress > 1.0f - blend) return 2.0f * kPi - TwistAngle(1.0f - progress);
    return rate * (progress - blend * 0.5f);
}

} // namespace TrackLimits

// All layout data is deliberately integer based. One unit is one metre in the
// editor grid, so saved layouts never accumulate floating-point placement drift.
struct GridPosition {
    int x;
    int y;
    int z;

    bool operator==(const GridPosition& other) const;
    bool operator<(const GridPosition& other) const;
};

enum class Heading {
    North,
    East,
    South,
    West,
};

enum class TrackPieceType {
    Straight,
    Curve,
    Loop,
    Twist,
    Branch,
    Merge,
};

enum class CurveTurn {
    Left,
    Right,
};

enum class RaceDirection {
    Forward,
    Reverse,
};

struct TrackConnector {
    GridPosition position;
    Heading heading;
    int width;
};

struct TrackPathPoint {
    float x;
    float y;
    float z;
};

struct TrackPiece {
    std::uint32_t id;
    TrackPieceType type;
    GridPosition entryPosition;
    Heading entryHeading;
    // `width` is the entry width retained for compatibility with milestone 1.
    // `exitWidth` makes a piece a linear road-width transition when different.
    int width;
    int exitWidth;
    int length;
    CurveTurn curveTurn;
    int curveRadius;
    int curveDegrees;
    int bankAngleDegrees;
    int elevationDelta;
    int lateralOffset;
    SurfaceMaterial material;

    TrackConnector EntryConnector() const;
    TrackConnector ExitConnector() const;
    // Current components expose one connector at each end. Branch/merge
    // components will return multiple entries or exits through this API.
    std::vector<TrackConnector> EntryConnectors() const;
    std::vector<TrackConnector> ExitConnectors() const;
    std::vector<GridPosition> CenterlineCells() const;
    std::vector<TrackPathPoint> PathPoints(int curveSubdivisions = 0) const;
    std::vector<TrackSurfaceSample> SurfaceSamples(int subdivisions = 0) const;
    float WidthAt(float progress) const;
};

enum class TrackIssueKind {
    DisconnectedEntry,
    DisconnectedExit,
    OverlappingGeometry,
    NotOneClosedLoop,
    MissingStartFinish,
    InvalidStartFinish,
};

struct TrackIssue {
    TrackIssueKind kind;
    std::string message;
    std::vector<std::uint32_t> affectedPieceIds;
};

struct TrackValidation {
    bool raceReady;
    std::vector<TrackIssue> issues;
};

struct TrackConnectorRef {
    std::uint32_t pieceId;
    std::size_t connectorIndex;
};

struct TrackConnection {
    TrackConnectorRef exit;
    TrackConnectorRef entry;
};

class Track {
public:
    Track();

    std::uint32_t AddStraight(GridPosition entry, Heading heading, int length, int width = 5,
                              int exitWidth = -1, int elevationDelta = 0, int lateralOffset = 0,
                              SurfaceMaterial material = SurfaceMaterial::Regular);
    std::uint32_t AddCurve(GridPosition entry, Heading heading, CurveTurn turn, int radius,
                           int width = 5, int exitWidth = -1, int elevationDelta = 0,
                           SurfaceMaterial material = SurfaceMaterial::Regular, int curveDegrees = 90,
                           int bankAngleDegrees = 0);
    std::uint32_t AddLoop(GridPosition entry, Heading heading, int radius, int width = 5,
                          int exitWidth = -1, int elevationDelta = 0,
                          SurfaceMaterial material = SurfaceMaterial::Regular, int lateralOffset = 0);
    std::uint32_t AddTwist(GridPosition entry, Heading heading, int length, int width = 5,
                           int exitWidth = -1, int elevationDelta = 0, int lateralOffset = 0,
                           SurfaceMaterial material = SurfaceMaterial::Regular,
                           int curveRadius = TrackLimits::kAutomaticTwistRadius);
    std::uint32_t AddBranch(GridPosition entry, Heading heading, int length, int armOffset = 3,
                            int width = 5, SurfaceMaterial material = SurfaceMaterial::Regular);
    std::uint32_t AddMerge(GridPosition exit, Heading heading, int length, int armOffset = 3,
                           int width = 5, SurfaceMaterial material = SurfaceMaterial::Regular);
    std::uint32_t Add(const TrackPiece& piece);
    bool RemovePiece(std::uint32_t id);
    void Clear();

    const std::vector<TrackPiece>& Pieces() const;
    const TrackPiece* GetPiece(std::uint32_t id) const;
    bool HasOverlappingGeometry(const TrackPiece& candidate) const;
    bool ReplacePiece(const TrackPiece& replacement);

    bool SetStartFinish(std::uint32_t pieceId, RaceDirection direction);
    bool HasStartFinish() const;
    std::uint32_t StartFinishPieceId() const;
    RaceDirection SelectedRaceDirection() const;
    bool IsLayoutValid() const;
    TrackValidation Validate() const;
    std::vector<TrackConnection> Connections() const;
    TrackContact QuerySurface(float x, float y, float z, float maxDistance = 4.0f) const;
    std::uint32_t LayoutRevision() const;

    static Track CreateSampleCircuit();

private:
    std::uint32_t AddPiece(const TrackPiece& piece);
    const TrackPiece* FindPiece(std::uint32_t id) const;
    static bool IsValidPiece(const TrackPiece& piece);
    void InvalidateValidation();

    std::vector<TrackPiece> pieces_;
    std::uint32_t nextPieceId_;
    std::uint32_t startFinishPieceId_;
    RaceDirection raceDirection_;
    std::uint32_t layoutRevision_;
    mutable bool validationDirty_;
    mutable TrackValidation cachedValidation_;
};

const char* HeadingName(Heading heading);
