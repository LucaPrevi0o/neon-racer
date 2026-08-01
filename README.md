# Neon Racer

Neon Racer is a Raylib/C++ 3D track-building and three-lap time-trial game. It
contains a custom-track editor, a surface-query-driven vehicle simulation,
verified ghost replays, and separate formats for editable drafts and frozen
playable packages.

## Quick start

From the repository root:

```sh
make build
make run
make test
```

The graphical application requires Raylib. Headless domain and race tests do
not:

```sh
cmake -S . -B build/cmake -DNEON_RACER_BUILD_APP=OFF
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

For the normal CMake build:

```sh
cmake -S . -B build/cmake
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

Set `RAYLIB_CFLAGS` and `RAYLIB_LIBS` for the Makefile, or
`RAYLIB_INCLUDE_DIR` and `RAYLIB_LIBRARY` for CMake, when Raylib is installed
outside the normal system paths.

## Game flow

The main menu exposes two entry points:

- **Play a complete track** opens the library of frozen `.nrplay` packages.
- **Create a new track** starts an empty editor session without modifying saved
  drafts.

The editor can save incomplete work as an editable `.draft`. A race-ready
layout can be opened as an in-memory time-trial preview with `Tab`. Completing
a verified three-lap run enables export of a frozen playable package containing
the exact raced layout, metadata, and its verification ghost.

`Tab` returns a race to the screen that launched it: the editor or the main
menu.

## Controls

### Global and menus

| Action | Controls |
| --- | --- |
| Navigate a menu | Up/Down or `W`/`S`; hover with the mouse |
| Confirm | Enter, Space, or left-click |
| Return between editor/menu and race | `Tab` |
| Quit | `Esc` |

### Track editor

| Action | Controls |
| --- | --- |
| Position the placement preview | Move the mouse over the grid |
| Place or apply the preview | Left-click |
| Select a placed component | Shift + left-click |
| Rotate the preview | Mouse wheel or `R` |
| Choose a component | `1`–`6`, or click a Piece Library card |
| Toggle the checkpoint graph overlay | `G` |
| Change the primary dimension | Ctrl + mouse wheel |
| Cycle curve extent | `V` |
| Increase/decrease curve bank | `B` / `N` |
| Select an inspector property | Up/Down |
| Adjust an inspector property | Left/Right |
| Undo/redo | `Ctrl+Z` / `Ctrl+Y` |
| Save a draft | `Ctrl+S` |
| Open the draft library | `Ctrl+O` or `F5` |
| Open the playable library | `Ctrl+P` or `F6` |
| Orbit the editor camera | Right-drag |
| Pan the editor camera | `W`/`A`/`S`/`D` |
| Move the editor camera vertically | `Q` / `E` |
| Zoom | Shift + mouse wheel |
| Reset the editor camera | Home |

The bottom-right **Piece Library** expands on hover. Clicking one of its cards
changes the selected component without placing it. Pointer actions over the
palette do not affect the world, and the palette remains inactive behind modal
libraries.

The **Piece Properties** panel exposes the parameters supported by the active
component, including dimensions, endpoint widths, height, ramp delta, lateral
offset, material, curve turn and extent, bank angle, and Twist run/radius.
Cyan arrows mark entries and pink arrows mark exits. A connector join requires
the same grid position, travel heading, elevation, and width.

Press `G` to inspect the progress graph used by lap tracking. Cyan spheres are
ordinary checkpoint states, yellow spheres are branch/merge states, and orange
spheres are pieces that are not part of a current connection. Pink arrows show
the selected race direction. Yellow rectangular portals are connector planes
the car must cross to confirm a transition; the green portal is the finish
plane. The overlay is hidden by default and is not persisted in drafts.

After completing a valid closed race graph, select an eligible straight and use
**SET START / FINISH**. **CLEAR TRACK** resets only the in-memory editor
session; it is undoable and never deletes saved drafts.

### Time trial

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Accelerate | `W` or Up | Right trigger or acceleration face button |
| Brake | `S` or Down | Left trigger or brake face button |
| Reverse | `X` | Reverse face button |
| Steer | `A`/`D` or Left/Right | Left stick |
| Restart the complete three-lap run | `R` | — |
| Pause | `P` | — |
| Return to the launching screen | `Tab` | — |
| Open playable export after verification | `E` | — |

A reset restarts all three laps. Ghosts are visual, non-colliding replays and do
not affect track or vehicle physics.

## Storage and compatibility

User-authored content is stored outside the repository:

| Content | Default location | Format |
| --- | --- | --- |
| Editable drafts | `$XDG_DATA_HOME/neon-racer/tracks` | `.draft` |
| Frozen playable packages | `$XDG_DATA_HOME/neon-racer/playables` | `.nrplay` |

When `XDG_DATA_HOME` is unset, both paths fall back to
`~/.local/share/neon-racer`. Set `NEON_RACER_DATA_DIR` to override the base
directory. Versioned examples remain under `assets/tracks/examples`.

New drafts use layout version 6. The loader accepts versions 1 through 6 and
preserves the legacy flat rolling-Twist geometry of versions 1 through 5.
Playable packages currently use package version 1 and may embed supported
layout versions 1 through 6.

See the format specifications for exact records, validation limits, migration
rules, and atomic-write behavior.

## Repository map

| Path | Responsibility |
| --- | --- |
| `src/app` | Window lifecycle, startup flow, application state, libraries, and export UI |
| `src/editor` | Mutable editor interaction and presentation |
| `src/persistence` | Draft/package serialization, storage paths, and the shared layout codec |
| `src/playable` | Frozen package contracts and export policy |
| `src/race` | Vehicle dynamics, ghost replay, and time-trial orchestration |
| `src/render` | Track, vehicle, and race presentation |
| `src/track` | Track model, geometry, validation, progress graphs, surface queries, and fingerprints |
| `src/ui` | Shared neon presentation primitives |
| `tests/unit` | Raylib-free behavior tests |
| `assets/tracks/examples` | Versioned example drafts |

## Documentation

Start with [`docs/index.md`](docs/index.md), which identifies the canonical
document for each topic.

- [`docs/architecture.md`](docs/architecture.md): dependency boundaries,
  ownership, and runtime flows.
- [`docs/development.md`](docs/development.md): contributor workflow, test
  expectations, smoke checks, and release publishing.
- [`docs/design-specification.md`](docs/design-specification.md): product
  direction and design invariants.
- [`docs/track-format.md`](docs/track-format.md): editable draft format.
- [`docs/playable-track-format.md`](docs/playable-track-format.md): frozen
  playable-package format.
