# Development guide

## Build and test

Use either supported build entrypoint from the repository root:

```sh
make build
make test
```

or:

```sh
cmake -S . -B build/cmake
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

Track, race-physics, vehicle-dynamics, ghost-replay, time-trial, and
playable-package tests must not include or link Raylib.
Add behavior tests under `tests/unit` whenever changing track geometry,
validation, serialization, playable-package rules, or isolated race-dynamics
math.

## Dependency rules

`src/track`, `src/persistence`, `src/playable`, and `src/race` are Raylib-free
core modules.
`src/editor`, `src/render`, and `src/ui` may use Raylib, but rendering and
input adapters must not define track or race rules.
`src/app` is the composition root.

Before adding a new source file, add it explicitly to both the Makefile and the
appropriate CMake target. This is intentional: a missing module should fail a
review visibly rather than being silently discovered by a wildcard.

Editor camera and picking code is Raylib-facing presentation code. Keep device
polling and interaction ordering in `TrackEditor::Update`; pass `Camera3D` or
`Ray` values into extracted helpers so camera transforms and piece-picking
geometry remain independently understandable and reusable. When those modules
change, manually verify grid preview placement, Shift+left piece selection,
plain-left placement near existing geometry, right-drag orbiting, WASD/QE
movement, shift-wheel zoom limits, and Home reset.

New editor sessions start with an empty layout. `CLEAR TRACK` must clear only
the current in-memory layout, reset its start/finish state, remain undoable with
`Ctrl+Z`, and leave saved custom drafts untouched. Manually check those cases
after changing editor commands or history.

When changing shared road geometry, place a branch and a merge in an editor
preview. Check that both visible arms can be selected with Shift+left-click,
that their neon rails only follow exposed road borders, and that a car can enter
either arm without a wall appearing across the road. The domain tests cover arm
expansion and surface contact; this smoke check covers the Raylib-facing picker
and renderer.

When changing time-trial presentation, manually open a race-ready editor layout
with `Tab`. Check the follow camera, the regular car and verified ghost draw
order, off-track and guardrail status messages, pause/reset feedback, and the
HUD's lap, timing, speed, and surface values.

When changing playable-package behavior, keep `Tab` as a transient preview:
create a race-ready draft, finish three laps, and use `E` to enter metadata and
save it. Confirm the package appears under `Ctrl+P`/`F6`, can be launched into
race mode with its ghost, and increments its version on a second export with the
same name. Add enough packages to use the library's next page, then launch one
from each page. Put a deliberately malformed `.nrplay` file in the playable
data folder and confirm its error leaves the current editor/race state
untouched. Also verify that an externally copied package whose filename contains
spaces launches correctly; the library must retain its exact discovered path.

## Git workflow

`main` represents integrated, verified releases. Create a focused branch for a
coherent change, for example `refactor/editor-commands` or
`feature/checkpoints`. Keep every commit buildable and run the relevant tests
before committing. Tag release candidates and releases with semantic versions,
for example `v0.2.0-alpha.0`.

To publish a tagged GitHub release, make sure the tag is reachable from `main`
and run `make publish-tag TAG=<version>`. The release workflow is triggered by
that one pushed tag; it builds and tests the Linux x86_64 archive before
creating or updating the GitHub release. A local tag alone cannot trigger a
GitHub workflow.

Do not commit generated executables, build directories, or editable user drafts.
Versioned example tracks belong in `assets/tracks/examples`.
