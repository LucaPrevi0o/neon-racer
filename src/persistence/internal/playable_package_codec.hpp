#pragma once

#include <iosfwd>
#include <string>

struct PlayableTrack;

namespace PlayablePackageCodec {

bool Write(std::ostream& output, const PlayableTrack& playable, std::string& error);
bool Read(std::istream& input, PlayableTrack& playable, std::string& error);

} // namespace PlayablePackageCodec
