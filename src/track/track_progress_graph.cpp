#include "track_progress_graph.hpp"

namespace {

Heading OppositeHeading(Heading heading) {
    switch (heading) {
    case Heading::North: return Heading::South;
    case Heading::East: return Heading::West;
    case Heading::South: return Heading::North;
    case Heading::West: return Heading::East;
    }
    return Heading::North;
}

TrackProgressPortal PortalFor(const TrackConnector& connector, RaceDirection direction) {
    return TrackProgressPortal{
        connector,
        direction == RaceDirection::Forward ? connector.heading : OppositeHeading(connector.heading),
    };
}

} // namespace

TrackProgressGraph BuildTrackProgressGraph(const Track& track) {
    TrackProgressGraph graph = TrackProgressGraph{
        track.SelectedRaceDirection(),
        std::vector<TrackProgressTransition>(),
        false,
        0,
        TrackProgressPortal{TrackConnector{GridPosition{0, 0, 0}, Heading::North, 0}, Heading::North},
    };

    const bool forward = graph.direction == RaceDirection::Forward;
    const std::vector<TrackConnection> connections = track.Connections();
    for (std::vector<TrackConnection>::const_iterator connection = connections.begin();
         connection != connections.end(); ++connection) {
        const std::uint32_t fromPieceId = forward ? connection->exit.pieceId : connection->entry.pieceId;
        const std::uint32_t toPieceId = forward ? connection->entry.pieceId : connection->exit.pieceId;
        const TrackPiece* fromPiece = track.GetPiece(fromPieceId);
        const TrackPiece* toPiece = track.GetPiece(toPieceId);
        if (fromPiece == 0 || toPiece == 0) continue;

        const std::vector<TrackConnector> portals = forward ? fromPiece->ExitConnectors() :
            fromPiece->EntryConnectors();
        const std::vector<TrackConnector> arrivals = forward ? toPiece->EntryConnectors() :
            toPiece->ExitConnectors();
        const std::size_t portalIndex = forward ? connection->exit.connectorIndex :
            connection->entry.connectorIndex;
        const std::size_t arrivalIndex = forward ? connection->entry.connectorIndex :
            connection->exit.connectorIndex;
        if (portalIndex >= portals.size() || arrivalIndex >= arrivals.size()) continue;

        graph.transitions.push_back(TrackProgressTransition{
            fromPieceId,
            toPieceId,
            PortalFor(portals[portalIndex], graph.direction),
            arrivals[arrivalIndex],
        });
    }

    if (track.HasStartFinish()) {
        const TrackPiece* startPiece = track.GetPiece(track.StartFinishPieceId());
        if (startPiece != 0) {
            graph.hasStartFinish = true;
            graph.startFinishPieceId = startPiece->id;
            graph.finishPortal = PortalFor(startPiece->EntryConnector(), graph.direction);
        }
    }

    return graph;
}