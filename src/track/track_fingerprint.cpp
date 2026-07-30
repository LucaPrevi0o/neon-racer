#include "track_fingerprint.hpp"

#include "track.hpp"

#include <cstdint>
#include <vector>

namespace {

const std::uint64_t kFnvOffsetBasis = 14695981039346656037ull;
const std::uint64_t kFnvPrime = 1099511628211ull;

void AppendByte(std::uint64_t& value, std::uint8_t byte) {
    value ^= static_cast<std::uint64_t>(byte);
    value *= kFnvPrime;
}

void AppendUnsigned(std::uint64_t& value, std::uint64_t field) {
    // Write numeric fields in an explicit byte order so the result does not
    // depend on the host architecture's endianness.
    for (int byte = 0; byte < 8; ++byte) {
        AppendByte(value, static_cast<std::uint8_t>((field >> (byte * 8)) & 0xffu));
    }
}

void AppendSigned(std::uint64_t& value, int field) {
    AppendUnsigned(value, static_cast<std::uint64_t>(static_cast<std::int64_t>(field)));
}

void AppendText(std::uint64_t& value, const char* text) {
    for (; *text != '\0'; ++text) AppendByte(value, static_cast<std::uint8_t>(*text));
    // Terminate fields explicitly, rather than relying on adjacent text not
    // being ambiguous when the schema evolves.
    AppendByte(value, 0u);
}

void AppendPiece(std::uint64_t& value, const TrackPiece& piece) {
    AppendSigned(value, static_cast<int>(piece.type));
    AppendSigned(value, piece.entryPosition.x);
    AppendSigned(value, piece.entryPosition.y);
    AppendSigned(value, piece.entryPosition.z);
    AppendSigned(value, static_cast<int>(piece.entryHeading));
    AppendSigned(value, piece.width);
    AppendSigned(value, piece.exitWidth);
    AppendSigned(value, piece.length);
    AppendSigned(value, static_cast<int>(piece.curveTurn));
    AppendSigned(value, piece.curveRadius);
    AppendSigned(value, piece.curveDegrees);
    AppendSigned(value, piece.bankAngleDegrees);
    AppendSigned(value, piece.elevationDelta);
    AppendSigned(value, piece.lateralOffset);
    AppendSigned(value, static_cast<int>(piece.material));
}

} // namespace

namespace TrackFingerprint {

std::uint64_t Calculate(const Track& track) {
    std::uint64_t fingerprint = kFnvOffsetBasis;
    AppendText(fingerprint, "NEON_RACER_TRACK_LAYOUT_FINGERPRINT_V1");

    const std::vector<TrackPiece>& pieces = track.Pieces();
    AppendUnsigned(fingerprint, static_cast<std::uint64_t>(pieces.size()));

    // IDs are regenerated when a serialized layout is reconstructed. Store
    // the selected start/finish component's stable ordinal instead.
    std::uint64_t startOrdinal = static_cast<std::uint64_t>(pieces.size());
    const std::uint32_t startPieceId = track.StartFinishPieceId();
    for (std::size_t index = 0; index < pieces.size(); ++index) {
        if (pieces[index].id == startPieceId) {
            startOrdinal = static_cast<std::uint64_t>(index);
            break;
        }
    }
    AppendUnsigned(fingerprint, startOrdinal);
    AppendSigned(fingerprint, static_cast<int>(track.SelectedRaceDirection()));

    for (std::vector<TrackPiece>::const_iterator piece = pieces.begin(); piece != pieces.end(); ++piece) {
        AppendPiece(fingerprint, *piece);
    }
    return fingerprint;
}

} // namespace TrackFingerprint
