#pragma once

#include <cstdint>

#include "../race/race_contracts.hpp"
#include "../track/track.hpp"

// Frozen, cross-domain content used by a saved playable time trial. It
// intentionally lives outside src/track: the layout remains Raylib-free and
// reusable, while this package additionally carries race verification data.
// A saved file is never edited in place; re-exporting a matching name creates
// a new snapshot that atomically replaces that file's current version.
struct PlayableTrack {
    Track layout;
    TrackMetadata metadata;
    // Durable identity for the frozen layout stored alongside its replay.
    std::uint64_t layoutFingerprint;
    VerifiedGhostData verificationGhost;

    PlayableTrack() : layoutFingerprint(0) {}
};
