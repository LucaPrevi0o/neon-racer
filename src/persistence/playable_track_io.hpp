#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../playable/playable_track.hpp"

// Storage for frozen custom time-trial packages. Editable .draft files remain
// exclusively under DraftIO; .nrplay files always contain one verified replay
// for their frozen layout. A same-name re-export atomically replaces the
// package's current version rather than editing its contents in place.
namespace PlayableTrackIO {

const std::size_t kMaximumPackageFileBytes = 64u * 1024u * 1024u;

// The exact discovered path must travel through the library UI unchanged:
// external files are allowed to use names that are not safe export stems.
struct PlayableTrackFile {
    std::string displayName;
    std::string path;
};

std::string CustomPlayablePath(const std::string& name);
bool ListCustomPlayableTracks(std::vector<PlayableTrackFile>& files, std::string& error);
bool NextExportVersion(const std::string& name, std::uint32_t& version, std::string& error);
bool Save(const PlayableTrack& playable, const std::string& path, std::string& error);
bool Load(const std::string& path, PlayableTrack& playable, std::string& error);

} // namespace PlayableTrackIO
