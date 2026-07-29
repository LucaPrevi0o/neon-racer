#pragma once

#include <cstdint>
#include <string>

// Shared contracts for later editor, renderer, physics, and persistence work.
// They use no Raylib types, keeping the track domain independently testable.
enum class SurfaceMaterial {
    Regular,
    Slippery,
    HighResistance,
};

struct TrackSurfaceSample {
    float x;
    float y;
    float z;
    float tangentX;
    float tangentY;
    float tangentZ;
    float normalX;
    float normalY;
    float normalZ;
    float halfWidth;
    SurfaceMaterial material;
    std::uint32_t pieceId;
};

struct TrackContact {
    bool found;
    TrackSurfaceSample surface;
    float distance;
    bool guardrailHit;
};

struct TrackMetadata {
    std::string name;
    std::string creator;
    std::string description;
    std::uint32_t playableExportVersion;

    TrackMetadata() : playableExportVersion(0) {}
};

struct VerificationState {
    bool hasSavedGhost;
    bool isVerifiedForPlayableExport;
    std::uint32_t verifiedLayoutRevision;

    VerificationState()
        : hasSavedGhost(false), isVerifiedForPlayableExport(false), verifiedLayoutRevision(0) {}
};
