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

struct BranchedCircuit {
    Track track;
    std::uint32_t firstStraight;
    std::uint32_t branch;
    std::uint32_t rightArm;
    std::uint32_t leftArm;
    std::uint32_t merge;
    std::uint32_t timingStart;
};

BranchedCircuit CreateBranchedCircuit() {
    // This is the same geometry already exercised by the position-tracker
    // suite. Selecting the north return straight as the timing start rotates
    // the lap so its one-third target falls at the two branch-arm portals.
    BranchedCircuit circuit;
    circuit.firstStraight = circuit.track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    circuit.branch = circuit.track.AddBranch(GridPosition{4, 0, 0}, Heading::East, 6, 3);
    circuit.rightArm = circuit.track.AddStraight(GridPosition{10, 0, 3}, Heading::East, 10);
    circuit.leftArm = circuit.track.AddStraight(GridPosition{10, 0, -3}, Heading::East, 10);
    circuit.merge = circuit.track.AddMerge(GridPosition{20, 0, 0}, Heading::East, 6, 3);
    circuit.track.AddCurve(GridPosition{26, 0, 0}, Heading::East, CurveTurn::Right, 4);
    circuit.track.AddStraight(GridPosition{30, 0, 4}, Heading::South, 8);
    circuit.track.AddCurve(GridPosition{30, 0, 12}, Heading::South, CurveTurn::Right, 4);
    circuit.track.AddStraight(GridPosition{26, 0, 16}, Heading::West, 20);
    circuit.track.AddStraight(GridPosition{6, 0, 16}, Heading::West, 6);
    circuit.track.AddCurve(GridPosition{0, 0, 16}, Heading::West, CurveTurn::Right, 4);
    circuit.timingStart = circuit.track.AddStraight(GridPosition{-4, 0, 12}, Heading::North, 8);
    circuit.track.AddCurve(GridPosition{-4, 0, 4}, Heading::North, CurveTurn::Right, 4);
    circuit.track.SetStartFinish(circuit.timingStart, RaceDirection::Forward);
    return circuit;
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

void TestBranchCircuitProducesRouteVariantsAndAlternativeGates() {
    const BranchedCircuit circuit = CreateBranchedCircuit();
    Expect(circuit.firstStraight != 0 && circuit.branch != 0 && circuit.rightArm != 0 &&
               circuit.leftArm != 0 && circuit.merge != 0 && circuit.timingStart != 0 &&
               circuit.track.Validate().raceReady,
           "branch-sector fixture is the repository's complete race-ready branch circuit");

    const TrackSectorLayout sectors = BuildTrackSectorLayout(circuit.track);
    Expect(sectors.IsReady() && sectors.routeVariants.size() == 2u,
           "a branch and merge produce one sector route variant per connected arm");
    Expect(sectors.boundaries.size() == 2u &&
               sectors.boundaries[0].alternativeGates.size() == 2u,
           "the rotated lap exposes both branch-arm portals as one logical sector boundary");

    std::set<std::uint32_t> firstBoundaryIds;
    std::set<std::vector<std::uint32_t> > routeSequences;
    for (std::vector<TrackSectorRouteVariant>::const_iterator route = sectors.routeVariants.begin();
         route != sectors.routeVariants.end(); ++route) {
        firstBoundaryIds.insert(route->firstBoundaryTransitionId);
        routeSequences.insert(route->transitionIds);
        const std::size_t first = TransitionIndex(*route, route->firstBoundaryTransitionId);
        const std::size_t second = TransitionIndex(*route, route->secondBoundaryTransitionId);
        Expect(first < second && second + 1u < route->transitionIds.size(),
               "each branch route crosses its two timing boundaries in sector order");
    }
    Expect(routeSequences.size() == 2u,
           "the two route variants retain distinct directed transition sequences");
    Expect(firstBoundaryIds.size() == 2u,
           "the branch routes select different exact gates for their shared first boundary");
}

void TestReverseBranchCircuitProducesSectors() {
    BranchedCircuit circuit = CreateBranchedCircuit();
    Expect(circuit.track.SetStartFinish(circuit.timingStart, RaceDirection::Reverse),
           "reverse-sector fixture changes race direction");
    const TrackSectorLayout sectors = BuildTrackSectorLayout(circuit.track);
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
    TestBranchCircuitProducesRouteVariantsAndAlternativeGates();
    TestReverseBranchCircuitProducesSectors();
    TestSectorLayoutReportsUnavailableTracks();
    if (failures == 0) std::cout << "Neon Racer track-progress and sector-layout tests passed.\n";
    return failures == 0 ? 0 : 1;
}
