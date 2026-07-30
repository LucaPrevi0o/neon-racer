#pragma once

#include <cstdint>

class Track;

// Produces a stable, non-cryptographic identity for a track's persisted
// layout. It deliberately ignores generated piece IDs and the in-memory
// layout revision, while retaining piece order, every persisted piece field,
// the selected start/finish piece ordinal, and race direction.
namespace TrackFingerprint {

std::uint64_t Calculate(const Track& track);

// Reproduces the v5 fingerprint representation while importing a flat Twist
// layout. Version-5 writers persisted that component's unused radius as zero.
std::uint64_t CalculateLegacyFlatTwist(const Track& track);

} // namespace TrackFingerprint
