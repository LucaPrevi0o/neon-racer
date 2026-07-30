#include "neon_racer/editor/editor.hpp"

#include "neon_racer/editor/piece_catalog.hpp"

#include <algorithm>
#include <cmath>

namespace {

int ClampGridValue(long long value) {
    const long long maximum = static_cast<long long>(TrackLimits::kMaximumGridCoordinate);
    if (value < -maximum) return -TrackLimits::kMaximumGridCoordinate;
    if (value > maximum) return TrackLimits::kMaximumGridCoordinate;
    return static_cast<int>(value);
}

int OffsetGridValue(int value, int offset) {
    return ClampGridValue(static_cast<long long>(value) + static_cast<long long>(offset));
}

int OffsetElevationDelta(int value, int offset) {
    const long long maximum = static_cast<long long>(TrackLimits::kMaximumElevationDelta);
    const long long shifted = static_cast<long long>(value) + static_cast<long long>(offset);
    if (shifted < -maximum) return -TrackLimits::kMaximumElevationDelta;
    if (shifted > maximum) return TrackLimits::kMaximumElevationDelta;
    return static_cast<int>(shifted);
}

const char* PieceName(TrackPieceType type) {
    const EditorPieceCatalog::Item* item = EditorPieceCatalog::Find(type);
    return item != 0 ? item->name : "Unknown";
}

const char* MaterialName(SurfaceMaterial material) {
    switch (material) {
    case SurfaceMaterial::Regular: return "regular";
    case SurfaceMaterial::Slippery: return "slippery";
    case SurfaceMaterial::HighResistance: return "high-resistance";
    }
    return "unknown";
}

SurfaceMaterial NextMaterial(SurfaceMaterial material) {
    return static_cast<SurfaceMaterial>((static_cast<int>(material) + 1) % 3);
}

SurfaceMaterial PreviousMaterial(SurfaceMaterial material) {
    return static_cast<SurfaceMaterial>((static_cast<int>(material) + 2) % 3);
}

int ClampTwistLength(int length, int width, int exitWidth) {
    const int minimum = TrackLimits::MinimumTwistLengthForRoadWidth(width, exitWidth);
    return std::max(minimum, std::min(TrackLimits::kMaximumTwistLength, length));
}

int ClampTwistRadius(int radius, int width, int exitWidth) {
    const int minimum = TrackLimits::MinimumTwistRadiusForRoadWidth(width, exitWidth);
    const int maximum = static_cast<int>(TrackLimits::kMaximumTwistRadius);
    return std::max(minimum, std::min(maximum, radius));
}

int EffectiveTwistRadiusForEditor(const TrackPiece& piece) {
    const int resolved = static_cast<int>(std::round(TrackLimits::ResolveTwistRadius(piece.length, piece.curveRadius)));
    return ClampTwistRadius(resolved, piece.width, piece.exitWidth);
}

} // namespace

void TrackEditor::SelectPreviewType(TrackPieceType type) {
    const EditorPieceCatalog::Item* item = EditorPieceCatalog::Find(type);
    if (item == 0) return;
    const bool editingSelectedPiece = selectedPieceId_ != 0;
    const TrackPieceType previousType = preview_.type;
    preview_.type = type;
    if (type == TrackPieceType::Twist) {
        const bool convertingLegacyFlatTwist = previousType == TrackPieceType::Twist &&
            TrackLimits::IsLegacyFlatTwistRadius(preview_.curveRadius);
        const int preferredLength = previousType == TrackPieceType::Twist && !convertingLegacyFlatTwist
            ? preview_.length : TrackLimits::kDefaultTwistLength;
        preview_.length = ClampTwistLength(preferredLength, preview_.width, preview_.exitWidth);
        if (previousType != TrackPieceType::Twist || convertingLegacyFlatTwist) {
            preview_.curveRadius = TrackLimits::DefaultTwistRadiusForRoadWidth(preview_.width, preview_.exitWidth);
        } else if (preview_.curveRadius != 0) {
            preview_.curveRadius = ClampTwistRadius(preview_.curveRadius, preview_.width, preview_.exitWidth);
        }
    } else if (previousType == TrackPieceType::Twist) {
        preview_.length = std::max(3, std::min(20, preview_.length));
    }
    if (type == TrackPieceType::Branch || type == TrackPieceType::Merge) {
        preview_.lateralOffset = std::max(1, std::abs(preview_.lateralOffset));
    }
    selectedPropertyIndex_ = 0;

    if (editingSelectedPiece) {
        SetMessage(std::string(item->name) + " selected for the active edit. Click or press M to apply.");
        return;
    }

    if (type == TrackPieceType::Straight) SetMessage("Straight selected for placement.");
    else if (type == TrackPieceType::Curve) SetMessage("Curve selected for placement. Edit its properties in the panel.");
    else if (type == TrackPieceType::Loop) SetMessage("Vertical loop selected for placement.");
    else if (type == TrackPieceType::Twist) {
        SetMessage(TextFormat("Twist selected: %i-cell run / %.1f-cell radius. Tune both in the panel.", preview_.length,
                              TrackLimits::ResolveTwistRadius(preview_.length, preview_.curveRadius)));
    } else if (type == TrackPieceType::Branch) SetMessage("Two-arm branch selected for placement.");
    else SetMessage("Two-arm merge selected for placement.");
}

void TrackEditor::MovePreview(int x, int z) {
    preview_.entryPosition.x = OffsetGridValue(preview_.entryPosition.x, x);
    preview_.entryPosition.z = OffsetGridValue(preview_.entryPosition.z, z);
}

void TrackEditor::RotatePreview() {
    preview_.entryHeading = static_cast<Heading>((static_cast<int>(preview_.entryHeading) + 1) % 4);
    SetMessage("Preview rotated 90 degrees.");
}

void TrackEditor::ChangeDimension(int amount) {
    if (preview_.type == TrackPieceType::Twist) {
        if (TrackLimits::IsLegacyFlatTwistRadius(preview_.curveRadius)) {
            preview_.length = ClampTwistLength(preview_.length, preview_.width, preview_.exitWidth);
            preview_.curveRadius = TrackLimits::DefaultTwistRadiusForRoadWidth(preview_.width, preview_.exitWidth);
            SetMessage("Legacy flat Twist converted to an adjustable corkscrew.");
        }
        preview_.length = ClampTwistLength(preview_.length + amount, preview_.width, preview_.exitWidth);
        SetMessage(TextFormat("Twist length set to %i.", preview_.length));
        return;
    }
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) {
        preview_.length = std::max(3, std::min(20, preview_.length + amount));
        SetMessage(TextFormat("%s length set to %i.", PieceName(preview_.type), preview_.length));
        return;
    }
    int& dimension = preview_.type == TrackPieceType::Straight ? preview_.length : preview_.curveRadius;
    const int maximum = preview_.type == TrackPieceType::Loop ? 10 : 20;
    dimension = std::max(3, std::min(maximum, dimension + amount));
}

int TrackEditor::PropertyCount() const {
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) return 4;
    if (preview_.type == TrackPieceType::Straight) return 7;
    if (preview_.type == TrackPieceType::Twist) return 8;
    return preview_.type == TrackPieceType::Curve ? 9 : 7;
}

const char* TrackEditor::PropertyName(int index) const {
    static const char* straight[] = {"Length", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Offset", "Surface"};
    static const char* twist[] = {"Run length", "Loop radius", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Offset", "Surface"};
    static const char* curve[] = {"Radius", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Surface", "Turn", "Extent", "Bank"};
    static const char* loop[] = {"Radius", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Offset", "Surface"};
    static const char* branch[] = {"Length", "Width", "Arm spread", "Surface"};
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) return branch[index];
    if (preview_.type == TrackPieceType::Twist) return twist[index];
    if (preview_.type == TrackPieceType::Straight) return straight[index];
    return preview_.type == TrackPieceType::Curve ? curve[index] : loop[index];
}

std::string TrackEditor::PropertyValue(int index) const {
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) {
        if (index == 0) return std::to_string(preview_.length);
        if (index == 1) return std::to_string(preview_.width);
        return index == 2 ? std::to_string(std::abs(preview_.lateralOffset)) : MaterialName(preview_.material);
    }
    if (preview_.type == TrackPieceType::Twist) {
        if (index == 0) return std::to_string(preview_.length);
        if (index == 1) {
            if (TrackLimits::IsLegacyFlatTwistRadius(preview_.curveRadius)) return "legacy flat";
            return preview_.curveRadius == 0 ? "auto" : std::to_string(preview_.curveRadius);
        }
        if (index == 2) return std::to_string(preview_.width);
        if (index == 3) return std::to_string(preview_.exitWidth);
        if (index == 4) return std::to_string(preview_.entryPosition.y);
        if (index == 5) return std::to_string(preview_.elevationDelta);
        return index == 6 ? std::to_string(preview_.lateralOffset) : MaterialName(preview_.material);
    }
    if (index == 0) return std::to_string(preview_.type == TrackPieceType::Straight ? preview_.length : preview_.curveRadius);
    if (index == 1) return std::to_string(preview_.width);
    if (index == 2) return std::to_string(preview_.exitWidth);
    if (index == 3) return std::to_string(preview_.entryPosition.y);
    if (index == 4) return std::to_string(preview_.elevationDelta);
    if (preview_.type == TrackPieceType::Straight) {
        return index == 5 ? std::to_string(preview_.lateralOffset) : MaterialName(preview_.material);
    }
    if (preview_.type == TrackPieceType::Loop) {
        return index == 5 ? std::to_string(preview_.lateralOffset) : MaterialName(preview_.material);
    }
    if (index == 5) return MaterialName(preview_.material);
    if (index == 6) return preview_.curveTurn == CurveTurn::Right ? "right" : "left";
    if (index == 7) return std::to_string(preview_.curveDegrees) + " deg";
    return std::to_string(preview_.bankAngleDegrees) + " deg";
}

float TrackEditor::PropertyFraction(int index) const {
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) {
        if (index == 0) return static_cast<float>(preview_.length - 3) / 17.0f;
        if (index == 1) return static_cast<float>(preview_.width - 5) / 6.0f;
        return index == 2 ? static_cast<float>(std::abs(preview_.lateralOffset) - 1) / 9.0f :
            (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
    }
    if (index == 0 && preview_.type == TrackPieceType::Twist) {
        const int minimum = TrackLimits::MinimumTwistLengthForRoadWidth(preview_.width, preview_.exitWidth);
        return static_cast<float>(preview_.length - minimum) /
            static_cast<float>(TrackLimits::kMaximumTwistLength - minimum);
    }
    if (preview_.type == TrackPieceType::Twist) {
        const int minimum = TrackLimits::MinimumTwistRadiusForRoadWidth(preview_.width, preview_.exitWidth);
        const int effectiveRadius = EffectiveTwistRadiusForEditor(preview_);
        if (index == 1) return static_cast<float>(effectiveRadius - minimum) /
            static_cast<float>(static_cast<int>(TrackLimits::kMaximumTwistRadius) - minimum);
        if (index == 2) return static_cast<float>(preview_.width - 5) / 6.0f;
        if (index == 3) return static_cast<float>(preview_.exitWidth - 5) / 6.0f;
        if (index == 4 || index == 5) return 0.5f;
        return index == 6 ? (preview_.lateralOffset + 3.0f) / 6.0f :
            (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
    }
    if (index == 0) return static_cast<float>((preview_.type == TrackPieceType::Straight ? preview_.length : preview_.curveRadius) - 3) / 17.0f;
    if (index == 1) return static_cast<float>(preview_.width - 5) / 6.0f;
    if (index == 2) return static_cast<float>(preview_.exitWidth - 5) / 6.0f;
    if (index == 3 || index == 4) return 0.5f;
    if (preview_.type == TrackPieceType::Straight) {
        return index == 5 ? (preview_.lateralOffset + 3.0f) / 6.0f :
            (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
    }
    if (preview_.type == TrackPieceType::Loop) {
        return index == 5 ? (preview_.lateralOffset + 10.0f) / 20.0f :
            (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
    }
    if (index == 5) return (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
    if (index == 6) return preview_.curveTurn == CurveTurn::Right ? 1.0f : 0.0f;
    if (index == 7) return static_cast<float>(preview_.curveDegrees - 90) / 180.0f;
    return static_cast<float>(preview_.bankAngleDegrees + 45) / 90.0f;
}

void TrackEditor::CycleProperty(int direction) {
    const int count = PropertyCount();
    selectedPropertyIndex_ = (selectedPropertyIndex_ + direction + count) % count;
}

void TrackEditor::AdjustSelectedProperty(int direction) {
    const int index = selectedPropertyIndex_;
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) {
        if (index == 0) ChangeDimension(direction);
        if (index == 1) preview_.width = preview_.exitWidth = std::max(5, std::min(11, preview_.width + direction));
        if (index == 2) preview_.lateralOffset = std::max(1, std::min(10, std::abs(preview_.lateralOffset) + direction));
        if (index == 3) preview_.material = direction > 0 ? NextMaterial(preview_.material) : PreviousMaterial(preview_.material);
        return;
    }
    if (preview_.type == TrackPieceType::Twist) {
        if (TrackLimits::IsLegacyFlatTwistRadius(preview_.curveRadius)) {
            preview_.length = ClampTwistLength(preview_.length, preview_.width, preview_.exitWidth);
            preview_.curveRadius = TrackLimits::DefaultTwistRadiusForRoadWidth(preview_.width, preview_.exitWidth);
            SetMessage("Legacy flat Twist converted to an adjustable corkscrew.");
        }
        if (index == 0) { ChangeDimension(direction); return; }
        if (index == 1) {
            preview_.curveRadius = ClampTwistRadius(EffectiveTwistRadiusForEditor(preview_) + direction,
                                                     preview_.width, preview_.exitWidth);
            SetMessage(TextFormat("Twist radius set to %i.", preview_.curveRadius));
            return;
        }
        if (index == 2) {
            preview_.width = std::max(5, std::min(11, preview_.width + direction));
            preview_.length = ClampTwistLength(preview_.length, preview_.width, preview_.exitWidth);
            preview_.curveRadius = ClampTwistRadius(EffectiveTwistRadiusForEditor(preview_), preview_.width,
                                                     preview_.exitWidth);
            return;
        }
        if (index == 3) {
            preview_.exitWidth = std::max(5, std::min(11, preview_.exitWidth + direction));
            preview_.length = ClampTwistLength(preview_.length, preview_.width, preview_.exitWidth);
            preview_.curveRadius = ClampTwistRadius(EffectiveTwistRadiusForEditor(preview_), preview_.width,
                                                     preview_.exitWidth);
            return;
        }
        if (index == 4) { preview_.entryPosition.y = OffsetGridValue(preview_.entryPosition.y, direction); return; }
        if (index == 5) { preview_.elevationDelta = OffsetElevationDelta(preview_.elevationDelta, direction); return; }
        if (index == 6) {
            preview_.lateralOffset = std::max(-3, std::min(3, preview_.lateralOffset + direction));
            return;
        }
        preview_.material = direction > 0 ? NextMaterial(preview_.material) : PreviousMaterial(preview_.material);
        return;
    }
    if (index == 0) { ChangeDimension(direction); return; }
    if (index == 1) { preview_.width = std::max(5, std::min(11, preview_.width + direction)); return; }
    if (index == 2) { preview_.exitWidth = std::max(5, std::min(11, preview_.exitWidth + direction)); return; }
    if (index == 3) { preview_.entryPosition.y = OffsetGridValue(preview_.entryPosition.y, direction); return; }
    if (index == 4) { preview_.elevationDelta = OffsetElevationDelta(preview_.elevationDelta, direction); return; }
    if (preview_.type == TrackPieceType::Straight) {
        if (index == 5) {
            const int maximum = preview_.length < 4 ? 2 : 3;
            preview_.lateralOffset = std::max(-maximum, std::min(maximum, preview_.lateralOffset + direction));
        } else {
            preview_.material = direction > 0 ? NextMaterial(preview_.material) : PreviousMaterial(preview_.material);
        }
        return;
    }
    if (preview_.type == TrackPieceType::Loop) {
        if (index == 5) preview_.lateralOffset = std::max(-10, std::min(10, preview_.lateralOffset + direction));
        if (index == 6) preview_.material = direction > 0 ? NextMaterial(preview_.material) : PreviousMaterial(preview_.material);
        return;
    }
    if (index == 5) preview_.material = direction > 0 ? NextMaterial(preview_.material) : PreviousMaterial(preview_.material);
    if (index == 6) preview_.curveTurn = preview_.curveTurn == CurveTurn::Right ? CurveTurn::Left : CurveTurn::Right;
    if (index == 7) preview_.curveDegrees = direction > 0
        ? (preview_.curveDegrees == 270 ? 90 : preview_.curveDegrees + 90)
        : (preview_.curveDegrees == 90 ? 270 : preview_.curveDegrees - 90);
    if (index == 8) preview_.bankAngleDegrees = std::max(-45, std::min(45, preview_.bankAngleDegrees + direction * 5));
}
