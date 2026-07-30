#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>

#include "../track/track.hpp"

// Shared stream codec for the structural Track records used by editable drafts
// and frozen playable packages. It deliberately knows nothing about either
// file's metadata, replay, or storage location.
namespace TrackLayoutCodec {

const int kCurrentVersion = 5;
// Track loading compares every component against existing road geometry. A
// bounded persisted count prevents hostile files from turning that work into a
// pathological quadratic load while remaining far above practical circuits.
const std::size_t kMaximumPieceCount = 128u;

bool Write(std::ostream& output, const Track& track, std::string& error);
bool Read(std::istream& input, int version, Track& track, std::string& error);

} // namespace TrackLayoutCodec
