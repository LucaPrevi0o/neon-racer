#include "../../src/track/track_progress_graph.hpp"
#include "../../src/track/track_sector_layout.hpp"

#include <cstdint>
#include <iostream>
#include <set>
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

std::size_t TransitionIndex(const TrackSectorRouteVariant& route, std::uint32_t transitionId) {
    for (std::size_t index = 0; index < route.transitionIds.size(); ++index) {
        if (route.transitionIds[index] == transitionId) return index;
    }
    return route.transitionIds.size();
}

Track CreateBranchCircuit() {
    Track track;
    const std::uint32_t start = track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4, 3);
    const std::uint32_t branch = track.AddBranch(GridPosition{4, 0, 0}, Heading::East, 8, 6, 3);
    const std::uint32_t southArm = track.AddStraight(GridPosition{12, 0, 6}, Heading::East, 40, 3);
    const std::uint32_t northArm = track.AddStraight(GridPosition{12, 0, -6}, Heading::East, 40, 3);
    const std::uint32_t merge = track.AddMerge(GridPosition{52, 0, 0}, Heading::East, 8, 6, 3);
    const std::uint32_t postMerge = track.AddStraight(GridPosition{60, 0, 0}, Heading::East, 4, 3);
    const std::uint32_t firstTurn = track.AddCurve(GridPosition{64, 0, 0}, Heading::East,
        CurveTurn::Right, 6, 3, -1, 0, SurfaceMaterial::Regular, 180);
    const std::uint32_t returnStraight = track.AddStraight(GridPosition{64, 0, 12}, Heading::West, 64, 3);
    const std::uint32_t finalTurn = track.AddCurve(GridPosition{0, 0, 12}, Heading::West,
        CurveTurn::Right, 6, 3, -1, 0, SurfaceMaterial::Regular, 180);

    Expect(start != 0 && branch != 0 && southArm != 0 && northArm != 0 && merge != 0 &&
               postMerge != 0 && firstTurn != 0 && returnStraight != 0 && finalTurn != 0,
           "branch-sector fixture places every component without overlap");
    Expect(track.SetStartFinish(start, RaceDirection::Forward),
           "branch-sector fixture selects its start/finish straight");
    Expect(track.Validate().raceReady,
           "branch-sector fixture forms one complete race-ready network");
    return track;
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
        const TrackPiece* toPiece = track.GetPiece(connection->entry.pieceId);
        const std::vector<TrackConnector> exits = fromPiece != 0 ? fromPiece->ExitConnectors() :
            std::vector<TrackConnector>();
        const std::vector<TrackConnector> entries = toPiece != 0 ? toPiece->EntryConnectors() :
            std::vector<TrackConnector>();
        const bool validPortal = connection->exit.connectorIndex < exits.size();
        const bool validArrival = connection->entry.connectorIndex < entries.size();
        Expect(transition != 0 && validPortal && validArrival &&
                   transition->portal.connector.position == exits[connection->exit.connectorIndex].position &&
                   transition->portal.connector.width == exits[connection->exit.connectorIndex].width &&
                   transition->portal.crossingHeading == exits[connection->exit.connectorIndex].heading &&
                   transition->arrivalConnector.position == entries[connection->entry.connectorIndex].position &&
                   transition->arrivalConnector.width == entries[connection->entry.connectorIndex].width,
               "forward transition preserves its exact outgoing portal and destination entry connector");
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
        const TrackPiece* toPiece = track.GetPiece(connection->exit.pieceId);
        const std::vector<TrackConnector> entries = fromPiece != 0 ? fromPiece->EntryConnectors() :
            std::vector<TrackConnector>();
        const std::vector<TrackConnector> exits = toPiece != 0 ? toPiece->ExitConnectors() :
            std::vector<TrackConnector>();
        const bool validPortal = connection->entry.connectorIndex < entries.size();
        const bool validArrival = connection->exit.connectorIndex < exits.size();
        Expect(transition != 0 && validPortal && validArrival &&
                   transition->portal.connector.position == entries[connection->entry.connectorIndex].position &&
                   transition->portal.connector.width == entries[connection->entry.connectorIndex].width &&
                   transition->portal.crossingHeading ==
                       OppositeHeading(entries[connection->entry.connectorIndex].heading) &&
                   transition->arrivalConnector.position == exits[connection->exit.connectorIndex].position &&
                   transition->arrivalConnector.width == exits[connection->exit.connectorIndex].width,
               "reverse transition preserves its reversed portal and destination exit connector");
    }
}

void TestTransitionIdentitiesAreUnique() {
    const TrackProgressGraph graph = BuildTrackProgressGraph(Track::CreateSampleCircuit());
    std::set<std::uint32_t> identities;
    for (std::vector<TrackProgressTransition>::const_iterator transition = graph.transitions.begin();
         transition != graph.transitions.end(); ++transition) {
        Expect(transition->id != 0u, "every progress transition receives a non-zero identity");
        identities.insert(transition->id);
    }
    Expect(identities.size() == graph.transitions.size(),
           "progress transition identities are unique within one graph snapshot");
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

void TestSampleCircuitProducesOrderedSectors() {
    const Track track = Track::CreateSampleCircuit();
    const TrackSectorLayout sectors = BuildTrackSectorLayout(track);
    Expect(sectors.IsReady() && sectors.boundaries.size() == 2u &&
               sectors.routeVariants.size() == 1u,
           "the sample circuit produces one route and two intermediate sector boundaries");

    if (!sectors.routeVariants.empty()) {
        const TrackSectorRouteVariant& route = sectors.routeVariants.front();
        const std::size_t first = TransitionIndex(route, route.firstBoundaryTransitionId);
        const std::size_t second = TransitionIndex(route, route.secondBoundaryTransitionId);
        Expect(route.length > 0.0f && first < second && second + 1u < route.transitionIds.size(),
               "sample-circuit sector gates are distinct, ordered, and leave a final sector before the finish");
    }
}

void TestBranchCircuitProducesEquivalentBoundaryGates() {
    const Track track = CreateBranchCircuit();
    const TrackSectorLayout sectors = BuildTrackSectorLayout(track);
    Expect(sectors.IsReady() && sectors.routeVariants.size() == 2u,
           "a branch and merge produce one sector route variant per connected arm");
    Expect(sectors.boundaries.size() == 2u &&
               sectors.boundaries[0].alternativeGates.size() == 2u,
           "a logical sector boundary exposes both equivalent physical branch gates");

    std::set<std::uint32_t> firstBoundaryIds;
    for (std::vector<TrackSectorRouteVariant>::const_iterator route = sectors.routeVariants.begin();
         route != sectors.routeVariants.end(); ++route) {
        firstBoundaryIds.insert(route->firstBoundaryTransitionId);
        const std::size_t first = TransitionIndex(*route, route->firstBoundaryTransitionId);
        const std::size_t second = TransitionIndex(*route, route->secondBoundaryTransitionId);
        Expect(first < second && second + 1u < route->transitionIds.size(),
               "each branch route crosses its two timing boundaries in sector order");
    }
    Expect(firstBoundaryIds.size() == 2u,
           "the two branch routes select different exact gates for their shared first boundary");
}

void TestReverseBranchCircuitProducesSectors() {
    Track track = CreateBranchCircuit();
    Expect(track.SetStartFinish(track.StartFinishPieceId(), RaceDirection::Reverse),
           "reverse-sector fixture changes race direction");
    const TrackSectorLayout sectors = BuildTrackSectorLayout(track);
    Expect(sectors.IsReady() && sectors.routeVariants.size() == 2u,
           "automatic sectors preserve both branch alternatives in reverse race direction");
}

void TestSectorLayoutReportsUnavailableTracks() {
    Track missingStart;
    missingStart.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    Expect(BuildTrackSectorLayout(missingStart).status == TrackSectorLayoutStatus::MissingStartFinish,
           "sector generation reports a missing start/finish line explicitly");

    Track incomplete;
    const std::uint32_t start = incomplete.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    incomplete.SetStartFinish(start, RaceDirection::Forward);
    Expect(BuildTrackSectorLayout(incomplete).status == TrackSectorLayoutStatus::TrackNotRaceReady,
           "sector generation rejects an incomplete network before enumerating routes");
}

} // namespace

int main() {
    TestForwardGraphUsesExitPortals();
    TestReverseGraphUsesEntryPortals();
    TestTransitionIdentitiesAreUnique();
    TestFinishPortalMatchesTrackerSemantics();
    TestBranchExposesBothOutgoingPortals();
    TestDraftWithoutStartStillHasGraphConnections();
    TestSampleCircuitProducesOrderedSectors();
    TestBranchCircuitProducesEquivalentBoundaryGates();
    TestReverseBranchCircuitProducesSectors();
    TestSectorLayoutReportsUnavailableTracks();
    if (failures == 0) std::cout << "Neon Racer track-progress and sector-layout tests passed.\n";
    return failures == 0 ? 0 : 1;
}
