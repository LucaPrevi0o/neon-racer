#include "track.hpp"

#include <map>
#include <set>

// Race readiness is a graph policy layered on top of valid individual pieces:
// every arm must connect exactly once and every piece must participate in one
// strongly connected network.
TrackValidation Track::Validate() const {
    if (!validationDirty_) return cachedValidation_;
    TrackValidation validation = {false, std::vector<TrackIssue>()};
    if (pieces_.empty()) {
        validation.issues.push_back(TrackIssue{TrackIssueKind::NotOneClosedLoop,
            "Place track pieces to form one closed loop.", std::vector<std::uint32_t>()});
        cachedValidation_ = validation;
        validationDirty_ = false;
        return cachedValidation_;
    }

    typedef std::pair<std::uint32_t, std::size_t> ConnectorKey;
    const std::vector<TrackConnection> connections = Connections();
    std::map<ConnectorKey, std::vector<std::uint32_t> > exitsToPieces;
    std::map<ConnectorKey, std::vector<std::uint32_t> > entriesFromPieces;
    std::map<std::uint32_t, std::set<std::uint32_t> > forwardGraph;
    std::map<std::uint32_t, std::set<std::uint32_t> > reverseGraph;
    for (std::vector<TrackConnection>::const_iterator connection = connections.begin(); connection != connections.end(); ++connection) {
        const ConnectorKey exitKey(connection->exit.pieceId, connection->exit.connectorIndex);
        const ConnectorKey entryKey(connection->entry.pieceId, connection->entry.connectorIndex);
        exitsToPieces[exitKey].push_back(connection->entry.pieceId);
        entriesFromPieces[entryKey].push_back(connection->exit.pieceId);
        forwardGraph[connection->exit.pieceId].insert(connection->entry.pieceId);
        reverseGraph[connection->entry.pieceId].insert(connection->exit.pieceId);
    }

    for (std::vector<TrackPiece>::const_iterator piece = pieces_.begin(); piece != pieces_.end(); ++piece) {
        const std::vector<TrackConnector> exits = piece->ExitConnectors();
        for (std::size_t index = 0; index < exits.size(); ++index) {
            const std::size_t matches = exitsToPieces[ConnectorKey(piece->id, index)].size();
            if (matches == 0) validation.issues.push_back(TrackIssue{TrackIssueKind::DisconnectedExit,
                "This exit connector is unconnected.", std::vector<std::uint32_t>{piece->id}});
            else if (matches > 1) validation.issues.push_back(TrackIssue{TrackIssueKind::DisconnectedExit,
                "One exit connector matches more than one entry connector.", std::vector<std::uint32_t>{piece->id}});
        }
        const std::vector<TrackConnector> entries = piece->EntryConnectors();
        for (std::size_t index = 0; index < entries.size(); ++index) {
            const std::size_t matches = entriesFromPieces[ConnectorKey(piece->id, index)].size();
            if (matches == 0) validation.issues.push_back(TrackIssue{TrackIssueKind::DisconnectedEntry,
                "This entry connector is unconnected.", std::vector<std::uint32_t>{piece->id}});
            else if (matches > 1) validation.issues.push_back(TrackIssue{TrackIssueKind::DisconnectedEntry,
                "One entry connector matches more than one exit connector.", std::vector<std::uint32_t>{piece->id}});
        }
    }

    for (std::vector<TrackPiece>::const_iterator piece = pieces_.begin(); piece != pieces_.end(); ++piece) {
        if (HasOverlappingGeometry(*piece)) validation.issues.push_back(TrackIssue{TrackIssueKind::OverlappingGeometry,
            "Track geometry overlaps another piece away from a compatible connector.", std::vector<std::uint32_t>{piece->id}});
    }

    if (validation.issues.empty()) {
        const std::uint32_t firstPieceId = pieces_.front().id;
        std::set<std::uint32_t> forwardVisited;
        std::set<std::uint32_t> reverseVisited;
        std::vector<std::uint32_t> pending(1, firstPieceId);
        while (!pending.empty()) {
            const std::uint32_t current = pending.back();
            pending.pop_back();
            if (!forwardVisited.insert(current).second) continue;
            const std::set<std::uint32_t>& next = forwardGraph[current];
            pending.insert(pending.end(), next.begin(), next.end());
        }
        pending.push_back(firstPieceId);
        while (!pending.empty()) {
            const std::uint32_t current = pending.back();
            pending.pop_back();
            if (!reverseVisited.insert(current).second) continue;
            const std::set<std::uint32_t>& previous = reverseGraph[current];
            pending.insert(pending.end(), previous.begin(), previous.end());
        }
        if (forwardVisited.size() != pieces_.size() || reverseVisited.size() != pieces_.size()) {
            validation.issues.push_back(TrackIssue{TrackIssueKind::NotOneClosedLoop,
                "All components must form one connected, closed race network.", std::vector<std::uint32_t>()});
        }
    }

    if (!HasStartFinish()) validation.issues.push_back(TrackIssue{TrackIssueKind::MissingStartFinish,
        "Choose a straight piece for the start/finish line.", std::vector<std::uint32_t>()});
    else {
        const TrackPiece* startPiece = FindPiece(startFinishPieceId_);
        if (startPiece == 0 || startPiece->type != TrackPieceType::Straight) validation.issues.push_back(
            TrackIssue{TrackIssueKind::InvalidStartFinish, "The start/finish line must be on a straight piece.",
                       std::vector<std::uint32_t>{startFinishPieceId_}});
    }
    validation.raceReady = validation.issues.empty();
    cachedValidation_ = validation;
    validationDirty_ = false;
    return cachedValidation_;
}
