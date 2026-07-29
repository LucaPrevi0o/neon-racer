#pragma once

#include <string>

#include "track.hpp"
#include "track_contracts.hpp"

// A playable track is a frozen, validated copy of an editable draft.  Race
// code only receives this snapshot, never the editor's mutable Track.
struct PlayableTrack {
    Track layout;
    TrackMetadata metadata;
    std::uint32_t sourceLayoutRevision;
};

namespace PlayableExport {

// Builds the in-memory artifact used by the time trial.  Ghost verification is
// deliberately not part of this first export phase.
bool Build(const Track& draft, const std::string& name, PlayableTrack& playable, std::string& error);

} // namespace PlayableExport
