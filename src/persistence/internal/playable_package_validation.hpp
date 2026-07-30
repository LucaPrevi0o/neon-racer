#pragma once

#include <string>

struct PlayableTrack;

namespace PlayablePackageValidation {

bool HasLineBreak(const std::string& value);
bool HasUnsupportedControlCharacter(const std::string& value);
bool Validate(const PlayableTrack& playable, std::string& error);

} // namespace PlayablePackageValidation
