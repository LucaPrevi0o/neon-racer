# Development guide

## Build and test

Use either supported entry point from the repository root.

### Makefile

```sh
make build
make run
make test
```

### CMake

```sh
cmake -S . -B build/cmake
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

To build only Raylib-free code and tests:

```sh
cmake -S . -B build/cmake -DNEON_RACER_BUILD_APP=OFF
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

Track, race-physics, vehicle-dynamics, ghost-replay, time-trial,
playable-package, and main-menu-flow tests must neither include nor link
Raylib.

## Before changing code

1. Read [`architecture.md`](architecture.md) and identify the module that owns
   the behavior.
2. Keep gameplay rules in Raylib-free code.
3. Add or update behavior tests for geometry, validation, serialization,
   package policy, replay handling, or race-dynamics math.
4. If adding a source file, list it explicitly in both the Makefile and the
   appropriate CMake target.
5. Run the smallest relevant tests during development, then run the full suite
   before committing.

Explicit source lists are intentional: a missing module should fail review
visibly instead of being silently included by a wildcard.

## Change-specific verification

Automated tests cover domain behavior. The following smoke checks cover
interaction ordering, rendering, and application-state transitions that the
headless suite cannot fully exercise.

### Editor camera and picking

After changing editor input, camera, or picking:

- verify grid-snapped preview placement;
- select a component with Shift + left-click;
- place near existing geometry with plain left-click;
- orbit with right-drag;
- pan with `W`/`A`/`S`/`D`;
- move vertically with `Q`/`E`;
- test Shift + wheel zoom limits;
- reset the camera with Home.

Keep device polling and interaction ordering in `TrackEditor::Update`. Pass
`Camera3D` or `Ray` values into extracted helpers instead of coupling helpers to
global window state.

### Piece catalogue and palette

After changing component selection or the bottom palette:

- hover the collapsed handle and verify smooth expansion;
- click each card and confirm the expected component is selected without being
  placed;
- verify `1`–`6` still map to the same component types;
- confirm pointer actions over the palette do not rotate, select, or place in
  the world;
- open the custom-draft library and confirm the palette is inert behind the
  modal.

The catalogue remains Raylib-free so keyboard shortcuts and visual cards cannot
drift apart.

### Editor commands and history

After changing commands, history, or session reset:

- start a new editor session and confirm it is empty;
- use **CLEAR TRACK** and confirm only the in-memory layout and start/finish
  state are cleared;
- undo the clear with `Ctrl+Z`;
- confirm saved custom drafts remain untouched;
- verify a fresh session also resets undo/redo and the editor camera.

### Startup menu and application flow

After changing startup or screen transitions, verify keyboard and mouse input:

- **Create a new track** resets only the current editor session;
- opening and closing the playable library from the main menu returns to the
  main menu;
- launching a package enters its race and `Tab` returns to the main menu;
- launching the playable library from the editor returns a race to that editor;
- the same return behavior survives choosing another package from the
  completed-race export library.

### Shared road geometry

After changing road-arm expansion, surface sampling, overlap, picking, or
guardrails:

- place a branch and merge;
- select both visible arms with Shift + left-click;
- confirm neon rails follow only exposed borders;
- drive into either arm and confirm no wall crosses the road;
- test a Twist through upright, side-facing, and inverted sections;
- verify a legacy flat Twist still follows its original rolling-straight path
  when loaded from an older draft.

Domain tests should cover arm expansion, surface contact, and format migration.
The smoke test covers the Raylib-facing picker and renderer.

### Time-trial presentation

Open a race-ready editor layout with `Tab` and verify:

- chase-camera tracking on level, banked, steep, inverted, and airborne motion;
- regular-car and verified-ghost draw order;
- off-track and guardrail messages;
- pause and reset feedback;
- HUD lap, timing, speed, and surface values;
- the rendered chassis stays above its contact reference on banks and Twists.

### Playable packages

When changing package export, import, or libraries:

1. Create a race-ready draft and complete three laps.
2. Press `E`, enter metadata, and save.
3. Confirm the package appears under `Ctrl+P`/`F6`.
4. Launch it and verify its bundled ghost.
5. Export the same name again and confirm its export version increments.
6. Add enough packages to exercise pagination and launch one from each page.
7. Add a malformed `.nrplay` file and confirm the error leaves the current
   editor/race state unchanged.
8. Copy in a valid package whose filename contains spaces and confirm the
   library launches its exact discovered path.
9. Confirm unreadable, oversized, special, or colliding destination paths are
   not overwritten.

`Tab` remains an in-memory preview. Persistent export is a separate verified
operation.

### Persistence and compatibility

When changing either format:

- update the appropriate normative format document;
- increment the layout or package version when serialized meaning changes;
- retain supported old-reader behavior where practical;
- add a prior-version fixture or focused migration test;
- test malformed labels, counts, numeric bounds, trailing data, and regular-file
  limits;
- verify failed loads do not replace caller state;
- test atomic replacement and temporary-file cleanup.

## Dependency checklist

Before review, confirm:

- no Raylib type entered `src/track`, `src/persistence`, `src/playable`, or
  `src/race`;
- rendering or input code does not decide track validity or race rules;
- package policy remains outside track and race;
- shared road geometry is used by overlap, picking, rendering, and surface
  contact rather than reimplemented;
- persisted identity excludes generated runtime IDs and layout revision;
- new files appear in both build systems.

## Git workflow

`main` represents integrated, verified releases.

Create a focused branch for one coherent change, for example:

```sh
git switch -c docs/reorganize-guides
```

Keep each commit buildable where practical and run relevant tests before
committing. Use semantic version tags for release candidates and releases, for
example `v0.2.1-alpha.4`.

Do not commit generated executables, build directories, or editable user
drafts. Versioned examples belong under `assets/tracks/examples`.

## Publishing a tagged GitHub release

The release workflow is triggered by one pushed tag beginning with `v`. It
builds and tests the Linux x86_64 archive, publishes the archive and SHA-256
checksum, and creates or updates the corresponding GitHub release. Alpha, beta,
and release-candidate tags are published as prereleases.

The tag must point to a commit reachable from `main`. Publish it with:

```sh
make publish-tag TAG=v0.2.1-alpha.4
```

This pushes `main` and only the requested tag. A tag that exists only in a local
clone cannot trigger GitHub Actions.
