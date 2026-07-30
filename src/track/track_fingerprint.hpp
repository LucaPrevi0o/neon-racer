#pragma once

#include <cstdint>

class Track;

// Produces a stable, non-cryptographic identity for a track's persisted
// layout. It deliberately ignores generated piece IDs and the in-memory
// layout revision, while retaining piece order, every persisted piece field,
// the selected start/finish piece ordinal, and race direction.
namespace TrackFingerprint {

std::uint64_t Calculate(const Track& track);

} // namespace TrackFingerprint
