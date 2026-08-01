#include "../../src/race/internal/track_position_tracker.hpp"
#include "../../src/race/time_trial.hpp"
#include "../../src/track/track.hpp"
#include "../../src/track/track_progress_graph.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

int failures = 0;
const float kFixedStep = 1.0f / 120.0f;

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

const TrackProgressTransition* FindTransition(const TrackProgressGraph& graph,
                                              std::uint32_t fromPieceId,
                                              std::uint32_t toPieceId) {
    for (std::vector<TrackProgressTransition>::const_iterator transition = graph.transitions.begin();
         transition != graph.transitions.end(); ++transition) {
        if (transition->fromPieceId == fromPieceId && transition->toPieceId == toPieceId)
            return &(*transition);
    }
    return 0;
}

bool CrossTransition(TrackPositionTracker& tracker, const TrackProgressGraph& graph,
                     std::uint32_t fromPieceId, std::uint32_t toPieceId,
                     bool correctDirection = true) {
    const TrackProgressTransition* transition = FindTransition(graph, fromPieceId, toPieceId);
    if (transition == 0) return false;
    RaceVector3 direction = HeadingVector(transition->portal.crossingHeading);
    if (!correctDirection) direction = Scale(direction, -1.0f);
    const TrackConnector connector = transition->portal.connector;
    const RaceVector3 center = RaceVector3{
        static_cast<float>(connector.position.x),
        static_cast<float>(connector.position.y) + 0.16f,
        static_cast<float>(connector.position.z),
    };
    tracker.Update(Add(center, Scale(direction, -0.25f)),
                   Add(center, Scale(direction, 0.25f)));
    return true;
}

void TestRecoveryChoosesTheConfirmedBranchArm() {
    Track track;
    const std::uint32_t start = track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    const std::uint32_t branch = track.AddBranch(GridPosition{4, 0, 0}, Heading::East, 6, 3);
    const std::uint32_t rightArm = track.AddStraight(GridPosition{10, 0, 3}, Heading::East, 6);
    const std::uint32_t leftArm = track.AddStraight(GridPosition{10, 0, -3}, Heading::East, 6);
    track.SetStartFinish(start, RaceDirection::Forward);
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);

    TrackPositionTracker rightTracker;
    rightTracker.Configure(track);
    Expect(CrossTransition(rightTracker, graph, start, branch),
           "branch recovery test enters the branch through a real checkpoint portal");
    const TrackPositionTracker::RecoveryPose shared = rightTracker.CurrentRecoveryPose();
    Expect(rightTracker.CurrentPieceId() == branch && shared.position.x > 4.0f &&
               shared.position.x < 5.0f && NearlyEqual(shared.position.z, 0.0f),
           "recovery stays centered while the branch exit has not been selected");
    Expect(CrossTransition(rightTracker, graph, branch, rightArm),
           "right-arm recovery test crosses the selected branch portal");
    const TrackPositionTracker::RecoveryPose right = rightTracker.CurrentRecoveryPose();
    Expect(rightTracker.CurrentPieceId() == rightArm && right.position.x > 10.0f &&
               right.position.x < 11.0f && NearlyEqual(right.position.z, 3.0f) &&
               NearlyEqual(right.headingRadians, 0.0f),
           "the recovery pose is inset into the exact confirmed right branch arm");

    TrackPositionTracker leftTracker;
    leftTracker.Configure(track);
    Expect(CrossTransition(leftTracker, graph, start, branch) &&
               CrossTransition(leftTracker, graph, branch, leftArm),
           "left-arm recovery test crosses real directed checkpoint portals");
    const TrackPositionTracker::RecoveryPose left = leftTracker.CurrentRecoveryPose();
    Expect(leftTracker.CurrentPieceId() == leftArm && left.position.x > 10.0f &&
               left.position.x < 11.0f && NearlyEqual(left.position.z, -3.0f) &&
               NearlyEqual(left.headingRadians, 0.0f),
           "the recovery pose is inset into the exact confirmed left branch arm");
}

void TestWrongWayCrossingDoesNotMoveRecoveryCheckpoint() {
    Track track;
    const std::uint32_t start = track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    const std::uint32_t next = track.AddStraight(GridPosition{4, 0, 0}, Heading::East, 4);
    track.SetStartFinish(start, RaceDirection::Forward);
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);

    TrackPositionTracker tracker;
    tracker.Configure(track);
    const TrackPositionTracker::RecoveryPose before = tracker.CurrentRecoveryPose();
    Expect(CrossTransition(tracker, graph, start, next, false),
           "wrong-way recovery test locates the first transition");
    const TrackPositionTracker::RecoveryPose after = tracker.CurrentRecoveryPose();
    Expect(tracker.CurrentPieceId() == start &&
               NearlyEqual(after.position.x, before.position.x) &&
               NearlyEqual(after.position.y, before.position.y) &&
               NearlyEqual(after.position.z, before.position.z) &&
               NearlyEqual(after.headingRadians, before.headingRadians),
           "an unconfirmed crossing cannot advance the recovery checkpoint");
}

void TestReverseRecoveryEntersDestinationFromItsExit() {
    Track track;
    const std::uint32_t first = track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    const std::uint32_t second = track.AddStraight(GridPosition{4, 0, 0}, Heading::East, 4);
    track.SetStartFinish(second, RaceDirection::Reverse);
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);

    TrackPositionTracker tracker;
    tracker.Configure(track);
    Expect(CrossTransition(tracker, graph, second, first),
           "reverse recovery test crosses the reversed connector portal");
    const TrackPositionTracker::RecoveryPose recovery = tracker.CurrentRecoveryPose();
    Expect(tracker.CurrentPieceId() == first && recovery.position.x > 3.0f &&
               recovery.position.x < 4.0f && NearlyEqual(recovery.position.z, 0.0f) &&
               NearlyEqual(std::fabs(recovery.headingRadians), 3.14159265358979323846f),
           "reverse recovery is inset from the destination exit and faces reverse race direction");
}

void TestTimeTrialRecoveryPreservesAttemptState() {
    const Track track = Track::CreateSampleCircuit();
    TimeTrial trial;
    trial.Start(track);
    const RaceInput accelerate = RaceInput{0.0f, 1.0f, 0.0f, 0.0f, false, false, false};
    for (int step = 0; step < 30; ++step) trial.Update(kFixedStep, accelerate);

    const float timeBeforeRecovery = trial.TotalTime();
    const int lapBeforeRecovery = trial.CurrentLap();
    Expect(timeBeforeRecovery > 0.0f && trial.Car().position.x > 0.0f,
           "time-trial recovery setup moves the car away from the start");

    trial.Recover();
    Expect(NearlyEqual(trial.TotalTime(), timeBeforeRecovery, 0.0001f) &&
               trial.CurrentLap() == lapBeforeRecovery && !trial.IsPaused() &&
               NearlyEqual(trial.Car().position.x, 0.0f) &&
               NearlyEqual(trial.Car().position.y, 0.16f) &&
               NearlyEqual(trial.Car().position.z, 0.0f) &&
               NearlyEqual(trial.Car().speed, 0.0f) &&
               std::strcmp(trial.StatusMessage(), "Recovered to the last confirmed checkpoint.") == 0,
           "recovery resets only the car pose while preserving lap and timer state");
}

} // namespace

int main() {
    TestRecoveryChoosesTheConfirmedBranchArm();
    TestWrongWayCrossingDoesNotMoveRecoveryCheckpoint();
    TestReverseRecoveryEntersDestinationFromItsExit();
    TestTimeTrialRecoveryPreservesAttemptState();
    if (failures == 0) std::cout << "Neon Racer checkpoint-recovery tests passed.\n";
    return failures == 0 ? 0 : 1;
}
