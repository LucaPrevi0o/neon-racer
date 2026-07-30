#include "../internal/playable_package_validation.hpp"

#include "../../playable/playable_track.hpp"
#include "../../race/ghost_replay.hpp"
#include "../../track/track_fingerprint.hpp"

namespace {

bool HasVisibleText(const std::string& value) {
    for (std::string::const_iterator character = value.begin(); character != value.end(); ++character) {
        if (*character != ' ' && *character != '\t') return true;
    }
    return false;
}

} // namespace

namespace PlayablePackageValidation {

bool HasLineBreak(const std::string& value) {
    return value.find('\n') != std::string::npos || value.find('\r') != std::string::npos;
}

bool HasUnsupportedControlCharacter(const std::string& value) {
    for (std::string::const_iterator character = value.begin(); character != value.end(); ++character) {
        const unsigned char valueByte = static_cast<unsigned char>(*character);
        if (valueByte < 32u || valueByte == 127u) return true;
    }
    return false;
}

bool Validate(const PlayableTrack& playable, std::string& error) {
    if (!playable.layout.Validate().raceReady) {
        error = "Playable package layout is not race-ready.";
        return false;
    }
    if (!HasVisibleText(playable.metadata.name) || !HasVisibleText(playable.metadata.creator) ||
        !HasVisibleText(playable.metadata.description) || playable.metadata.playableExportVersion == 0 ||
        HasLineBreak(playable.metadata.name) || HasLineBreak(playable.metadata.creator) ||
        HasLineBreak(playable.metadata.description) || HasUnsupportedControlCharacter(playable.metadata.name) ||
        HasUnsupportedControlCharacter(playable.metadata.creator) ||
        HasUnsupportedControlCharacter(playable.metadata.description)) {
        error = "Playable package metadata is incomplete or invalid.";
        return false;
    }
    const std::uint64_t fingerprint = TrackFingerprint::Calculate(playable.layout);
    if (playable.layoutFingerprint != fingerprint || playable.verificationGhost.layoutFingerprint != fingerprint) {
        error = "Playable package verification does not match its layout.";
        return false;
    }
    if (!GhostReplay::IsValidVerifiedData(playable.verificationGhost)) {
        error = "Playable package contains an invalid verification ghost.";
        return false;
    }
    return true;
}

} // namespace PlayablePackageValidation
