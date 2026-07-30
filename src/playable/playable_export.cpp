#include "playable_export.hpp"

#include "../race/ghost_replay.hpp"
#include "../track/track_fingerprint.hpp"

#include <cctype>

namespace {

bool HasVisibleText(const std::string& value) {
    for (std::string::const_iterator character = value.begin(); character != value.end(); ++character) {
        if (!std::isspace(static_cast<unsigned char>(*character))) return true;
    }
    return false;
}

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

bool IsValidMetadata(const TrackMetadata& metadata, std::string& error) {
    if (!HasVisibleText(metadata.name) || !HasVisibleText(metadata.creator) || !HasVisibleText(metadata.description)) {
        error = "A playable export needs a name, creator, and description.";
        return false;
    }
    if (HasLineBreak(metadata.name) || HasLineBreak(metadata.creator) || HasLineBreak(metadata.description) ||
        HasUnsupportedControlCharacter(metadata.name) || HasUnsupportedControlCharacter(metadata.creator) ||
        HasUnsupportedControlCharacter(metadata.description)) {
        error = "Playable metadata must use printable single-line text.";
        return false;
    }
    if (metadata.playableExportVersion == 0) {
        error = "A playable export needs a positive version.";
        return false;
    }
    return true;
}

} // namespace

namespace PlayableExport {

bool BuildPreview(const Track& draft, const std::string& name, PlayableTrack& playable, std::string& error) {
    if (!draft.Validate().raceReady) {
        error = "Only a race-ready layout with a start/finish line can be previewed.";
        return false;
    }

    PlayableTrack preview;
    preview.layout = draft;
    preview.metadata.name = name.empty() ? "Untitled track" : name;
    preview.layoutFingerprint = TrackFingerprint::Calculate(draft);
    playable = preview;
    return true;
}

bool BuildVerified(const Track& draft, const TrackMetadata& metadata, const VerifiedGhostData& verificationGhost,
                   const VerificationState& verification, PlayableTrack& playable, std::string& error) {
    if (!draft.Validate().raceReady) {
        error = "Only a race-ready layout with a start/finish line can be exported.";
        return false;
    }
    if (!IsValidMetadata(metadata, error)) return false;
    const std::uint64_t fingerprint = TrackFingerprint::Calculate(draft);
    if (!verification.hasSavedGhost || !verification.isVerifiedForPlayableExport ||
        verification.verifiedLayoutRevision != draft.LayoutRevision() ||
        verification.verifiedLayoutFingerprint != fingerprint) {
        error = "Complete a verified three-lap run on this unchanged layout before exporting it.";
        return false;
    }
    if (!GhostReplay::IsValidVerifiedData(verificationGhost)) {
        error = "The verified ghost data is incomplete or invalid.";
        return false;
    }

    if (verificationGhost.layoutFingerprint != fingerprint) {
        error = "The verified ghost belongs to a different track layout.";
        return false;
    }

    PlayableTrack verified;
    verified.layout = draft;
    verified.metadata = metadata;
    verified.layoutFingerprint = fingerprint;
    verified.verificationGhost = verificationGhost;
    playable = verified;
    return true;
}

} // namespace PlayableExport
