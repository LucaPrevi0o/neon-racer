#include "../../src/persistence/draft_io.hpp"
#include "../../src/persistence/track_layout_codec.hpp"
#include "../../src/track/track_fingerprint.hpp"
#include "../../src/track/track.hpp"
#include "../../src/track/track_road_geometry.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

bool NearlyEqual(float first, float second) {
    return std::fabs(first - second) < 0.0001f;
}

void TestSurfaceContracts() {
    TrackSurfaceSample sample = {1.0f, 2.0f, 3.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f, 2.5f, SurfaceMaterial::Regular, 7};
    TrackContact contact = {true, sample, 0.25f, false};
    TrackMetadata metadata;
    VerificationState verification;
    Expect(contact.found && contact.surface.pieceId == 7, "surface/contact contract stores a sampled piece");
    Expect(metadata.playableExportVersion == 0, "metadata starts at export version zero");
    Expect(!verification.hasSavedGhost && !verification.isVerifiedForPlayableExport &&
               verification.verifiedLayoutFingerprint == 0,
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

void TestDraftRejectsDuplicatePieceIds() {
    const std::string path = "/tmp/neon_racer_duplicate_piece_ids.draft";
    {
        std::ofstream file(path.c_str());
        file << "NEON_RACER_DRAFT 5\n";
        file << "STATUS DRAFT\n";
        file << "START 0 0\n";
        file << "PIECES 2\n";
        file << "PIECE 1 0 0 0 0 1 5 5 4 1 0 0 0 0 90 0\n";
        file << "PIECE 1 0 10 0 0 1 5 5 4 1 0 0 0 0 90 0\n";
    }
    Track unchanged = Track::CreateSampleCircuit();
    const std::size_t originalPieceCount = unchanged.Pieces().size();
    std::string error;
    Expect(!DraftIO::Load(path, unchanged, error) && unchanged.Pieces().size() == originalPieceCount,
           "duplicate serialized piece IDs are rejected without replacing a loaded track");
    std::remove(path.c_str());
}

void TestTrackNumericSafetyLimits() {
    Track bounded;
    Expect(bounded.AddStraight(GridPosition{TrackLimits::kMaximumGridCoordinate - 20, 0, 0}, Heading::East, 20) != 0,
           "the documented grid envelope allows a practical far-edge piece");
    Expect(bounded.AddStraight(GridPosition{TrackLimits::kMaximumGridCoordinate + 1, 0, 0}, Heading::East, 4) == 0,
           "the track domain rejects positions beyond its safe grid envelope");

    TrackPiece malformed = bounded.Pieces().front();
    malformed.lateralOffset = std::numeric_limits<int>::min();
    Expect(bounded.HasOverlappingGeometry(malformed),
           "malformed placement candidates are blocked before their geometry is evaluated");
}

void TestDraftRejectsUnsafeNumericFields() {
    const std::string path = "/tmp/neon_racer_unsafe_numeric_draft.draft";
    const std::string minimum = std::to_string(std::numeric_limits<int>::min());
    const std::string maximum = std::to_string(std::numeric_limits<int>::max());
    const std::vector<std::string> invalidPieces = {
        "PIECE 1 0 0 0 0 1 5 5 4 1 0 0 " + minimum + " 0 90 0\n",
        "PIECE 1 1 0 0 0 1 5 5 0 1 4 0 0 0 90 " + minimum + "\n",
        "PIECE 1 0 " + maximum + " 0 0 1 5 5 4 1 0 0 0 0 90 0\n",
        "PIECE 1 0 0 " + maximum + " 0 1 5 5 4 1 0 " + maximum + " 0 0 90 0\n",
    };

    for (std::size_t index = 0; index < invalidPieces.size(); ++index) {
        {
            std::ofstream file(path.c_str());
            file << "NEON_RACER_DRAFT 5\nSTATUS DRAFT\nSTART 0 0\nPIECES 1\n" << invalidPieces[index];
        }
        Track destination = Track::CreateSampleCircuit();
        const std::uint64_t destinationFingerprint = TrackFingerprint::Calculate(destination);
        std::string error;
        Expect(!DraftIO::Load(path, destination, error) &&
                   TrackFingerprint::Calculate(destination) == destinationFingerprint,
               "unsafe serialized integer fields are rejected transactionally before geometry arithmetic");
    }
    std::remove(path.c_str());
}

void TestPersistedPieceLimit() {
    const std::string path = "/tmp/neon_racer_oversized_draft.draft";
    {
        std::ofstream file(path.c_str());
        file << "NEON_RACER_DRAFT 5\nSTATUS DRAFT\nSTART 0 0\nPIECES "
             << (TrackLayoutCodec::kMaximumPieceCount + 1u) << "\n";
    }
    Track destination = Track::CreateSampleCircuit();
    const std::uint64_t destinationFingerprint = TrackFingerprint::Calculate(destination);
    std::string error;
    Expect(!DraftIO::Load(path, destination, error) &&
               TrackFingerprint::Calculate(destination) == destinationFingerprint,
           "an oversized persisted layout is rejected before component loading work begins");
    std::remove(path.c_str());

    Track oversized;
    for (std::size_t index = 0; index <= TrackLayoutCodec::kMaximumPieceCount; ++index) {
        Expect(oversized.AddStraight(GridPosition{static_cast<int>(index * 100u), 0, 0}, Heading::East, 4) != 0,
               "oversized-save setup can create separate valid pieces");
    }
    const Track priorDraft = Track::CreateSampleCircuit();
    const std::uint64_t priorFingerprint = TrackFingerprint::Calculate(priorDraft);
    Expect(DraftIO::Save(priorDraft, path, error), "oversized-save setup stores a prior valid draft");
    Track preserved;
    Expect(!DraftIO::Save(oversized, path, error) && DraftIO::Load(path, preserved, error) &&
               TrackFingerprint::Calculate(preserved) == priorFingerprint,
           "an oversized save is rejected before it can clobber an existing draft");
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

void TestSharedRoadGeometry() {
    Track branchTrack;
    const std::uint32_t branchId = branchTrack.AddBranch(GridPosition{0, 0, 0}, Heading::East, 6, 3);
    const TrackPiece* branch = branchTrack.GetPiece(branchId);
    Expect(branch != 0, "branch setup succeeds for shared road geometry");
    if (branch != 0) {
        const std::vector<TrackPiece> branchArms = TrackRoadGeometry::PhysicalRoadArms(*branch);
        Expect(branchArms.size() == 2 && branchArms[0].type == TrackPieceType::Straight &&
                   branchArms[1].type == TrackPieceType::Straight && branchArms[0].id == branchId &&
                   branchArms[1].id == branchId && branchArms[0].entryPosition == branch->entryPosition &&
                   branchArms[0].lateralOffset == branch->lateralOffset &&
                   branchArms[1].lateralOffset == -branch->lateralOffset,
               "branch expansion owns both physical straight arms in stable order");
        const std::vector<TrackSurfaceSample> branchSamples =
            TrackRoadGeometry::PhysicalRoadSurfaceSamples(*branch);
        Expect(branchSamples.size() == branchArms[0].SurfaceSamples().size() + branchArms[1].SurfaceSamples().size() &&
                   branchSamples.front().pieceId == branchId && branchSamples.back().pieceId == branchId,
               "physical branch samples preserve both drivable arms and their component ID");
    }

    Track mergeTrack;
    const std::uint32_t mergeId = mergeTrack.AddMerge(GridPosition{20, 0, 0}, Heading::East, 6, 3);
    const TrackPiece* merge = mergeTrack.GetPiece(mergeId);
    Expect(merge != 0, "merge setup succeeds for shared road geometry");
    if (merge != 0) {
        const std::vector<TrackConnector> entries = merge->EntryConnectors();
        const std::vector<TrackPiece> mergeArms = TrackRoadGeometry::PhysicalRoadArms(*merge);
        Expect(mergeArms.size() == 2 && mergeArms[0].type == TrackPieceType::Straight &&
                   mergeArms[1].type == TrackPieceType::Straight && mergeArms[0].entryPosition == entries[0].position &&
                   mergeArms[1].entryPosition == entries[1].position && mergeArms[0].lateralOffset == -3 &&
                   mergeArms[1].lateralOffset == 3,
               "merge expansion follows its entry connector order and converges both arms");

        TrackPiece negativeOffsetMerge = *merge;
        negativeOffsetMerge.lateralOffset = -4;
        const std::vector<TrackPiece> negativeOffsetArms = TrackRoadGeometry::PhysicalRoadArms(negativeOffsetMerge);
        Expect(negativeOffsetArms.size() == 2 && negativeOffsetArms[0].lateralOffset == -4 &&
                   negativeOffsetArms[1].lateralOffset == 4,
               "merge expansion normalizes arm sign independently of the stored offset sign");
    }

    const TrackSurfaceSample tilted = {0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 4.0f,
                                       0.0f, 2.0f, 0.0f, 2.5f, SurfaceMaterial::Regular, 1};
    const TrackRoadAxis rawAxis = TrackRoadGeometry::SurfaceRightAxis(tilted);
    TrackRoadAxis normalizedAxis;
    Expect(NearlyEqual(rawAxis.x, -8.0f) && NearlyEqual(rawAxis.y, 0.0f) && NearlyEqual(rawAxis.z, 6.0f) &&
               TrackRoadGeometry::NormalizedSurfaceRightAxis(tilted, normalizedAxis) &&
               NearlyEqual(normalizedAxis.x, -0.8f) && NearlyEqual(normalizedAxis.y, 0.0f) &&
               NearlyEqual(normalizedAxis.z, 0.6f),
           "shared road-axis helpers retain the sampled frame and provide a unit contact axis");
    TrackSurfaceSample degenerate = tilted;
    degenerate.tangentX = degenerate.tangentY = degenerate.tangentZ = 0.0f;
    Expect(!TrackRoadGeometry::NormalizedSurfaceRightAxis(degenerate, normalizedAxis),
           "shared road-axis helpers reject a degenerate surface frame");
}

void TestTrackFingerprint() {
    const Track sample = Track::CreateSampleCircuit();
    const std::uint64_t sampleFingerprint = TrackFingerprint::Calculate(sample);

    Track sameLayoutWithDifferentIds;
    const std::uint32_t discardedId = sameLayoutWithDifferentIds.AddStraight(GridPosition{100, 0, 100}, Heading::East, 4);
    Expect(discardedId != 0 && sameLayoutWithDifferentIds.RemovePiece(discardedId),
           "fingerprint setup can advance generated IDs without retaining geometry");
    for (std::vector<TrackPiece>::const_iterator piece = sample.Pieces().begin(); piece != sample.Pieces().end(); ++piece) {
        Expect(sameLayoutWithDifferentIds.Add(*piece) != 0,
               "fingerprint setup reconstructs every semantic piece with regenerated IDs");
    }
    std::size_t startOrdinal = sample.Pieces().size();
    for (std::size_t index = 0; index < sample.Pieces().size(); ++index) {
        if (sample.Pieces()[index].id == sample.StartFinishPieceId()) {
            startOrdinal = index;
            break;
        }
    }
    Expect(sameLayoutWithDifferentIds.SetStartFinish(sameLayoutWithDifferentIds.Pieces()[startOrdinal].id,
                                                     sample.SelectedRaceDirection()),
           "fingerprint setup restores the start/finish semantic by ordinal");
    Expect(sameLayoutWithDifferentIds.Pieces().front().id != sample.Pieces().front().id &&
               sameLayoutWithDifferentIds.LayoutRevision() != sample.LayoutRevision() &&
               TrackFingerprint::Calculate(sameLayoutWithDifferentIds) == sampleFingerprint,
           "fingerprint ignores regenerated IDs and runtime layout revisions");

    Track sameStateAfterRevision = sample;
    Expect(sameStateAfterRevision.SetStartFinish(sameStateAfterRevision.StartFinishPieceId(),
                                                 sameStateAfterRevision.SelectedRaceDirection()) &&
               sameStateAfterRevision.LayoutRevision() != sample.LayoutRevision() &&
               TrackFingerprint::Calculate(sameStateAfterRevision) == sampleFingerprint,
           "fingerprint remains stable when an unchanged start/finish setting advances the revision");

    Track reverseDirection = sample;
    Expect(reverseDirection.SetStartFinish(reverseDirection.StartFinishPieceId(), RaceDirection::Reverse) &&
               TrackFingerprint::Calculate(reverseDirection) != sampleFingerprint,
           "fingerprint includes race direction");

    Track differentStart = sample;
    Expect(differentStart.SetStartFinish(differentStart.Pieces()[2].id, sample.SelectedRaceDirection()) &&
               TrackFingerprint::Calculate(differentStart) != sampleFingerprint,
           "fingerprint includes the selected start/finish piece ordinal");

    Track changedPiece = sample;
    TrackPiece replacement = changedPiece.Pieces().front();
    replacement.material = SurfaceMaterial::Slippery;
    Expect(changedPiece.ReplacePiece(replacement) && TrackFingerprint::Calculate(changedPiece) != sampleFingerprint,
           "fingerprint includes persisted piece fields");

    Track firstOrder;
    firstOrder.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    firstOrder.AddStraight(GridPosition{20, 0, 0}, Heading::East, 4);
    Track reversedOrder;
    reversedOrder.AddStraight(GridPosition{20, 0, 0}, Heading::East, 4);
    reversedOrder.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    Expect(TrackFingerprint::Calculate(firstOrder) != TrackFingerprint::Calculate(reversedOrder),
           "fingerprint includes persisted piece order");
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
    TestDraftRejectsDuplicatePieceIds();
    TestTrackNumericSafetyLimits();
    TestDraftRejectsUnsafeNumericFields();
    TestPersistedPieceLimit();
    TestVariableStraightSurface();
    TestExtendedCurves();
    TestRoadSurfaceOverlap();
    TestConnectedCurveJoin();
    TestConnectorCollections();
    TestBranchArmCollision();
    TestBranchMergeGuardrailsFollowOuterBoundary();
    TestBranchUsesDistinctExitConnectors();
    TestBranchMergeDraftRoundTrip();
    TestSharedRoadGeometry();
    TestTrackFingerprint();
    TestLoopDraftRoundTrip();
    if (failures == 0) std::cout << "Neon Racer track tests passed.\n";
    return failures == 0 ? 0 : 1;
}
