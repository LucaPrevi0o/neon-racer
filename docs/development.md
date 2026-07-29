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

Track and isolated race-physics tests must not include or link Raylib. The
headless time-trial test temporarily needs Raylib headers because `RaceCar`
still exposes `Vector3`; the next race-core split will remove that dependency.
Add behavior tests under `tests/unit` whenever changing track geometry,
validation, serialization, playable-export rules, or isolated race-dynamics
math.

## Dependency rules

`src/track` and `src/persistence` are the domain layer and must remain free of
Raylib types. `src/editor`, `src/race`, `src/render`, and `src/ui` may use
Raylib, but rendering and input adapters must not define track or race rules.
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

## Git workflow

`main` represents integrated, verified releases. Create a focused branch for a
coherent change, for example `refactor/editor-commands` or
`feature/checkpoints`. Keep every commit buildable and run the relevant tests
before committing. Tag release candidates and releases with semantic versions,
for example `v0.2.0-alpha.0`.

Do not commit generated executables, build directories, or editable user drafts.
Versioned example tracks belong in `assets/tracks/examples`.
