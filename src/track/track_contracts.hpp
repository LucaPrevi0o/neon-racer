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
    // Twists are enclosed corkscrew road frames. This flag allows vehicle
    // dynamics to apply its bounded guide contact only inside that component;
    // ordinary roads and vertical loops retain one-sided suspension behavior.
    bool twistGuide;
    // Signed distance from the sampled road centerline along its local side
    // axis, before the query clamps a guardrail contact to the road edge.
    float lateralOffset;
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
    std::uint64_t verifiedLayoutFingerprint;

    VerificationState()
        : hasSavedGhost(false), isVerifiedForPlayableExport(false), verifiedLayoutRevision(0),
          verifiedLayoutFingerprint(0) {}
};
