#include "neon_racer/editor/editor.hpp"

#include "track/track_progress_graph.hpp"
#include "ui/neon.hpp"

#include <map>
#include <set>
#include <vector>

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

Vector3 Add(Vector3 first, Vector3 second) {
    return Vector3{first.x + second.x, first.y + second.y, first.z + second.z};
}

Vector3 Subtract(Vector3 first, Vector3 second) {
    return Vector3{first.x - second.x, first.y - second.y, first.z - second.z};
}

Vector3 Scale(Vector3 vector, float amount) {
    return Vector3{vector.x * amount, vector.y * amount, vector.z * amount};
}

Vector3 Lerp(Vector3 from, Vector3 to, float amount) {
    return Add(from, Scale(Subtract(to, from), amount));
}

Vector3 ConnectorPoint(const TrackConnector& connector) {
    return Vector3{static_cast<float>(connector.position.x),
                   static_cast<float>(connector.position.y),
                   static_cast<float>(connector.position.z)};
}

Vector3 NodePosition(const TrackPiece& piece) {
    const std::vector<TrackConnector> entries = piece.EntryConnectors();
    const std::vector<TrackConnector> exits = piece.ExitConnectors();
    Vector3 total{0.0f, 0.0f, 0.0f};
    std::size_t count = 0;
    for (std::vector<TrackConnector>::const_iterator connector = entries.begin();
         connector != entries.end(); ++connector) {
        total = Add(total, ConnectorPoint(*connector));
        ++count;
    }
    for (std::vector<TrackConnector>::const_iterator connector = exits.begin();
         connector != exits.end(); ++connector) {
        total = Add(total, ConnectorPoint(*connector));
        ++count;
    }
    if (count == 0) return Vector3{0.0f, 1.65f, 0.0f};
    return Add(Scale(total, 1.0f / static_cast<float>(count)), Vector3{0.0f, 1.65f, 0.0f});
}

void DrawDirectedEdge(Vector3 from, Vector3 to, Color color) {
    const Vector3 bodyEnd = Lerp(from, to, 0.78f);
    DrawCylinderEx(from, bodyEnd, 0.035f, 0.035f, 7, color);
    const Vector3 arrowBase = Lerp(from, to, 0.70f);
    const Vector3 arrowTip = Lerp(from, to, 0.88f);
    DrawCylinderEx(arrowBase, arrowTip, 0.15f, 0.0f, 8, color);
}

void DrawPortal(const TrackProgressPortal& portal, Color color) {
    const Vector3 forward = HeadingVector(portal.crossingHeading);
    const Vector3 side{-forward.z, 0.0f, forward.x};
    const float halfWidth = static_cast<float>(portal.connector.width) * 0.5f;
    const Vector3 base = Add(ConnectorPoint(portal.connector), Vector3{0.0f, 0.08f, 0.0f});
    const Vector3 top = Add(base, Vector3{0.0f, 2.35f, 0.0f});
    const Vector3 lowerLeft = Add(base, Scale(side, -halfWidth));
    const Vector3 lowerRight = Add(base, Scale(side, halfWidth));
    const Vector3 upperLeft = Add(top, Scale(side, -halfWidth));
    const Vector3 upperRight = Add(top, Scale(side, halfWidth));
    const Color fill = Fade(color, 0.13f);

    DrawTriangle3D(lowerLeft, lowerRight, upperRight, fill);
    DrawTriangle3D(lowerLeft, upperRight, upperLeft, fill);
    DrawTriangle3D(upperRight, lowerRight, lowerLeft, fill);
    DrawTriangle3D(upperLeft, upperRight, lowerLeft, fill);
    DrawLine3D(lowerLeft, lowerRight, color);
    DrawLine3D(lowerRight, upperRight, color);
    DrawLine3D(upperRight, upperLeft, color);
    DrawLine3D(upperLeft, lowerLeft, color);

    const Vector3 center = Add(Scale(Add(base, top), 0.5f), Scale(forward, -0.62f));
    const Vector3 tip = Add(center, Scale(forward, 1.24f));
    DrawCylinderEx(center, tip, 0.10f, 0.0f, 8, color);
}

Color NodeColor(const TrackPiece& piece, bool startFinish, bool connected) {
    if (startFinish) return Neon::Green;
    if (!connected) return Neon::Orange;
    if (piece.type == TrackPieceType::Branch || piece.type == TrackPieceType::Merge) return Neon::Yellow;
    return Neon::Cyan;
}

} // namespace

void TrackEditor::DrawTrackingGraph3D() const {
    const TrackProgressGraph graph = BuildTrackProgressGraph(track_);
    std::map<std::uint32_t, Vector3> nodes;
    std::set<std::uint32_t> connectedPieceIds;

    for (std::vector<TrackProgressTransition>::const_iterator transition = graph.transitions.begin();
         transition != graph.transitions.end(); ++transition) {
        connectedPieceIds.insert(transition->fromPieceId);
        connectedPieceIds.insert(transition->toPieceId);
    }
    if (graph.hasStartFinish) connectedPieceIds.insert(graph.startFinishPieceId);

    for (std::vector<TrackPiece>::const_iterator piece = track_.Pieces().begin();
         piece != track_.Pieces().end(); ++piece) {
        nodes[piece->id] = NodePosition(*piece);
    }

    for (std::vector<TrackProgressTransition>::const_iterator transition = graph.transitions.begin();
         transition != graph.transitions.end(); ++transition) {
        const std::map<std::uint32_t, Vector3>::const_iterator from = nodes.find(transition->fromPieceId);
        const std::map<std::uint32_t, Vector3>::const_iterator to = nodes.find(transition->toPieceId);
        if (from != nodes.end() && to != nodes.end()) {
            DrawDirectedEdge(from->second, to->second, Fade(Neon::Pink, 0.78f));
        }
        DrawPortal(transition->portal, Neon::Yellow);
    }

    if (graph.hasStartFinish) DrawPortal(graph.finishPortal, Neon::Green);

    for (std::vector<TrackPiece>::const_iterator piece = track_.Pieces().begin();
         piece != track_.Pieces().end(); ++piece) {
        const Vector3 node = nodes[piece->id];
        const bool startFinish = graph.hasStartFinish && piece->id == graph.startFinishPieceId;
        const bool connected = connectedPieceIds.find(piece->id) != connectedPieceIds.end();
        const Color color = NodeColor(*piece, startFinish, connected);
        DrawLine3D(Add(node, Vector3{0.0f, -1.20f, 0.0f}), node, Fade(color, 0.55f));
        DrawSphere(node, startFinish ? 0.30f : 0.24f, Fade(color, 0.86f));
        DrawSphereWires(node, startFinish ? 0.38f : 0.31f, 8, 8, color);
    }
}
