#include "piece_catalog.hpp"

namespace {

const EditorPieceCatalog::Item kItems[] = {
    {TrackPieceType::Straight, "Straight", "Level road or ramp", 1},
    {TrackPieceType::Curve, "Curve", "Banked turning arc", 2},
    {TrackPieceType::Loop, "Loop", "Vertical full loop", 3},
    {TrackPieceType::Twist, "Twist", "Wide corkscrew roll", 4},
    {TrackPieceType::Branch, "Branch", "Split into two arms", 5},
    {TrackPieceType::Merge, "Merge", "Join two arms", 6},
};

const std::size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);

} // namespace

namespace EditorPieceCatalog {

const Item* Items(std::size_t& count) {
    count = kItemCount;
    return kItems;
}

const Item* Find(TrackPieceType type) {
    for (std::size_t index = 0; index < kItemCount; ++index) {
        if (kItems[index].type == type) return &kItems[index];
    }
    return 0;
}

const Item* FindShortcut(int shortcut) {
    for (std::size_t index = 0; index < kItemCount; ++index) {
        if (kItems[index].shortcut == shortcut) return &kItems[index];
    }
    return 0;
}

TrackPiece Thumbnail(TrackPieceType type) {
    const int armSpread = (type == TrackPieceType::Branch || type == TrackPieceType::Merge) ? 3 : 0;
    const int length = type == TrackPieceType::Twist ? TrackLimits::kDefaultTwistLength : 4;
    const int radius = type == TrackPieceType::Twist
        ? TrackLimits::DefaultTwistRadiusForRoadWidth(5, 5) : 4;
    return TrackPiece{0, type, GridPosition{0, 0, 0}, Heading::East, 5, 5, length,
                      CurveTurn::Right, radius, 90, 0, 0, armSpread, SurfaceMaterial::Regular};
}

} // namespace EditorPieceCatalog
