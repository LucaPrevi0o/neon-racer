#include "track_layout_codec.hpp"

#include "format_reading.hpp"

#include <map>
#include <ostream>
#include <istream>

namespace TrackLayoutCodec {

bool Write(std::ostream& output, const Track& track, std::string& error) {
    if (track.Pieces().size() > kMaximumPieceCount) {
        error = "Track layout has too many pieces to persist.";
        return false;
    }
    output << "START " << track.StartFinishPieceId() << " "
           << static_cast<int>(track.SelectedRaceDirection()) << "\n";
    output << "PIECES " << track.Pieces().size() << "\n";
    for (std::vector<TrackPiece>::const_iterator piece = track.Pieces().begin();
         piece != track.Pieces().end(); ++piece) {
        output << "PIECE " << piece->id << " " << static_cast<int>(piece->type) << " "
               << piece->entryPosition.x << " " << piece->entryPosition.y << " " << piece->entryPosition.z << " "
               << static_cast<int>(piece->entryHeading) << " " << piece->width << " " << piece->exitWidth << " "
               << piece->length << " " << static_cast<int>(piece->curveTurn) << " " << piece->curveRadius << " "
               << piece->elevationDelta << " " << piece->lateralOffset << " "
               << static_cast<int>(piece->material) << " " << piece->curveDegrees << " "
               << piece->bankAngleDegrees << "\n";
    }
    if (output) return true;
    error = "Could not finish writing track layout.";
    return false;
}

bool Read(std::istream& input, int version, Track& track, std::string& error) {
    if (version < 1 || version > kCurrentVersion) {
        error = "Track layout format is not supported.";
        return false;
    }

    std::string startLabel;
    std::uint32_t savedStartId = 0;
    int savedDirection = 0;
    if (!FormatReading::ReadToken(input, startLabel) || !(input >> savedStartId >> savedDirection) || startLabel != "START" ||
        (savedDirection != static_cast<int>(RaceDirection::Forward) &&
         savedDirection != static_cast<int>(RaceDirection::Reverse))) {
        error = "Track layout start/finish data is invalid.";
        return false;
    }

    std::string piecesLabel;
    std::size_t pieceCount = 0;
    if (!FormatReading::ReadToken(input, piecesLabel) || !(input >> pieceCount) || piecesLabel != "PIECES") {
        error = "Track layout piece list is missing.";
        return false;
    }
    if (pieceCount > kMaximumPieceCount) {
        error = "Track layout has too many pieces.";
        return false;
    }

    Track loaded;
    std::map<std::uint32_t, std::uint32_t> savedToLoadedId;
    for (std::size_t index = 0; index < pieceCount; ++index) {
        std::string pieceLabel;
        std::uint32_t savedId = 0;
        int type = 0;
        GridPosition entry{0, 0, 0};
        int heading = 0;
        int width = 0;
        int exitWidth = 0;
        int length = 0;
        int turn = 0;
        int radius = 0;
        int elevationDelta = 0;
        int lateralOffset = 0;
        int material = static_cast<int>(SurfaceMaterial::Regular);
        int curveDegrees = 90;
        int bankAngleDegrees = 0;
        if (!FormatReading::ReadToken(input, pieceLabel) ||
            !(input >> savedId >> type >> entry.x >> entry.y >> entry.z >> heading >> width) ||
            savedId == 0 || savedToLoadedId.find(savedId) != savedToLoadedId.end() ||
            pieceLabel != "PIECE" || (type != static_cast<int>(TrackPieceType::Straight) &&
            type != static_cast<int>(TrackPieceType::Curve) && type != static_cast<int>(TrackPieceType::Loop) &&
            type != static_cast<int>(TrackPieceType::Twist) && type != static_cast<int>(TrackPieceType::Branch) &&
            type != static_cast<int>(TrackPieceType::Merge)) ||
            heading < static_cast<int>(Heading::North) || heading > static_cast<int>(Heading::West)) {
            error = "Track layout contains an invalid piece.";
            return false;
        }

        if (version >= 3) {
            if (!(input >> exitWidth >> length >> turn >> radius >> elevationDelta >> lateralOffset >> material) ||
                material < static_cast<int>(SurfaceMaterial::Regular) ||
                material > static_cast<int>(SurfaceMaterial::HighResistance)) {
                error = "Track layout contains invalid advanced piece parameters.";
                return false;
            }
            if (version >= 4 && (!(input >> curveDegrees) ||
                (type == static_cast<int>(TrackPieceType::Curve) &&
                 (curveDegrees != 90 && curveDegrees != 180 && curveDegrees != 270)))) {
                error = "Track layout contains an invalid curve extent.";
                return false;
            }
            if (version == 5 && (!(input >> bankAngleDegrees) || bankAngleDegrees < -45 || bankAngleDegrees > 45)) {
                error = "Track layout contains an invalid curve bank angle.";
                return false;
            }
        } else {
            if (!(input >> length >> turn >> radius)) {
                error = "Track layout contains an invalid piece.";
                return false;
            }
            exitWidth = width;
        }
        if (turn != static_cast<int>(CurveTurn::Left) && turn != static_cast<int>(CurveTurn::Right)) {
            error = "Track layout contains an invalid curve turn.";
            return false;
        }

        std::uint32_t loadedId = 0;
        if (type == static_cast<int>(TrackPieceType::Straight)) {
            loadedId = loaded.AddStraight(entry, static_cast<Heading>(heading), length, width, exitWidth,
                                          elevationDelta, lateralOffset, static_cast<SurfaceMaterial>(material));
        } else if (type == static_cast<int>(TrackPieceType::Curve)) {
            loadedId = loaded.AddCurve(entry, static_cast<Heading>(heading), static_cast<CurveTurn>(turn), radius,
                                       width, exitWidth, elevationDelta, static_cast<SurfaceMaterial>(material),
                                       curveDegrees, bankAngleDegrees);
        } else if (type == static_cast<int>(TrackPieceType::Loop)) {
            loadedId = loaded.AddLoop(entry, static_cast<Heading>(heading), radius, width, exitWidth,
                                      elevationDelta, static_cast<SurfaceMaterial>(material), lateralOffset);
        } else {
            loadedId = type == static_cast<int>(TrackPieceType::Twist)
                ? loaded.AddTwist(entry, static_cast<Heading>(heading), length, width, exitWidth,
                                  elevationDelta, lateralOffset, static_cast<SurfaceMaterial>(material))
                : type == static_cast<int>(TrackPieceType::Branch)
                    ? loaded.AddBranch(entry, static_cast<Heading>(heading), length, lateralOffset, width,
                                       static_cast<SurfaceMaterial>(material))
                    : loaded.AddMerge(entry, static_cast<Heading>(heading), length, lateralOffset, width,
                                      static_cast<SurfaceMaterial>(material));
        }
        if (loadedId == 0) {
            error = "Track layout piece overlaps existing geometry or has invalid dimensions.";
            return false;
        }
        savedToLoadedId[savedId] = loadedId;
    }

    if (savedStartId != 0) {
        const std::map<std::uint32_t, std::uint32_t>::const_iterator start = savedToLoadedId.find(savedStartId);
        if (start == savedToLoadedId.end() ||
            !loaded.SetStartFinish(start->second, static_cast<RaceDirection>(savedDirection))) {
            error = "Track layout start/finish piece is invalid.";
            return false;
        }
    }

    track = loaded;
    return true;
}

} // namespace TrackLayoutCodec
