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
and runs Raylib-free track, race-physics, vehicle-dynamics, ghost-replay,
time-trial, playable-package, and main-menu-flow tests.
Only building or running the graphical application requires Raylib, installed
or supplied through `RAYLIB_CFLAGS` and `RAYLIB_LIBS` as documented there.

## CMake build

```sh
cmake -S . -B build/cmake
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

CMake uses separate domain, application, and test targets. Set
`RAYLIB_INCLUDE_DIR` and `RAYLIB_LIBRARY` if Raylib is installed outside normal
system paths. To build only the headless domain and race tests, configure with
`-DNEON_RACER_BUILD_APP=OFF`; this mode does not discover or require Raylib.

## GitHub releases

Pushing one version tag beginning with `v` runs the GitHub release workflow. It
builds and tests a Linux x86_64 binary, publishes a `.tar.gz` archive and its
SHA-256 checksum, then creates or updates the matching GitHub prerelease for
alpha, beta, and release-candidate tags.

After tagging a commit already reachable from `main`, publish it with:

```sh
make publish-tag TAG=v0.2.0-alpha.17
```

The command pushes `main` and only that tag. GitHub Actions cannot see a tag
that exists only in a local clone, so this explicit push is the handoff that
starts the remote release.

## Current controls

- Main menu: use Up/Down or `W`/`S` to select, Enter/Space to choose, or
  hover and click a card. **Play a complete track** opens the verified
  playable-track library; **Create a new track** begins a fresh empty editor
  session without changing saved drafts.
- `Tab`: enter a race preview from the editor, or return a race to the screen
  that launched it (the editor or main menu)
- `Esc`: quit
- Editor: move the mouse to position the grid-snapped preview; **Shift** +
  left-click a placed component to select it for editing. Plain left-click
  places the preview even over existing road geometry when it does not collide,
  or applies an active edit; use the mouse wheel to rotate the preview.
  Hover the **Piece Library** bar at the bottom-right to raise illustrated
  cards for every component; click a card to select it without placing it.
  `1`–`6` remain component-type shortcuts; `R` rotates; `Ctrl` + mouse wheel changes
  its length/radius; `Ctrl+Z`/`Ctrl+Y`
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
  editable-draft library. `Ctrl+P` (or `F6`) opens the separate playable
  time-trial library; its frozen packages launch directly into a race with
  their saved verification ghost. The library paginates larger collections.
  Draft storage remains draft-only:
  it never contains replay data.

## Time trial controls

- `Tab`: return to the editor preview or main menu that launched the time
  trial. An editor layout must be race-ready before its preview can start.
- Keyboard: `W`/up accelerates, `S`/down applies a moderated digital brake,
  `X` reverses, `A`/`D` or left/right steers, `R` restarts all three laps, and
  `P` pauses.
- Gamepad: left stick steers; right/left trigger accelerate/brake with their
  full analogue range; face buttons provide acceleration, moderated brake, and
  reverse fallbacks.
- After a completed verified three-lap run, `E` opens the playable-package
  export form. Enter a track name, creator, and description; the export stores
  the frozen raced layout and its fastest verified ghost. Re-exporting the
  same name atomically replaces that package's current file with a higher
  version; it does not edit an existing artifact in place.

## Project structure

- `src/app`: Raylib window lifecycle, startup-menu flow/presentation,
  application-state coordination, and playable-library/export UI.
- `src/editor`: track editing interaction and editor interface.
- `src/persistence`: draft and playable-package serialization, storage paths,
  and the shared structural layout codec.
- `src/playable`: frozen playable-package contracts and export eligibility.
- `src/race`: time-trial state and vehicle simulation.
- `src/render`: track, car, and time-trial presentation.
- `src/track`: grid track model, geometry, validation, surface sampling, and
  durable layout fingerprints.
- `src/ui`: shared neon visual primitives.
- `tests/unit`: Raylib-independent domain tests.
- `assets/tracks/examples`: versioned example drafts.

See `docs/architecture.md` for dependency rules and the planned internal splits.
See `docs/development.md` for contributor workflow, `docs/track-format.md` for
the editable draft format, and `docs/playable-track-format.md` for the saved
time-trial package format.

## Data and compatibility baseline

Custom editable layouts are stored outside the repository: under
`$XDG_DATA_HOME/neon-racer/tracks` (or `~/.local/share/neon-racer/tracks`). Set
`NEON_RACER_DATA_DIR` to override that location. Shipped examples remain in
`assets/tracks/examples`.
New saves use `NEON_RACER_DRAFT 6`; the loader continues to accept
`NEON_RACER_DRAFT 1` through `NEON_RACER_DRAFT 5` files. Draft files remain
editable-only and contain no verification replay or playable-export status.

Saved playable packages live separately under
`$XDG_DATA_HOME/neon-racer/playables` with the `.nrplay` suffix. Each package
contains one frozen race-ready layout, its required metadata and export
version, and one fastest verified three-lap ghost. The package loader validates
the layout fingerprint before a race starts; malformed packages leave the
active editor/race session unchanged.

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
