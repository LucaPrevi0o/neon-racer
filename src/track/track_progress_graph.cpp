#include "track_progress_graph.hpp"
#include "track_sector_layout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace {

const std::size_t kMaximumRouteVariants = 4096u;

typedef std::map<std::uint32_t, std::vector<const TrackProgressTransition*> > TransitionMap;

struct EnumeratedRoute {
    std::vector<const TrackProgressTransition*> transitions;
    float length;
};

struct EnumerationState {
    std::vector<EnumeratedRoute> routes;
    bool unsupportedCycle;
    bool tooManyRoutes;

    EnumerationState() : routes(), unsupportedCycle(false), tooManyRoutes(false) {}
};

Heading OppositeHeading(Heading heading) {
    switch (heading) {
    case Heading::North: return Heading::South;
    case Heading::East: return Heading::West;
    case Heading::South: return Heading::North;
    case Heading::West: return Heading::East;
    }
    return Heading::North;
}

TrackProgressPortal PortalFor(const TrackConnector& connector, RaceDirection direction) {
    return TrackProgressPortal{
        connector,
        direction == RaceDirection::Forward ? connector.heading : OppositeHeading(connector.heading),
    };
}

float Distance(const TrackPathPoint& first, const TrackPathPoint& second) {
    const float dx = second.x - first.x;
    const float dy = second.y - first.y;
    const float dz = second.z - first.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

TransitionMap BuildTransitionMap(const TrackProgressGraph& graph) {
    TransitionMap transitions;
    for (std::vector<TrackProgressTransition>::const_iterator transition = graph.transitions.begin();
         transition != graph.transitions.end(); ++transition) {
        transitions[transition->fromPieceId].push_back(&(*transition));
    }
    return transitions;
}

void EnumerateLapRoutes(const Track& track,
                        const TransitionMap& transitionMap,
                        std::uint32_t startPieceId,
                        std::uint32_t currentPieceId,
                        std::set<std::uint32_t>& visitedPieceIds,
                        std::vector<const TrackProgressTransition*>& currentRoute,
                        float currentLength,
                        EnumerationState& state) {
    if (state.tooManyRoutes) return;

    const TrackPiece* currentPiece = track.GetPiece(currentPieceId);
    if (currentPiece == 0) return;
    const float nextLength = currentLength + TrackRouteLengthForPiece(*currentPiece);

    const TransitionMap::const_iterator outgoing = transitionMap.find(currentPieceId);
    if (outgoing == transitionMap.end()) return;

    for (std::vector<const TrackProgressTransition*>::const_iterator transition = outgoing->second.begin();
         transition != outgoing->second.end(); ++transition) {
        currentRoute.push_back(*transition);
        if ((*transition)->toPieceId == startPieceId) {
            if (state.routes.size() >= kMaximumRouteVariants) {
                state.tooManyRoutes = true;
                currentRoute.pop_back();
                return;
            }
            state.routes.push_back(EnumeratedRoute{currentRoute, nextLength});
        } else if (visitedPieceIds.find((*transition)->toPieceId) != visitedPieceIds.end()) {
            // A legal lap could circulate through this sub-cycle before
            // returning to the finish. That produces an unbounded family of
            // route variants, so automatic sectors must reject it explicitly.
            state.unsupportedCycle = true;
        } else {
            visitedPieceIds.insert((*transition)->toPieceId);
            EnumerateLapRoutes(track, transitionMap, startPieceId, (*transition)->toPieceId,
                               visitedPieceIds, currentRoute, nextLength, state);
            visitedPieceIds.erase((*transition)->toPieceId);
        }
        currentRoute.pop_back();
    }
}

bool SelectBoundaryIndices(const Track& track,
                           const EnumeratedRoute& route,
                           std::size_t& firstBoundary,
                           std::size_t& secondBoundary) {
    const std::size_t transitionCount = route.transitions.size();
    if (transitionCount < 3u || route.length <= 0.0f || !std::isfinite(route.length)) return false;

    std::vector<float> cumulativeDistances;
    cumulativeDistances.reserve(transitionCount);
    float cumulative = 0.0f;
    for (std::vector<const TrackProgressTransition*>::const_iterator transition = route.transitions.begin();
         transition != route.transitions.end(); ++transition) {
        const TrackPiece* piece = track.GetPiece((*transition)->fromPieceId);
        if (piece == 0) return false;
        cumulative += TrackRouteLengthForPiece(*piece);
        cumulativeDistances.push_back(cumulative);
    }

    const float firstTarget = route.length / 3.0f;
    const float secondTarget = route.length * 2.0f / 3.0f;
    float bestError = std::numeric_limits<float>::max();
    bool found = false;

    // Leave at least one transition between the second split and the finish,
    // while ensuring the two intermediate boundaries are always ordered and
    // distinct even on the smallest supported circuit.
    for (std::size_t first = 0; first + 2u < transitionCount; ++first) {
        for (std::size_t second = first + 1u; second + 1u < transitionCount; ++second) {
            const float error = std::fabs(cumulativeDistances[first] - firstTarget) +
                                std::fabs(cumulativeDistances[second] - secondTarget);
            if (!found || error < bestError) {
                found = true;
                bestError = error;
                firstBoundary = first;
                secondBoundary = second;
            }
        }
    }
    return found;
}

void AddBoundaryGate(TrackSectorBoundary& boundary, const TrackProgressTransition& transition) {
    for (std::vector<TrackSectorGate>::const_iterator gate = boundary.alternativeGates.begin();
         gate != boundary.alternativeGates.end(); ++gate) {
        if (gate->transitionId == transition.id) return;
    }
    boundary.alternativeGates.push_back(TrackSectorGate{
        transition.id,
        transition.fromPieceId,
        transition.toPieceId,
        transition.portal,
    });
}

bool GateOrder(const TrackSectorGate& first, const TrackSectorGate& second) {
    return first.transitionId < second.transitionId;
}

TrackSectorLayout FailedLayout(TrackSectorLayoutStatus status) {
    TrackSectorLayout layout;
    layout.status = status;
    return layout;
}

} // namespace

float TrackRouteLengthForPiece(const TrackPiece& piece) {
    if (piece.type == TrackPieceType::Branch || piece.type == TrackPieceType::Merge) {
        const float forward = static_cast<float>(piece.length);
        const float lateral = static_cast<float>(std::abs(piece.lateralOffset));
        const float vertical = static_cast<float>(piece.elevationDelta);
        return std::sqrt(forward * forward + lateral * lateral + vertical * vertical);
    }

    const std::vector<TrackPathPoint> points = piece.PathPoints();
    float length = 0.0f;
    for (std::size_t index = 1; index < points.size(); ++index) {
        length += Distance(points[index - 1], points[index]);
    }
    return length;
}

TrackProgressGraph BuildTrackProgressGraph(const Track& track) {
    TrackProgressGraph graph = TrackProgressGraph{
        track.SelectedRaceDirection(),
        std::vector<TrackProgressTransition>(),
        false,
        0,
        TrackProgressPortal{TrackConnector{GridPosition{0, 0, 0}, Heading::North, 0}, Heading::North},
    };

    const bool forward = graph.direction == RaceDirection::Forward;
    const std::vector<TrackConnection> connections = track.Connections();
    for (std::vector<TrackConnection>::const_iterator connection = connections.begin();
         connection != connections.end(); ++connection) {
        const std::uint32_t fromPieceId = forward ? connection->exit.pieceId : connection->entry.pieceId;
        const std::uint32_t toPieceId = forward ? connection->entry.pieceId : connection->exit.pieceId;
        const TrackPiece* fromPiece = track.GetPiece(fromPieceId);
        const TrackPiece* toPiece = track.GetPiece(toPieceId);
        if (fromPiece == 0 || toPiece == 0) continue;

        const std::vector<TrackConnector> portals = forward ? fromPiece->ExitConnectors() :
            fromPiece->EntryConnectors();
        const std::vector<TrackConnector> arrivals = forward ? toPiece->EntryConnectors() :
            toPiece->ExitConnectors();
        const std::size_t portalIndex = forward ? connection->exit.connectorIndex :
            connection->entry.connectorIndex;
        const std::size_t arrivalIndex = forward ? connection->entry.connectorIndex :
            connection->exit.connectorIndex;
        if (portalIndex >= portals.size() || arrivalIndex >= arrivals.size()) continue;

        graph.transitions.push_back(TrackProgressTransition{
            fromPieceId,
            toPieceId,
            PortalFor(portals[portalIndex], graph.direction),
            arrivals[arrivalIndex],
            static_cast<std::uint32_t>(graph.transitions.size() + 1u),
        });
    }

    if (track.HasStartFinish()) {
        const TrackPiece* startPiece = track.GetPiece(track.StartFinishPieceId());
        if (startPiece != 0) {
            graph.hasStartFinish = true;
            graph.startFinishPieceId = startPiece->id;
            graph.finishPortal = PortalFor(startPiece->EntryConnector(), graph.direction);
        }
    }

    return graph;
}

bool TrackSectorLayout::IsReady() const {
    return status == TrackSectorLayoutStatus::Ready && boundaries.size() == 2u &&
           !routeVariants.empty();
}

TrackSectorLayout BuildTrackSectorLayout(const Track& track) {
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);
    if (!graph.hasStartFinish) return FailedLayout(TrackSectorLayoutStatus::MissingStartFinish);
    if (!track.Validate().raceReady) return FailedLayout(TrackSectorLayoutStatus::TrackNotRaceReady);

    const TransitionMap transitionMap = BuildTransitionMap(graph);
    std::set<std::uint32_t> visitedPieceIds;
    visitedPieceIds.insert(graph.startFinishPieceId);
    std::vector<const TrackProgressTransition*> currentRoute;
    EnumerationState enumeration;
    EnumerateLapRoutes(track, transitionMap, graph.startFinishPieceId, graph.startFinishPieceId,
                       visitedPieceIds, currentRoute, 0.0f, enumeration);

    if (enumeration.tooManyRoutes) return FailedLayout(TrackSectorLayoutStatus::TooManyRouteVariants);
    if (enumeration.unsupportedCycle) return FailedLayout(TrackSectorLayoutStatus::UnsupportedCycle);
    if (enumeration.routes.empty()) return FailedLayout(TrackSectorLayoutStatus::NoClosedRoute);

    TrackSectorLayout layout;
    layout.status = TrackSectorLayoutStatus::Ready;
    layout.boundaries.resize(2u);

    for (std::size_t routeIndex = 0; routeIndex < enumeration.routes.size(); ++routeIndex) {
        const EnumeratedRoute& route = enumeration.routes[routeIndex];
        std::size_t firstBoundary = 0u;
        std::size_t secondBoundary = 0u;
        if (!SelectBoundaryIndices(track, route, firstBoundary, secondBoundary)) {
            return FailedLayout(TrackSectorLayoutStatus::TooFewTransitions);
        }

        const TrackProgressTransition& firstTransition = *route.transitions[firstBoundary];
        const TrackProgressTransition& secondTransition = *route.transitions[secondBoundary];
        AddBoundaryGate(layout.boundaries[0], firstTransition);
        AddBoundaryGate(layout.boundaries[1], secondTransition);

        TrackSectorRouteVariant variant;
        variant.id = static_cast<std::uint32_t>(routeIndex + 1u);
        variant.length = route.length;
        variant.firstBoundaryTransitionId = firstTransition.id;
        variant.secondBoundaryTransitionId = secondTransition.id;
        for (std::vector<const TrackProgressTransition*>::const_iterator transition = route.transitions.begin();
             transition != route.transitions.end(); ++transition) {
            variant.transitionIds.push_back((*transition)->id);
        }
        layout.routeVariants.push_back(variant);
    }

    std::sort(layout.boundaries[0].alternativeGates.begin(),
              layout.boundaries[0].alternativeGates.end(), GateOrder);
    std::sort(layout.boundaries[1].alternativeGates.begin(),
              layout.boundaries[1].alternativeGates.end(), GateOrder);
    return layout;
}

const char* TrackSectorLayoutStatusMessage(TrackSectorLayoutStatus status) {
    switch (status) {
    case TrackSectorLayoutStatus::Ready:
        return "Three timing sectors are available.";
    case TrackSectorLayoutStatus::MissingStartFinish:
        return "Choose a start/finish line before generating timing sectors.";
    case TrackSectorLayoutStatus::TrackNotRaceReady:
        return "Complete and validate the track before generating timing sectors.";
    case TrackSectorLayoutStatus::NoClosedRoute:
        return "No closed lap route returns to the selected start/finish line.";
    case TrackSectorLayoutStatus::TooFewTransitions:
        return "The lap route is too short to place two distinct sector boundaries.";
    case TrackSectorLayoutStatus::UnsupportedCycle:
        return "Automatic sectors do not support a repeatable sub-cycle inside one lap.";
    case TrackSectorLayoutStatus::TooManyRouteVariants:
        return "The track exposes too many alternate lap routes for automatic sectors.";
    }
    return "Timing sectors are unavailable.";
}
