#include "../../src/race/internal/route_progress.hpp"
#include "../../src/track/track.hpp"

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

void ObserveStable(RouteProgress& progress, std::uint32_t pieceId) {
    progress.ObserveSurfacePiece(pieceId);
    progress.ObserveSurfacePiece(pieceId);
}

void FollowRoute(RouteProgress& progress, const std::vector<std::uint32_t>& route) {
    for (std::vector<std::uint32_t>::const_iterator pieceId = route.begin(); pieceId != route.end(); ++pieceId)
        ObserveStable(progress, *pieceId);
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

void TestStartLineRecrossNeedsAConnectedRoute() {
    const Track track = Track::CreateSampleCircuit();
    RouteProgress progress;
    progress.Configure(track);
    ObserveStable(progress, track.StartFinishPieceId());
    Expect(progress.IsValid() && !progress.HasDepartedStart() && !progress.CanCompleteLap(),
           "remaining on the start piece cannot complete a lap");
}

void TestForwardAndReverseSinglePathCycles() {
    Track forwardTrack = Track::CreateSampleCircuit();
    const std::vector<std::uint32_t> forwardRoute = SinglePathCycle(forwardTrack);
    RouteProgress forwardProgress;
    forwardProgress.Configure(forwardTrack);
    FollowRoute(forwardProgress, forwardRoute);
    Expect(!forwardRoute.empty() && forwardProgress.IsValid() && forwardProgress.CanCompleteLap(),
           "a complete forward connector cycle becomes lap-eligible");

    Track reverseTrack = Track::CreateSampleCircuit();
    reverseTrack.SetStartFinish(reverseTrack.StartFinishPieceId(), RaceDirection::Reverse);
    const std::vector<std::uint32_t> reverseRoute = SinglePathCycle(reverseTrack);
    RouteProgress reverseProgress;
    reverseProgress.Configure(reverseTrack);
    FollowRoute(reverseProgress, reverseRoute);
    Expect(!reverseRoute.empty() && reverseProgress.IsValid() && reverseProgress.CanCompleteLap(),
           "reverse racing follows the reversed connector graph");
}

void TestSkippedPieceInvalidatesTheLap() {
    const Track track = Track::CreateSampleCircuit();
    const std::vector<std::uint32_t> route = SinglePathCycle(track);
    RouteProgress progress;
    progress.Configure(track);
    if (route.size() >= 2) ObserveStable(progress, route[1]);
    Expect(route.size() >= 2 && !progress.IsValid() && !progress.CanCompleteLap(),
           "jumping directly to a non-adjacent piece invalidates route progress");
}

void TestEitherConnectedBranchArmIsAccepted() {
    const BranchedCircuit circuit = CreateBranchedCircuit();
    Expect(circuit.track.Validate().raceReady, "branch-route test circuit is race-ready");

    RouteProgress progress;
    progress.Configure(circuit.track);
    FollowRoute(progress, BranchRoute(circuit, circuit.rightArm));
    Expect(progress.IsValid() && progress.CanCompleteLap(),
           "the first connected branch arm forms a valid lap route");

    progress.BeginNextLap();
    Expect(progress.IsValid() && !progress.CanCompleteLap(),
           "beginning the next lap clears prior route completion");
    FollowRoute(progress, BranchRoute(circuit, circuit.leftArm));
    Expect(progress.IsValid() && progress.CanCompleteLap(),
           "the alternative connected branch arm also forms a valid lap route");
}

void TestSkippingABranchArmIsRejected() {
    const BranchedCircuit circuit = CreateBranchedCircuit();
    RouteProgress progress;
    progress.Configure(circuit.track);
    ObserveStable(progress, circuit.branch);
    ObserveStable(progress, circuit.merge);
    Expect(!progress.IsValid() && !progress.CanCompleteLap(),
           "a branch cannot jump directly to its merge without traversing an arm");
}

} // namespace

int main() {
    TestStartLineRecrossNeedsAConnectedRoute();
    TestForwardAndReverseSinglePathCycles();
    TestSkippedPieceInvalidatesTheLap();
    TestEitherConnectedBranchArmIsAccepted();
    TestSkippingABranchArmIsRejected();
    if (failures == 0) std::cout << "Neon Racer route-progress tests passed.\n";
    return failures == 0 ? 0 : 1;
}
