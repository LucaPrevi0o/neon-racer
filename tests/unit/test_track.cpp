#include "../../src/persistence/draft_io.hpp"
#include "../../src/track/playable_export.hpp"
#include "../../src/track/track.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

void TestSurfaceContracts() {
    TrackSurfaceSample sample = {1.0f, 2.0f, 3.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f, 2.5f, SurfaceMaterial::Regular, 7};
    TrackContact contact = {true, sample, 0.25f, false};
    TrackMetadata metadata;
    VerificationState verification;
    Expect(contact.found && contact.surface.pieceId == 7, "surface/contact contract stores a sampled piece");
    Expect(metadata.playableExportVersion == 0, "metadata starts at export version zero");
    Expect(!verification.hasSavedGhost && !verification.isVerifiedForPlayableExport,
           "verification starts invalid");
}

void TestEmptyTrackClearInvariant() {
    Track fresh;
    const TrackValidation freshValidation = fresh.Validate();
    Expect(fresh.Pieces().empty() && !fresh.HasStartFinish() && fresh.StartFinishPieceId() == 0 &&
               fresh.SelectedRaceDirection() == RaceDirection::Forward && fresh.Connections().empty(),
           "a new track starts as an empty forward draft");
    Expect(!freshValidation.raceReady && freshValidation.issues.size() == 1 &&
               freshValidation.issues.front().kind == TrackIssueKind::NotOneClosedLoop,
           "an empty track reports that it needs a closed loop");
    Expect(!fresh.QuerySurface(0.0f, 0.0f, 0.0f).found,
           "an empty track has no drivable surface");

    Track track;
    const std::uint32_t pieceId = track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    Expect(pieceId != 0 && track.SetStartFinish(pieceId, RaceDirection::Reverse),
           "clear-invariant setup creates a reverse start/finish");
    const std::uint32_t revisionBeforeClear = track.LayoutRevision();
    track.Clear();

    const TrackValidation clearedValidation = track.Validate();
    Expect(track.Pieces().empty() && !track.HasStartFinish() && track.StartFinishPieceId() == 0 &&
               track.SelectedRaceDirection() == fresh.SelectedRaceDirection() && track.Connections().empty(),
           "clearing restores the fresh empty-track state");
    Expect(!clearedValidation.raceReady && clearedValidation.issues.size() == 1 &&
               clearedValidation.issues.front().kind == TrackIssueKind::NotOneClosedLoop &&
               !track.QuerySurface(0.0f, 0.0f, 0.0f).found,
           "a cleared track exposes no stale validation or surface state");
    Expect(track.LayoutRevision() > revisionBeforeClear,
           "clearing advances the layout revision for dependent editor caches");
    Expect(track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4) == 1,
           "clearing resets the next component ID");
}

void TestDraftV1Load() {
    const std::string path = "/tmp/neon_racer_draft_v1_test.draft";
    std::ofstream file(path.c_str());
    file << "NEON_RACER_DRAFT 1\nSTART 0 0\nPIECES 1\nPIECE 1 0 0 0 0 1 5 3 0 0\n";
    file.close();

    Track track;
    std::string error;
    Expect(DraftIO::Load(path, track, error), "v1 draft remains loadable");
    Expect(track.Pieces().size() == 1, "v1 draft preserves pieces");
    std::remove(path.c_str());
}

void TestDraftV2RoundTrip() {
    const std::string path = "/tmp/neon_racer_draft_v2_test.draft";
    Track original = Track::CreateSampleCircuit();
    std::string error;
    Expect(DraftIO::Save(original, path, error), "v2 draft saves");

    Track loaded;
    Expect(DraftIO::Load(path, loaded, error), "v2 draft loads");
    Expect(loaded.Pieces().size() == original.Pieces().size(), "v2 round-trip preserves piece count");
    Expect(loaded.HasStartFinish(), "v2 round-trip preserves start/finish");
    std::remove(path.c_str());
}

void TestVariableStraightSurface() {
    Track track;
    const std::uint32_t id = track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4, 5, 9, 2, 2,
                                               SurfaceMaterial::Slippery);
    Expect(id != 0, "advanced straight is accepted");
    const TrackPiece* piece = track.GetPiece(id);
    Expect(piece != 0 && piece->ExitConnector().position == GridPosition{4, 2, 2},
           "straight exit applies elevation and lateral offset");
    Expect(piece != 0 && piece->ExitConnector().width == 9, "straight exit preserves endpoint width");
    const std::vector<TrackSurfaceSample> samples = piece->SurfaceSamples();
    Expect(samples.size() == 5 && samples.front().halfWidth == 2.5f && samples.back().halfWidth == 4.5f,
           "surface samples interpolate road width");
    const TrackContact contact = track.QuerySurface(2.0f, 1.0f, 1.0f);
    Expect(contact.found && contact.surface.pieceId == id && contact.surface.material == SurfaceMaterial::Slippery,
           "surface query finds the variable straight");
    Track guardrailTrack;
    guardrailTrack.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    const TrackContact rail = guardrailTrack.QuerySurface(2.0f, 0.16f, 2.7f);
    Expect(rail.found && rail.guardrailHit, "surface query reports a guardrail just beyond the road edge");
    const TrackContact broadRail = guardrailTrack.QuerySurface(2.0f, 0.16f, 3.4f, 1.4f);
    Expect(broadRail.found && broadRail.guardrailHit,
           "guardrail query keeps a broad enough contact band for fast movement");

    const std::string path = "/tmp/neon_racer_advanced_draft_test.draft";
    std::string error;
    Expect(DraftIO::Save(track, path, error), "v3 draft saves advanced straight parameters");
    Track loaded;
    Expect(DraftIO::Load(path, loaded, error), "v3 draft loads advanced straight parameters");
    const TrackPiece* restored = loaded.GetPiece(1);
    Expect(restored != 0 && restored->exitWidth == 9 && restored->elevationDelta == 2 &&
               restored->lateralOffset == 2 && restored->material == SurfaceMaterial::Slippery,
           "v3 round-trip preserves advanced straight parameters");
    std::remove(path.c_str());
}

void TestExtendedCurves() {
    Track leftQuarterTrack;
    const std::uint32_t leftQuarter = leftQuarterTrack.AddCurve(GridPosition{0, 0, 0}, Heading::East,
                                                                 CurveTurn::Left, 4);
    const TrackPiece* leftQuarterPiece = leftQuarterTrack.GetPiece(leftQuarter);
    Expect(leftQuarterPiece != 0 && leftQuarterPiece->ExitConnector().position == GridPosition{4, 0, -4} &&
               leftQuarterPiece->ExitConnector().heading == Heading::North,
           "left 90-degree curve connector matches its forward-rendered arc");

    Track track;
    const std::uint32_t halfTurn = track.AddCurve(GridPosition{0, 0, 0}, Heading::East, CurveTurn::Right, 4,
                                                  5, 5, 0, SurfaceMaterial::Regular, 180);
    const TrackPiece* halfPiece = track.GetPiece(halfTurn);
    Expect(halfPiece != 0 && halfPiece->ExitConnector().position == GridPosition{0, 0, 8} &&
               halfPiece->ExitConnector().heading == Heading::West,
           "180-degree curve produces an opposite-heading semicircle connector");

    Track quarterTrack;
    const std::uint32_t threeQuarter = quarterTrack.AddCurve(GridPosition{0, 0, 0}, Heading::East,
                                                              CurveTurn::Right, 4, 5, 5, 0,
                                                              SurfaceMaterial::Regular, 270);
    const TrackPiece* threeQuarterPiece = quarterTrack.GetPiece(threeQuarter);
    Expect(threeQuarterPiece != 0 && threeQuarterPiece->ExitConnector().position == GridPosition{-4, 0, 4} &&
               threeQuarterPiece->ExitConnector().heading == Heading::North,
           "270-degree curve produces the expected connector position and heading");

    Track bankedTrack;
    const std::uint32_t banked = bankedTrack.AddCurve(GridPosition{0, 0, 0}, Heading::East, CurveTurn::Right, 4,
                                                       5, 5, 0, SurfaceMaterial::Regular, 90, 45);
    const TrackPiece* bankedPiece = bankedTrack.GetPiece(banked);
    const std::vector<TrackSurfaceSample> samples = bankedPiece->SurfaceSamples();
    Expect(samples[samples.size() / 2].normalY < 0.8f && samples.front().normalY > 0.99f &&
               samples.back().normalY > 0.99f,
           "curve banking peaks at the midpoint and returns to level connectors");
}

void TestRoadSurfaceOverlap() {
    Track sameLevel;
    Expect(sameLevel.AddStraight(GridPosition{0, 0, 0}, Heading::East, 6) != 0,
           "first road for overlap test is accepted");
    Expect(sameLevel.AddStraight(GridPosition{3, 0, -3}, Heading::South, 6) == 0,
           "same-level road surfaces cannot cross");

    Track bridge;
    Expect(bridge.AddStraight(GridPosition{0, 0, 0}, Heading::East, 6) != 0,
           "lower bridge road is accepted");
    Expect(bridge.AddStraight(GridPosition{3, 3, -3}, Heading::South, 6) != 0,
           "elevated bridge road may pass over a lower road");
}

void TestConnectedCurveJoin() {
    const Track sample = Track::CreateSampleCircuit();
    Expect(sample.Pieces().size() == 8 && sample.Validate().raceReady,
           "connected straights and curves form a valid circuit without false overlap errors");
}

void TestConnectorCollections() {
    TrackPiece straight = Track::CreateSampleCircuit().Pieces().front();
    Expect(straight.EntryConnectors().size() == 1 && straight.ExitConnectors().size() == 1 &&
               straight.EntryConnectors().front().position == straight.EntryConnector().position &&
               straight.ExitConnectors().front().position == straight.ExitConnector().position,
           "single-path components expose their connectors through the graph-ready collection API");
    const Track sample = Track::CreateSampleCircuit();
    Expect(sample.Connections().size() == sample.Pieces().size(),
           "closed single-path circuit exposes one indexed graph connection per piece");
}

void TestBranchArmCollision() {
    Track track;
    Expect(track.AddBranch(GridPosition{0, 0, 0}, Heading::East, 6, 3) != 0,
           "branch is accepted as a draft component");
    Expect(track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 6, 5, 5, 0, 3) == 0,
           "branch arm geometry blocks an overlapping straight");
    const TrackContact rightArm = track.QuerySurface(3.0f, 0.16f, 1.5f);
    const TrackContact leftArm = track.QuerySurface(3.0f, 0.16f, -1.5f);
    Expect(rightArm.found && leftArm.found,
           "branch arms expose physical surface contacts for driving");
}

void TestBranchMergeGuardrailsFollowOuterBoundary() {
    Track branch;
    Expect(branch.AddBranch(GridPosition{0, 0, 0}, Heading::East, 6, 3) != 0,
           "branch is accepted for guardrail-boundary testing");
    const TrackContact branchRightInterior = branch.QuerySurface(4.0f, 0.16f, 1.0f);
    const TrackContact branchLeftInterior = branch.QuerySurface(4.0f, 0.16f, -1.0f);
    Expect(branchRightInterior.found && !branchRightInterior.guardrailHit &&
               branchLeftInterior.found && !branchLeftInterior.guardrailHit,
           "branch guardrails do not cross either neighboring arm's road");
    const TrackContact branchGap = branch.QuerySurface(6.0f, 0.16f, 0.0f);
    Expect(branchGap.found && branchGap.guardrailHit,
           "branch guardrails remain on the real gap between separated arms");

    Track merge;
    Expect(merge.AddMerge(GridPosition{0, 0, 0}, Heading::East, 6, 3) != 0,
           "merge is accepted for guardrail-boundary testing");
    const TrackContact mergeInterior = merge.QuerySurface(2.0f, 0.16f, 1.0f);
    Expect(mergeInterior.found && !mergeInterior.guardrailHit,
           "merge guardrails do not cross the neighboring arm's road");
}

void TestBranchUsesDistinctExitConnectors() {
    Track track;
    Expect(track.AddBranch(GridPosition{0, 0, 0}, Heading::East, 6, 3) != 0,
           "branch for connector validation is accepted");
    Expect(track.AddStraight(GridPosition{6, 0, -3}, Heading::East, 4) != 0,
           "first branch exit accepts its own onward piece");
    Expect(track.AddStraight(GridPosition{6, 0, 3}, Heading::East, 4) != 0,
           "second branch exit accepts its own onward piece");

    const TrackValidation validation = track.Validate();
    bool reportsAmbiguousExit = false;
    for (std::vector<TrackIssue>::const_iterator issue = validation.issues.begin();
         issue != validation.issues.end(); ++issue) {
        if (issue->message.find("more than one entry") != std::string::npos) reportsAmbiguousExit = true;
    }
    Expect(!reportsAmbiguousExit,
           "separate branch exits do not count as one exit matching multiple entries");
}

void TestBranchMergeDraftRoundTrip() {
    Track track;
    Expect(track.AddBranch(GridPosition{0, 0, 0}, Heading::East, 6, 3) != 0,
           "branch saves from a valid domain definition");
    Expect(track.AddMerge(GridPosition{20, 0, 0}, Heading::East, 6, 3) != 0,
           "merge saves from a valid domain definition");
    const std::string path = "/tmp/neon_racer_branch_merge_draft_test.draft";
    std::string error;
    Expect(DraftIO::Save(track, path, error), "branch and merge draft saves");
    Track loaded;
    Expect(DraftIO::Load(path, loaded, error) && loaded.Pieces().size() == 2 &&
               loaded.Pieces()[0].type == TrackPieceType::Branch && loaded.Pieces()[1].type == TrackPieceType::Merge,
           "branch and merge retain their distinct types after loading");
    std::remove(path.c_str());
}

void TestPlayableExport() {
    PlayableTrack playable;
    std::string error;
    Track incomplete;
    incomplete.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    Expect(!PlayableExport::Build(incomplete, "Incomplete", playable, error),
           "playable export rejects an incomplete editable draft");

    const Track raceReady = Track::CreateSampleCircuit();
    Expect(PlayableExport::Build(raceReady, "Night Run", playable, error) &&
               playable.metadata.name == "Night Run" && playable.metadata.playableExportVersion == 1 &&
               playable.sourceLayoutRevision == raceReady.LayoutRevision() && playable.layout.Validate().raceReady,
           "playable export creates a frozen race-ready layout snapshot");
}

void TestLoopDraftRoundTrip() {
    Track loopTrack;
    Expect(loopTrack.AddLoop(GridPosition{0, 0, 0}, Heading::East, 4, 5, 5, 0,
                             SurfaceMaterial::Regular, 3) != 0, "loop is accepted by the track domain");
    const std::string path = "/tmp/neon_racer_loop_draft_test.draft";
    std::string error;
    Expect(DraftIO::Save(loopTrack, path, error), "loop draft saves");
    Track loaded;
    Expect(DraftIO::Load(path, loaded, error) && loaded.Pieces().size() == 1 &&
               loaded.Pieces().front().type == TrackPieceType::Loop && loaded.Pieces().front().lateralOffset == 3 &&
               loaded.Pieces().front().ExitConnector().position == GridPosition{0, 0, 3},
           "loop draft loads as a loop component");
    const std::vector<TrackSurfaceSample> samples = loaded.Pieces().front().SurfaceSamples();
    Expect(samples.front().normalY > 0.99f && samples[samples.size() / 2].normalY < -0.99f &&
               samples[samples.size() / 4].normalY > -0.1f && samples[samples.size() / 4].normalY < 0.1f,
           "loop surface stays well-defined through vertical and inverted sections");
    std::remove(path.c_str());
}

} // namespace

int main() {
    TestSurfaceContracts();
    TestEmptyTrackClearInvariant();
    TestDraftV1Load();
    TestDraftV2RoundTrip();
    TestVariableStraightSurface();
    TestExtendedCurves();
    TestRoadSurfaceOverlap();
    TestConnectedCurveJoin();
    TestConnectorCollections();
    TestBranchArmCollision();
    TestBranchMergeGuardrailsFollowOuterBoundary();
    TestBranchUsesDistinctExitConnectors();
    TestBranchMergeDraftRoundTrip();
    TestPlayableExport();
    TestLoopDraftRoundTrip();
    if (failures == 0) std::cout << "Neon Racer track tests passed.\n";
    return failures == 0 ? 0 : 1;
}
