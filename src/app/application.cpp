#include "application.hpp"
#include "app_settings.hpp"

#include "../render/car_renderer.hpp"
#include "../render/race_scene.hpp"
#include "../render/track_renderer.hpp"
#include "../ui/neon.hpp"

#include <cmath>
#include <sstream>

namespace {

/*void DrawGridFloor(int slices, float spacing, Color color) {

    const float extent = static_cast<float>(slices) * spacing * 0.5f;
    for (int i = 0; i <= slices; ++i) {

        const float offset = -extent + static_cast<float>(i) * spacing;
        DrawLine3D(Vector3{offset, 0.0f, -extent}, Vector3{offset, 0.0f, extent}, color);
        DrawLine3D(Vector3{-extent, 0.0f, offset}, Vector3{extent, 0.0f, offset}, color);
    }
}

Vector3 LerpVector3(Vector3 from, Vector3 to, float amount) {

    return Vector3{from.x + (to.x - from.x) * amount,
                   from.y + (to.y - from.y) * amount,
                   from.z + (to.z - from.z) * amount};
}

void DrawRaceTrack(const Track& track) {

    for (std::vector<TrackPiece>::const_iterator piece = track.Pieces().begin();
    piece != track.Pieces().end(); ++piece)
        DrawTrackPieceSurface(*piece, Fade(Neon::Panel, 0.96f), Neon::Cyan);

    if (track.HasStartFinish()) {

        const TrackPiece* startPiece = track.GetPiece(track.StartFinishPieceId());
        if (startPiece != 0) {
            
            const GridPosition position = startPiece->EntryConnector().position;
            DrawCube(Vector3{static_cast<float>(position.x), static_cast<float>(position.y) + 0.18f,
                             static_cast<float>(position.z)},
                     0.95f, 0.10f, 0.95f, Neon::Green);
        }
    }
}*/

} // namespace

namespace {
Vector3 LerpVector3(Vector3 from, Vector3 to, float amount) {
    return Vector3{from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount,
                   from.z + (to.z - from.z) * amount};
}
}

RacerApplication::RacerApplication()
    : state_(AppState::Editor),
      editorCamera_{},
      raceCamera_{},
      editor_() {
    editorCamera_.position = Vector3{18.0f, 18.0f, 18.0f};
    editorCamera_.target = Vector3{0.0f, 0.0f, 0.0f};
    editorCamera_.up = Vector3{0.0f, 1.0f, 0.0f};
    editorCamera_.fovy = 45.0f;
    editorCamera_.projection = CAMERA_PERSPECTIVE;

    raceCamera_.position = Vector3{0.0f, 5.0f, 10.0f};
    raceCamera_.target = Vector3{0.0f, 0.0f, 0.0f};
    raceCamera_.up = Vector3{0.0f, 1.0f, 0.0f};
    raceCamera_.fovy = 55.0f;
    raceCamera_.projection = CAMERA_PERSPECTIVE;
}

void RacerApplication::Update(float frameTime) {
    if (IsKeyPressed(KEY_TAB)) {
        const AppState nextState = state_ == AppState::Editor ? AppState::Race : AppState::Editor;
        if (nextState == AppState::Race) {
            std::string error;
            if (PlayableExport::Build(editor_.GetTrack(), "Editor session", playableTrack_, error)) {
                race_.Start(playableTrack_.layout);
            } else {
                // Start preserves its existing, clear validation feedback when an
                // export cannot be made yet.
                race_.Start(editor_.GetTrack());
            }
        }
        state_ = nextState;
    }

    if (state_ == AppState::Editor) {
        UpdateEditor();
    } else {
        UpdateRace(frameTime);
    }
}

void RacerApplication::Draw() const {
    Neon::DrawBackground(AppSettings::kWindowWidth, AppSettings::kWindowHeight);

    if (state_ == AppState::Editor) {
        DrawEditor();
    } else {
        DrawRace();
    }

    DrawStateHint();
}

void RacerApplication::UpdateEditor() {
    editor_.Update(editorCamera_);
}

void RacerApplication::UpdateRace(float frameTime) {
    race_.Update(frameTime);
    if (!race_.IsReady()) return;

    const RaceCar& car = race_.Car();
    const Vector3 behind = Vector3{-std::cos(car.headingRadians) * 8.0f, 4.5f,
                                   -std::sin(car.headingRadians) * 8.0f};
    const Vector3 desiredPosition = Vector3{car.position.x + behind.x, car.position.y + behind.y,
                                            car.position.z + behind.z};
    const Vector3 desiredTarget = Vector3{car.position.x, car.position.y + 0.2f, car.position.z};
    raceCamera_.position = LerpVector3(raceCamera_.position, desiredPosition, 0.10f);
    raceCamera_.target = LerpVector3(raceCamera_.target, desiredTarget, 0.14f);
}

void RacerApplication::DrawEditor() const {
    BeginMode3D(editorCamera_);
    DrawGridFloor(24, 1.0f, Fade(Neon::Cyan, 0.20f));
    editor_.DrawTrack3D();
    EndMode3D();

    Neon::DrawHeader("NEON RACER  //  TRACK EDITOR", AppSettings::kWindowWidth);
    editor_.DrawInterface();
}

void RacerApplication::DrawRace() const {
    BeginMode3D(raceCamera_);
    DrawGridFloor(40, 1.0f, Fade(Neon::Cyan, 0.15f));
    DrawRaceTrackScene(race_.IsReady() ? playableTrack_.layout : editor_.GetTrack());
    if (race_.IsReady() && race_.HasVerifiedGhost()) DrawGhostRaceCar(race_.GhostCar());
    if (race_.IsReady()) DrawRaceCar(race_.Car());
    EndMode3D();

    Neon::DrawHeader("NEON RACER  //  TIME TRIAL", AppSettings::kWindowWidth);
    Neon::DrawOverlayPanel(Rectangle{28.0f, 80.0f, 300.0f, 198.0f});
    if (!race_.IsReady()) {
        DrawText("TRACK NOT READY", 46, 100, 20, Neon::Orange);
        DrawText(race_.StatusMessage(), 46, 134, 15, Fade(RAYWHITE, 0.80f));
        DrawText("Press Tab to return to the editor.", 46, 166, 15, Neon::Yellow);
        return;
    }
    DrawText(TextFormat("LAP %i / 3", race_.CurrentLap()), 46, 100, 21, Neon::Cyan);
    DrawText(TextFormat("CURRENT  %s", FormatRaceTime(race_.CurrentLapTime())), 46, 132, 18, RAYWHITE);
    DrawText(TextFormat("BEST     %s", race_.BestLapTime() > 0.0f ? FormatRaceTime(race_.BestLapTime()) : "--:--.--"),
             46, 158, 18, Neon::Green);
    DrawText(TextFormat("TOTAL    %s", FormatRaceTime(race_.TotalTime())), 46, 184, 18, RAYWHITE);
    DrawText(TextFormat("SPEED    %03i km/h", static_cast<int>(std::fabs(race_.Car().speed) * 8.0f)),
             46, 210, 18, Neon::Pink);
    const char* surface = race_.CurrentSurfaceMaterial() == SurfaceMaterial::Slippery ? "SLIPPERY" :
                          race_.CurrentSurfaceMaterial() == SurfaceMaterial::HighResistance ? "HIGH RESISTANCE" :
                          "REGULAR";
    DrawText(TextFormat("SURFACE  %s", race_.IsOnTrack() ? surface : "OFF TRACK"), 46, 244, 14,
             race_.IsOnTrack() ? Neon::Cyan : Neon::Orange);
    DrawText(race_.StatusMessage(), 46, 264, 14, race_.IsPaused() ? Neon::Yellow : Fade(RAYWHITE, 0.78f));
    if (race_.HasVerifiedGhost()) DrawText("VERIFIED GHOST ACTIVE", 46, 282, 13, Neon::Green);
    Neon::DrawOverlayPanel(Rectangle{28.0f, 296.0f, 420.0f, 34.0f}, 0.72f);
    DrawText("WASD / arrows drive  X reverse  R reset  P pause", 42, 305, 16, Neon::Yellow);
}

void RacerApplication::DrawStateHint() const {
    Neon::DrawOverlayPanel(Rectangle{28.0f, static_cast<float>(AppSettings::kWindowHeight - 58), 460.0f, 34.0f}, 0.72f);
    DrawText("TAB: switch editor / race preview    ESC: quit", 42, AppSettings::kWindowHeight - 49, 17, Neon::Yellow);
}
