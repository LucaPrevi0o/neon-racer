#pragma once

#include <string>

#include "playable_track.hpp"

namespace PlayableExport {

// Creates the transient race-preview snapshot used by Tab. It deliberately
// does not require or claim a verified replay, so a new layout can be driven
// for the first time.
bool BuildPreview(const Track& draft, const std::string& name, PlayableTrack& playable, std::string& error);

// Builds a package suitable for persistent storage. The caller supplies the
// next export version and a verified ghost from the exact unmodified layout.
bool BuildVerified(const Track& draft, const TrackMetadata& metadata, const VerifiedGhostData& verificationGhost,
                   const VerificationState& verification, PlayableTrack& playable, std::string& error);

} // namespace PlayableExport
