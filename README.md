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

The main menu is the application Home screen and exposes two destinations:

- **Play a complete track** opens the library of frozen `.nrplay` packages.
- **Open track editor** starts an empty editor session. It becomes **Return to
  track editor** while an editor session remains active behind a local trial or
  another temporary screen.

`Esc` is the standard contextual action key. In the editor it opens the editor
action menu; during a race it pauses the run; from Home it quits the
application. Raylib's implicit Escape shutdown is disabled so every screen owns
its behavior explicitly.

The editor action menu provides:

- **Start Trial**, enabled only for a race-ready track with a start/finish line;
- **Save Draft**, which names a new editable draft or updates the current draft
  in the latest format;
- **Open Draft**, which opens the saved-draft library;
- **Quit to Menu**, which ends the editor session and returns Home. A track that
  has never been saved first presents an OK/Cancel discard warning.

Completing a verified local three-lap trial enables export of a frozen playable
package containing the exact raced layout, metadata, and its verification
ghost. Saved playable packages remain accessible only from Home.

The race pause and completed-results menus return to Home. Confirmation buttons
work with keyboard, gamepad, and mouse: destructive actions still require two
activations, and keeping the pointer on the same button allows the second click
to complete the confirmation.

## Controls

### Global and menus

| Action | Controls |
| --- | --- |
| Navigate the Home, editor-action, pause, results, or playable-library menu | Up/Down, `W`/`S`, or gamepad D-pad; hover with the mouse |
| Confirm a menu action | Enter, Space, gamepad A, or left-click |
| Complete a destructive confirmation | Repeat Enter/Space, gamepad A, or left-click on the same action |
| Cancel a confirmation / go back | `Esc` or gamepad B |
| Page the playable library | Left/Right, Page Up/Page Down, or gamepad D-pad Left/Right |
| Refresh the playable library | `R`, `F5`, or gamepad X |
| Quit from the Home menu | `Esc` or gamepad B |

### Track editor

| Action | Controls |
| --- | --- |
| Open editor actions | `Esc` or gamepad Start |
| Save a draft directly | `Ctrl+S` |
| Open/close the draft library directly | `Ctrl+O` |
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
| Orbit the editor camera | Right-drag |
| Pan the editor camera | `W`/`A`/`S`/`D` |
| Move the editor camera vertically | `Q` / `E` |
| Zoom | Shift + mouse wheel |
| Reset the editor camera | Home |

**Start Trial** remains disabled until track validation reports a complete,
race-ready layout. **Save Draft** reuses the same persistence path as
`Ctrl+S`: the first save asks for a name, and later saves update the identified
file. **Open Draft** reuses the `Ctrl+O` library. **Quit to Menu** deliberately
ends the current editor session; saved files are never deleted.

The playable-track library is opened from Home rather than directly from the
editor. This keeps top-level navigation in one place while local editor trials
remain available through the editor action menu for verification and export.

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
| Recover to the last confirmed checkpoint | `R` | Upper face button |
| Restart the complete three-lap run | `Shift+R` | Middle-left button |
| Open the pause menu | `Esc` | Start |

The pause menu provides **Resume**, **Return to checkpoint**, **Restart run**,
**Return to main menu**, and **Quit**. Restart, return, and quit require a second
confirmation. Returning to the checkpoint is immediate: it restores the car to
the last confirmed recovery pose, closes the pause menu, resumes the race, and
snaps the chase camera to the recovered car. Escape, Start, or B resumes from
the pause menu, while B remains available as the digital brake during active
play.

The completed-race results menu provides **Race again**, **Save playable track**
or **Update saved ghost** when available, **Return to main menu**, and **Quit**.
It shows the final three-lap total and best lap. Race again resets the attempt
and snaps the camera to the start; return and quit require confirmation.

Recovery returns the car to a safe pose just inside the last track piece reached
through a valid checkpoint portal. It preserves the current lap, all recorded
times, and the confirmed graph state. Wrong-way and unrelated portal crossings
cannot move the recovery checkpoint. A complete restart resets all three laps.
Ghosts are visual, non-colliding replays and do not affect track or vehicle
physics.

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
| `src/app` | Window lifecycle, Home flow, application state, contextual menus, libraries, and export UI |
| `src/editor` | Mutable editor interaction and presentation |
| `src/persistence` | Draft/package serialization, storage paths, and the shared layout codec |
| `src/playable` | Frozen package contracts and export policy |
| `src/race` | Vehicle dynamics, ghost replay, checkpoint recovery, and time-trial orchestration |
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
