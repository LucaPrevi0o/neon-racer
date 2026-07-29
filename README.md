# Neon Racer

Neon Racer is a Raylib/C++ 3D track-building and time-trial game. It is a
self-contained project: application flow, editor, race simulation, track domain,
rendering, persistence, tests, assets, and documentation each have dedicated
locations.

## Build and run

From the workspace root:

```sh
make build
make run
make test
```

The root Makefile explicitly lists the application sources and links
the UI module. It produces `build/neon-racer`; `make test` builds
and runs the Raylib-independent track-domain tests. Raylib must be installed or
supplied through `RAYLIB_CFLAGS` and `RAYLIB_LIBS`, as documented in that Makefile.

## CMake build

```sh
cmake -S . -B build/cmake
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

CMake uses separate domain, application, and test targets. Set
`RAYLIB_INCLUDE_DIR` and `RAYLIB_LIBRARY` if Raylib is installed outside normal
system paths.

## Current controls

- `Tab`: switch between the editor and race-preview application states
- `Esc`: quit
- Editor: move the mouse to position the grid-snapped preview; left-click a
  placed component to select it, left-click empty space to place/apply it, and
  use the mouse wheel to rotate the preview. `1`/`2`/`3` choose straight/curve/loop;
  `R` rotates; `Ctrl` + mouse wheel changes its length/radius; `Ctrl+Z`/`Ctrl+Y`
  undo/redo. The **Piece Properties** panel
  uses Up/Down to select a property
  and Left/Right to adjust it; it contains all size, width, height, ramp,
  offset, material, curve-turn, extent, and bank settings.
  Apply the edited preview with left-click. Cyan connector arrows
  are entries and pink arrows are exits: a join is valid only when they meet on
  the same grid cell, point in the same travel direction, and have equal width.
  Right-drag turns the editor view, `W`/`A`/`S`/`D` move it in camera-relative
  directions, `Q`/`E` move it vertically, `Shift` + wheel zooms, and `Home`
  restores the default view.
  Click `HIDE` in the left editor panel to collapse its instructions; click
  `SHOW EDITOR HELP` to restore them.
  After the closed loop is complete, select a straight and click `SET START /
  FINISH`. New editor sessions start with an empty layout; `CLEAR TRACK` resets
  only the current in-memory layout and `Ctrl+Z` restores it. Saved drafts are
  never deleted by that action.
  `Ctrl+S` opens the name-and-save menu; `Ctrl+O` (or `F5`) opens the saved
  custom-track library. This is draft-only storage: no playable export or
  verification replay is created yet.

## Time trial controls

- `Tab`: enter or leave the time trial (the editor layout must be race-ready).
- Keyboard: `W`/up accelerates, `S`/down brakes, `X` reverses, `A`/`D` or left/
  right steers, `R` restarts all three laps, and `P` pauses.
- Gamepad: left stick steers; right/left trigger accelerate/brake; face buttons
  provide acceleration, brake, and reverse fallbacks.

## Project structure

- `src/app`: Raylib window lifecycle, application states, and cameras.
- `src/editor`: track editing interaction and editor interface.
- `src/persistence`: draft-format serialization and draft library access.
- `src/race`: time-trial state and vehicle simulation.
- `src/render`: track surface rendering.
- `src/track`: grid track model, geometry, validation, surface sampling, and
  playable-export contracts.
- `src/ui`: shared neon visual primitives.
- `tests/unit`: Raylib-independent domain tests.
- `assets/tracks/examples`: versioned example drafts.

See `docs/architecture.md` for dependency rules and the planned internal splits.
See `docs/development.md` for contributor workflow and `docs/track-format.md`
for the editable draft format.

## Data and compatibility baseline

Custom editable layouts are stored outside the repository: under
`$XDG_DATA_HOME/neon-racer/tracks` (or `~/.local/share/neon-racer/tracks`). Set
`NEON_RACER_DATA_DIR` to override that location. Shipped examples remain in
`assets/tracks/examples`.
New saves use `NEON_RACER_DRAFT 5`; the loader continues to accept milestone-one
`NEON_RACER_DRAFT 1` through `NEON_RACER_DRAFT 4` files. Draft files remain editable-only at this stage and
contain no verification replay or playable-export status.

The track domain exposes Raylib-independent surface/contact and export-metadata
contracts. They are scaffolding for later editor, physics, replay, and
playable-export phases; current racing behavior is unchanged.

## Track-domain foundation

`track.*` supports grid-aligned straights and right/left 90°, 180°, and 270°
curves (3–20 cells), each with entry/exit connectors. Press `V` while a curve
preview is active to cycle its extent; `B`/`N` increase/decrease its signed
midpoint bank angle, which smoothly returns to zero at both connectors.
Straights support independently
configured endpoint width, elevation, lateral offset, and surface material in
the domain and saved format; the dedicated editor inspector follows in the next
editor phase. Track sampling exposes the same continuous road data to rendering
and future vehicle physics. Validation currently matches connector position,
heading, elevation, and width; it rejects centreline intersections except
compatible joins and requires one closed loop plus a start/finish straight.
