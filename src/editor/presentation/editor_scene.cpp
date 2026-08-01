#include "neon_racer/editor/editor.hpp"

#include "editor_connector_guides.hpp"
#include "render/track_renderer.hpp"
#include "ui/neon.hpp"

#include <cmath>

namespace {

Vector3 HeadingVector(Heading heading) {
    switch (heading) {
    case Heading::North: return Vector3{0.0f, 0.0f, -1.0f};
    case Heading::East: return Vector3{1.0f, 0.0f, 0.0f};
    case Heading::South: return Vector3{0.0f, 0.0f, 1.0f};
    case Heading::West: return Vector3{-1.0f, 0.0f, 0.0f};
    }
    return Vector3{0.0f, 0.0f, 0.0f};
}

Color PieceColor(const TrackPiece& piece, std::uint32_t selectedId) {
    if (selectedId != 0 && piece.id == selectedId) return Neon::Yellow;
    return piece.type == TrackPieceType::Straight ? Neon::Pink : Neon::Cyan;
}

void DrawPieceCells(const TrackPiece& piece, std::uint32_t selectedId, float alpha) {
    const Color color = Fade(PieceColor(piece, selectedId), alpha);
    DrawTrackPieceSurface(piece, Fade(Neon::Panel, alpha), color);
}

void DrawConnectorGuide(const TrackConnector& connector, Color color) {
    const Vector3 origin{static_cast<float>(connector.position.x),
                         static_cast<float>(connector.position.y) + 0.48f,
                         static_cast<float>(connector.position.z)};
    const Vector3 direction = HeadingVector(connector.heading);
    const Vector3 tip{origin.x + direction.x * 0.70f, origin.y, origin.z + direction.z * 0.70f};
    DrawSphere(origin, 0.16f, color);
    DrawLine3D(origin, tip, color);
    DrawSphere(tip, 0.09f, color);
}

void DrawConnectorGuides(const std::vector<TrackConnector>& connectors, Color color) {
    for (std::vector<TrackConnector>::const_iterator connector = connectors.begin();
         connector != connectors.end(); ++connector) {
        DrawConnectorGuide(*connector, color);
    }
}

void DrawConnectorMarkers(const std::vector<TrackConnector>& connectors, Color color) {
    for (std::vector<TrackConnector>::const_iterator connector = connectors.begin();
         connector != connectors.end(); ++connector) {
        DrawSphere(Vector3{static_cast<float>(connector->position.x),
                           static_cast<float>(connector->position.y) + 0.42f,
                           static_cast<float>(connector->position.z)},
                   0.22f, color);
    }
}

void DrawPieceConnectorGuides(const TrackPiece& piece) {
    const EditorPresentation::ConnectorGuideSet guides =
        EditorPresentation::ConnectorGuidesFor(piece);
    DrawConnectorGuides(guides.entries, Neon::Cyan);
    DrawConnectorGuides(guides.exits, Neon::Pink);
}

} // namespace

void TrackEditor::DrawTrack3D() const {
    for (std::vector<TrackPiece>::const_iterator piece = track_.Pieces().begin();
         piece != track_.Pieces().end(); ++piece) {
        if (piece->id == selectedPieceId_) continue;
        DrawPieceCells(*piece, selectedPieceId_, 0.96f);
        DrawPieceConnectorGuides(*piece);
    }

    if (selectedPieceId_ != 0) {
        const TrackPiece* original = track_.GetPiece(selectedPieceId_);
        if (original != 0) {
            const float pulse = 0.16f + 0.14f *
                (0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 5.0f));
            DrawPieceCells(*original, 0, pulse);
        }
    }

    if (track_.HasStartFinish()) {
        const TrackPiece* start = track_.GetPiece(track_.StartFinishPieceId());
        if (start != 0) {
            const GridPosition position = start->EntryConnector().position;
            DrawCube(Vector3{static_cast<float>(position.x), static_cast<float>(position.y) + 0.34f,
                             static_cast<float>(position.z)},
                     0.92f, 0.08f, 0.92f, Neon::Green);
        }
    }

    const TrackValidation validation = track_.Validate();
    for (std::vector<TrackIssue>::const_iterator issue = validation.issues.begin();
         issue != validation.issues.end(); ++issue) {
        for (std::vector<std::uint32_t>::const_iterator id = issue->affectedPieceIds.begin();
             id != issue->affectedPieceIds.end(); ++id) {
            const TrackPiece* affected = track_.GetPiece(*id);
            if (affected == 0) continue;
            const GridPosition position = affected->EntryConnector().position;
            DrawSphere(Vector3{static_cast<float>(position.x), static_cast<float>(position.y) + 0.72f,
                               static_cast<float>(position.z)},
                       0.25f, Neon::Orange);
        }
    }

    const TrackPiece candidate = BuildPreview();
    const bool overlaps = PreviewOverlaps(candidate);
    DrawPieceCells(candidate, selectedPieceId_, overlaps ? 0.28f : 0.52f);
    DrawPieceConnectorGuides(candidate);

    const EditorPresentation::ConnectorGuideSet candidateGuides =
        EditorPresentation::ConnectorGuidesFor(candidate);
    const Color connectorColor = overlaps ? Neon::Orange : Neon::Green;
    DrawConnectorMarkers(candidateGuides.entries, connectorColor);
    DrawConnectorMarkers(candidateGuides.exits, connectorColor);

    if (trackingGraphVisible_) DrawTrackingGraph3D();
}
