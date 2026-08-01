#include "../../src/race/internal/track_position_tracker.hpp"
#include "../../src/track/track.hpp"

#include <cmath>
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

bool NearlyEqual(float first, float second, float tolerance = 0.05f) {
    return std::fabs(first - second) < tolerance;
}

RaceVector3 HeadingVector(Heading heading) {
    switch (heading) {
    case Heading::North: return RaceVector3{0.0f, 0.0f, -1.0f};
    case Heading::East: return RaceVector3{1.0f, 0.0f, 0.0f};
    case Heading::South: return RaceVector3{0.0f, 0.0f, 1.0f};
    case Heading::West: return RaceVector3{-1.0f, 0.0f, 0.0f};
    }
    return RaceVector3{0.0f, 0.0f, 0.0f};
}

RaceVector3 Scale(RaceVector3 vector, float amount) {
    return RaceVector3{vector.x * amount, vector.y * amount, vector.z * amount};
}

RaceVector3 Add(RaceVector3 first, RaceVector3 second) {
    return RaceVector3{first.x + second.x, first.y + second.y, first.z + second.z};
}

RaceVector3 ConnectorPosition(const TrackConnector& connector) {
    return RaceVector3{static_cast<float>(connector.position.x),
                       static_cast<float>(connector.position.y) + 0.16f,
                       static_cast<float>(connector.position.z)};
}

bool FindDirectedTransition(const Track& track, std::uint32_t fromPieceId, std::uint32_t toPieceId,
                            TrackConnector& gate) {
    const bool forward = track.SelectedRaceDirection() == RaceDirection::Forward;
    const std::vector<TrackConnection> connections = track.Connections();
    for (std::vector<TrackConnection>::const_iterator connection = connections.begin();
         connection != connections.end(); ++connection) {
        const std::uint32_t from = forward ? connection->exit.pieceId : connection->entry.pieceId;
        const std::uint32_t to = forward ? connection->entry.pieceId : connection->exit.pieceId;
        if (from != fromPieceId || to != toPieceId) continue;
        const TrackPiece* piece = track.GetPiece(fromPieceId);
        if (piece == 0) return false;
        const std::vector<TrackConnector> connectors = forward ? piece->ExitConnectors() : piece->EntryConnectors();
        const std::size_t index = forward ? connection->exit.connectorIndex : connection->entry.connectorIndex;
        if (index >= connectors.size()) return false;
        gate = connectors[index];
        return true;
    }
    return false;
}

bool CrossTransition(TrackPositionTracker& tracker, const Track& track,
                     std::uint32_t fromPieceId, std::uint32_t toPieceId, bool correctDirection = true) {
    TrackConnector connector;
    if (!FindDirectedTransition(track, fromPieceId, toPieceId, connector)) return false;
    RaceVector3 direction = HeadingVector(connector.heading);
    if (track.SelectedRaceDirection() == RaceDirection::Reverse) direction = Scale(direction, -1.0f);
    if (!correctDirection) direction = Scale(direction, -1.0f);
    const RaceVector3 center = ConnectorPosition(connector);
    tracker.Update(Add(center, Scale(direction, -0.25f)), Add(center, Scale(direction, 0.25f)));
    return true;
}

void CrossFinish(TrackPositionTracker& tracker, const Track& track) {
    const TrackPiece* start = track.GetPiece(track.StartFinishPieceId());
    const TrackConnector connector = start->EntryConnector();
    RaceVector3 direction = HeadingVector(connector.heading);
    if (track.SelectedRaceDirection() == RaceDirection::Reverse) direction = Scale(direction, -1.0f);
    const RaceVector3 center = ConnectorPosition(connector);
    tracker.Update(Add(center, Scale(direction, -0.25f)), Add(center, Scale(direction, 0.25f)));
}

std::vector<std::uint32_t> SinglePathCycle(const Track& track) {
    std::vector<std::uint32_t> route;
    std::uint32_t current = track.StartFinishPieceId();
    const bool forward = track.SelectedRaceDirection() == RaceDirection::Forward;
    const std::vector<TrackConnection> connections = track.Connections();
    for (std::size_t step = 0; step <= track.Pieces().size(); ++step) {
        std::set<std::uint32_t> candidates;
        for (std::vector<TrackConnection>::const_iterator connection = connections.begin();
             connection != connections.end(); ++connection) {
            const std::uint32_t from = forward ? connection->exit.pieceId : connection->entry.pieceId;
            const std::uint32_t to = forward ? connection->entry.pieceId : connection->exit.pieceId;
            if (from == current) candidates.insert(to);
        }
        if (candidates.size() != 1) return std::vector<std::uint32_t>();
        current = *candidates.begin();
        route.push_back(current);
        if (current == track.StartFinishPieceId()) return route;
    }
    return std::vector<std::uint32_t>();
}

void FollowRoute(TrackPositionTracker& tracker, const Track& track,
                 const std::vector<std::uint32_t>& route) {
    std::uint32_t current = tracker.CurrentPieceId();
    for (std::vector<std::uint32_t>::const_iterator next = route.begin(); next != route.end(); ++next) {
        Expect(CrossTransition(tracker, track, current, *next), "test route references a real directed connector");
        current = *next;
    }
}

struct BranchedCircuit {
    Track track;
    std::uint32_t start;
    std::uint32_t branch;
    std::uint32_t rightArm;
    std::uint32_t leftArm;
    std::uint32_t merge;
    std::vector<std::uint32_t> sharedReturn;
};

BranchedCircuit CreateBranchedCircuit() {
    BranchedCircuit circuit;
    circuit.start = circuit.track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    circuit.branch = circuit.track.AddBranch(GridPosition{4, 0, 0}, Heading::East, 6, 3);
    circuit.rightArm = circuit.track.AddStraight(GridPosition{10, 0, 3}, Heading::East, 10);
    circuit.leftArm = circuit.track.AddStraight(GridPosition{10, 0, -3}, Heading::East, 10);
    circuit.merge = circuit.track.AddMerge(GridPosition{20, 0, 0}, Heading::East, 6, 3);
    circuit.sharedReturn.push_back(circuit.track.AddCurve(GridPosition{26, 0, 0}, Heading::East,
                                                         CurveTurn::Right, 4));
    circuit.sharedReturn.push_back(circuit.track.AddStraight(GridPosition{30, 0, 4}, Heading::South, 8));
    circuit.sharedReturn.push_back(circuit.track.AddCurve(GridPosition{30, 0, 12}, Heading::South,
                                                         CurveTurn::Right, 4));
    circuit.sharedReturn.push_back(circuit.track.AddStraight(GridPosition{26, 0, 16}, Heading::West, 20));
    circuit.sharedReturn.push_back(circuit.track.AddStraight(GridPosition{6, 0, 16}, Heading::West, 6));
    circuit.sharedReturn.push_back(circuit.track.AddCurve(GridPosition{0, 0, 16}, Heading::West,
                                                         CurveTurn::Right, 4));
    circuit.sharedReturn.push_back(circuit.track.AddStraight(GridPosition{-4, 0, 12}, Heading::North, 8));
    circuit.sharedReturn.push_back(circuit.track.AddCurve(GridPosition{-4, 0, 4}, Heading::North,
                                                         CurveTurn::Right, 4));
    circuit.track.SetStartFinish(circuit.start, RaceDirection::Forward);
    return circuit;
}

std::vector<std::uint32_t> BranchRoute(const BranchedCircuit& circuit, std::uint32_t arm) {
    std::vector<std::uint32_t> route;
    route.push_back(circuit.branch);
    route.push_back(arm);
    route.push_back(circuit.merge);
    route.insert(route.end(), circuit.sharedReturn.begin(), circuit.sharedReturn.end());
    route.push_back(circuit.start);
    return route;
}

void TestProjectionTracksProgressWithinConfirmedPiece() {
    const Track track = Track::CreateSampleCircuit();
    TrackPositionTracker tracker;
    tracker.Configure(track);
    tracker.Update(RaceVector3{1.0f, 0.16f, 0.0f}, RaceVector3{3.0f, 0.16f, 0.0f});
    Expect(tracker.CurrentPieceId() == track.StartFinishPieceId() &&
               NearlyEqual(tracker.CurrentProgress(), 0.5f),
           "the tracker projects a normalized position along the confirmed start piece");
}

void TestForwardAndReverseConnectorCycles() {
    Track forwardTrack = Track::CreateSampleCircuit();
    TrackPositionTracker forward;
    forward.Configure(forwardTrack);
    const std::vector<std::uint32_t> forwardRoute = SinglePathCycle(forwardTrack);
    FollowRoute(forward, forwardTrack, forwardRoute);
    Expect(!forwardRoute.empty() && forward.LapCompleted(),
           "a forward lap completes after crossing every directed connector gate");

    Track reverseTrack = Track::CreateSampleCircuit();
    reverseTrack.SetStartFinish(reverseTrack.StartFinishPieceId(), RaceDirection::Reverse);
    TrackPositionTracker reverse;
    reverse.Configure(reverseTrack);
    const std::vector<std::uint32_t> reverseRoute = SinglePathCycle(reverseTrack);
    FollowRoute(reverse, reverseTrack, reverseRoute);
    Expect(!reverse.LapCompleted() && reverse.HasReturnedToStart(),
           "reverse route return enters the start piece before reaching its finish line");
    CrossFinish(reverse, reverseTrack);
    Expect(reverse.LapCompleted(), "reverse lap completes at the configured start line");
}

void TestWrongWayAndShortcutNoiseAreIgnored() {
    const Track track = Track::CreateSampleCircuit();
    const std::vector<std::uint32_t> route = SinglePathCycle(track);
    TrackPositionTracker tracker;
    tracker.Configure(track);
    const std::uint32_t start = track.StartFinishPieceId();

    Expect(route.size() > 2 && CrossTransition(tracker, track, start, route.front(), false),
           "wrong-way test finds the first connector");
    Expect(tracker.CurrentPieceId() == start && !tracker.HasDepartedStart(),
           "crossing an expected gate backwards does not move or invalidate route state");

    TrackConnector laterGate;
    Expect(FindDirectedTransition(track, route[0], route[1], laterGate),
           "shortcut-noise test finds a later connector");
    RaceVector3 laterDirection = HeadingVector(laterGate.heading);
    const RaceVector3 laterCenter = ConnectorPosition(laterGate);
    tracker.Update(Add(laterCenter, Scale(laterDirection, -0.25f)),
                   Add(laterCenter, Scale(laterDirection, 0.25f)));
    Expect(tracker.CurrentPieceId() == start,
           "crossing a non-current connector cannot corrupt the confirmed position");

    FollowRoute(tracker, track, route);
    Expect(tracker.LapCompleted(),
           "a legitimate route can still complete after unrelated or wrong-way gate noise");
}

void TestEitherBranchGateSelectsItsRoute() {
    const BranchedCircuit circuit = CreateBranchedCircuit();
    Expect(circuit.track.Validate().raceReady, "branch-position test circuit is race-ready");

    TrackPositionTracker tracker;
    tracker.Configure(circuit.track);
    FollowRoute(tracker, circuit.track, BranchRoute(circuit, circuit.rightArm));
    Expect(tracker.LapCompleted(), "crossing the right branch gate completes a valid route");

    tracker.BeginNextLap();
    FollowRoute(tracker, circuit.track, BranchRoute(circuit, circuit.leftArm));
    Expect(tracker.LapCompleted(), "crossing the left branch gate completes the alternative route");
}

} // namespace

int main() {
    TestProjectionTracksProgressWithinConfirmedPiece();
    TestForwardAndReverseConnectorCycles();
    TestWrongWayAndShortcutNoiseAreIgnored();
    TestEitherBranchGateSelectsItsRoute();
    if (failures == 0) std::cout << "Neon Racer track-position tests passed.\n";
    return failures == 0 ? 0 : 1;
}
