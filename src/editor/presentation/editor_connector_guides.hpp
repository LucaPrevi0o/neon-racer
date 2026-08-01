#pragma once

#include "../../track/track.hpp"

#include <vector>

namespace EditorPresentation {

struct ConnectorGuideSet {
    std::vector<TrackConnector> entries;
    std::vector<TrackConnector> exits;
};

inline ConnectorGuideSet ConnectorGuidesFor(const TrackPiece& piece) {
    return ConnectorGuideSet{piece.EntryConnectors(), piece.ExitConnectors()};
}

} // namespace EditorPresentation
