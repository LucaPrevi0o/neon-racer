#include "../../src/track/track_progress_graph.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

Heading OppositeHeading(Heading heading) {
    switch (heading) {
    case Heading::North: return Heading::South;
    case Heading::East: return Heading::West;
    case Heading::South: return Heading::North;
    case Heading::West: return Heading::East;
    }
    return Heading::North;
}

const TrackProgressTransition* FindTransition(const TrackProgressGraph& graph,
                                              std::uint32_t fromPieceId,
                                              std::uint32_t toPieceId) {
    for (std::vector<TrackProgressTransition>::const_iterator transition = graph.transitions.begin();
         transition != graph.transitions.end(); ++transition) {
        if (transition->fromPieceId == fromPieceId && transition->toPieceId == toPieceId) {
            return &(*transition);
        }
    }
    return 0;
}

void TestForwardGraphUsesExitPortals() {
    const Track track = Track::CreateSampleCircuit();
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);
    const std::vector<TrackConnection> connections = track.Connections();

    Expect(graph.direction == RaceDirection::Forward &&
               graph.transitions.size() == connections.size(),
           "forward progress graph preserves every authored connection");

    for (std::vector<TrackConnection>::const_iterator connection = connections.begin();
         connection != connections.end(); ++connection) {
        const TrackProgressTransition* transition = FindTransition(
            graph, connection->exit.pieceId, connection->entry.pieceId);
        const TrackPiece* fromPiece = track.GetPiece(connection->exit.pieceId);
        const std::vector<TrackConnector> exits = fromPiece != 0 ? fromPiece->ExitConnectors() :
            std::vector<TrackConnector>();
        const bool validIndex = connection->exit.connectorIndex < exits.size();
        Expect(transition != 0 && validIndex &&
                   transition->portal.connector.position == exits[connection->exit.connectorIndex].position &&
                   transition->portal.connector.width == exits[connection->exit.connectorIndex].width &&
                   transition->portal.crossingHeading == exits[connection->exit.connectorIndex].heading,
               "forward transition exposes its exact outgoing connector as the crossing portal");
    }
}

void TestReverseGraphUsesEntryPortals() {
    Track track = Track::CreateSampleCircuit();
    track.SetStartFinish(track.StartFinishPieceId(), RaceDirection::Reverse);
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);
    const std::vector<TrackConnection> connections = track.Connections();

    Expect(graph.direction == RaceDirection::Reverse &&
               graph.transitions.size() == connections.size(),
           "reverse progress graph reverses every authored connection");

    for (std::vector<TrackConnection>::const_iterator connection = connections.begin();
         connection != connections.end(); ++connection) {
        const TrackProgressTransition* transition = FindTransition(
            graph, connection->entry.pieceId, connection->exit.pieceId);
        const TrackPiece* fromPiece = track.GetPiece(connection->entry.pieceId);
        const std::vector<TrackConnector> entries = fromPiece != 0 ? fromPiece->EntryConnectors() :
            std::vector<TrackConnector>();
        const bool validIndex = connection->entry.connectorIndex < entries.size();
        Expect(transition != 0 && validIndex &&
                   transition->portal.connector.position == entries[connection->entry.connectorIndex].position &&
                   transition->portal.connector.width == entries[connection->entry.connectorIndex].width &&
                   transition->portal.crossingHeading ==
                       OppositeHeading(entries[connection->entry.connectorIndex].heading),
               "reverse transition exposes the current piece entry with reversed crossing direction");
    }
}

void TestFinishPortalMatchesTrackerSemantics() {
    Track forwardTrack = Track::CreateSampleCircuit();
    const TrackProgressGraph forwardGraph = BuildTrackProgressGraph(forwardTrack);
    const TrackPiece* forwardStart = forwardTrack.GetPiece(forwardTrack.StartFinishPieceId());
    Expect(forwardGraph.hasStartFinish && forwardStart != 0 &&
               forwardGraph.finishPortal.connector.position == forwardStart->EntryConnector().position &&
               forwardGraph.finishPortal.crossingHeading == forwardStart->EntryConnector().heading,
           "forward finish portal uses the start piece entry plane");

    Track reverseTrack = Track::CreateSampleCircuit();
    reverseTrack.SetStartFinish(reverseTrack.StartFinishPieceId(), RaceDirection::Reverse);
    const TrackProgressGraph reverseGraph = BuildTrackProgressGraph(reverseTrack);
    const TrackPiece* reverseStart = reverseTrack.GetPiece(reverseTrack.StartFinishPieceId());
    Expect(reverseGraph.hasStartFinish && reverseStart != 0 &&
               reverseGraph.finishPortal.connector.position == reverseStart->EntryConnector().position &&
               reverseGraph.finishPortal.crossingHeading ==
                   OppositeHeading(reverseStart->EntryConnector().heading),
           "reverse finish portal keeps the same plane and reverses its crossing direction");
}

void TestBranchExposesBothOutgoingPortals() {
    Track track;
    const std::uint32_t branch = track.AddBranch(GridPosition{0, 0, 0}, Heading::East, 6, 3);
    track.AddStraight(GridPosition{6, 0, 3}, Heading::East, 4);
    track.AddStraight(GridPosition{6, 0, -3}, Heading::East, 4);
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);

    int outgoing = 0;
    for (std::vector<TrackProgressTransition>::const_iterator transition = graph.transitions.begin();
         transition != graph.transitions.end(); ++transition) {
        if (transition->fromPieceId == branch) ++outgoing;
    }
    Expect(outgoing == 2, "a forward branch exposes both connected outgoing checkpoint portals");
}

void TestDraftWithoutStartStillHasGraphConnections() {
    Track track;
    track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    track.AddStraight(GridPosition{4, 0, 0}, Heading::East, 4);
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);
    Expect(!graph.hasStartFinish && graph.startFinishPieceId == 0 && graph.transitions.size() == 1,
           "an incomplete draft can visualize connections without inventing a finish portal");
}

} // namespace

int main() {
    TestForwardGraphUsesExitPortals();
    TestReverseGraphUsesEntryPortals();
    TestFinishPortalMatchesTrackerSemantics();
    TestBranchExposesBothOutgoingPortals();
    TestDraftWithoutStartStillHasGraphConnections();
    if (failures == 0) std::cout << "Neon Racer track-progress graph tests passed.\n";
    return failures == 0 ? 0 : 1;
}
