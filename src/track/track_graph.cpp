#include "track.hpp"

namespace {

bool Connects(const TrackConnector& exit, const TrackConnector& entry) {
    return exit.position == entry.position && exit.heading == entry.heading && exit.width == entry.width;
}

} // namespace

// The connection graph deliberately records connector indices, not only piece
// IDs: branch and merge components expose multiple independently connectable
// arms.
std::vector<TrackConnection> Track::Connections() const {
    std::vector<TrackConnection> connections;
    for (std::vector<TrackPiece>::const_iterator piece = pieces_.begin(); piece != pieces_.end(); ++piece) {
        const std::vector<TrackConnector> exits = piece->ExitConnectors();
        for (std::vector<TrackPiece>::const_iterator other = pieces_.begin(); other != pieces_.end(); ++other) {
            if (piece->id == other->id) continue;
            const std::vector<TrackConnector> entries = other->EntryConnectors();
            for (std::size_t exitIndex = 0; exitIndex < exits.size(); ++exitIndex) {
                for (std::size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex) {
                    if (Connects(exits[exitIndex], entries[entryIndex])) {
                        connections.push_back(TrackConnection{TrackConnectorRef{piece->id, exitIndex},
                                                              TrackConnectorRef{other->id, entryIndex}});
                    }
                }
            }
        }
    }
    return connections;
}
