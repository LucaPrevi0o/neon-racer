# Source layout

Neon Racer separates declarations from implementations and lets directory depth express ownership.

## Convention

- Public and cross-module declarations live under `include/neon_racer/<module>/...`.
- C++ implementations live under `src/<module>/...`.
- Deeper implementation directories describe responsibilities such as `interaction`, `presentation`, `persistence`, or `model`.
- A source file includes its matching project header through the `neon_racer/...` include prefix.
- Private helpers that are used by only one translation unit stay in that `.cpp` file.
- A private declaration shared by several implementation files belongs under an `internal` header directory, not beside `.cpp` files.
- Build targets expose `${PROJECT_SOURCE_DIR}/include` as the project include root.

## Target structure

```text
include/
  neon_racer/
    app/
    editor/
    persistence/
    playable/
    race/
    render/
    track/
    ui/

src/
  app/
    flow/
    presentation/
  editor/
    core/
    interaction/
    model/
    presentation/
    persistence/
  persistence/
    common/
    draft/
    internal/
    playable/
  playable/
  race/
    simulation/
    replay/
    time_trial/
  render/
  track/
    model/
    geometry/
    validation/
    query/
  ui/
```

This is a migration direction, not a requirement to move every file in one change. Each refactor should preserve behavior, update both build systems, and leave every intermediate commit buildable where practical.

## Current migration status

### Editor

```text
include/neon_racer/editor/
  editor.hpp
  editor_camera.hpp
  editor_picking.hpp
  piece_catalog.hpp
  piece_palette.hpp

src/editor/
  core/editor.cpp
  interaction/editor_input.cpp
  model/editor_preview.cpp
  presentation/editor_scene.cpp
  presentation/editor_panel.cpp
  persistence/editor_library_ui.cpp
```

The smaller editor implementation files remain at `src/editor` temporarily. Moving them into the responsibility folders is a later mechanical step and should not be mixed with behavior changes.

### Playable persistence

```text
include/neon_racer/persistence/
  playable_track_io.hpp

src/persistence/
  common/atomic_file_writer.cpp
  internal/atomic_file_writer.hpp
  internal/playable_package_codec.hpp
  internal/playable_package_validation.hpp
  playable/playable_library_storage.cpp
  playable/playable_package_codec.cpp
  playable/playable_package_store.cpp
  playable/playable_package_validation.cpp
```

The public storage API is separate from implementation-only codec and validation declarations. Draft and playable writes share the same bounded temporary-file and atomic-rename primitive. A forwarding header remains at the former `src/persistence/playable_track_io.hpp` path only for older source and test includes; it should disappear when those modules migrate their own declarations.

## Suggested migration order

1. **Application UI:** separate playable-library state/input from its Raylib presentation.
2. **Race:** group vehicle simulation, replay, and time-trial orchestration under distinct subdirectories.
3. **Track:** move stable declarations into the public include tree, then group model, geometry, validation, and query implementations.
4. **Rendering and UI:** migrate declarations last, after their domain-facing dependencies use stable include paths.
5. **Compatibility cleanup:** remove forwarding headers once no source or test includes the legacy paths.

Each step should update both build manifests and add or retain a CI target that compiles the affected code. Moving files without changing behavior is preferable to combining a directory migration with new gameplay rules.

## Naming

Prefer responsibility names over generic suffixes:

- `editor_input.cpp`, not `editor_helpers.cpp`;
- `editor_scene.cpp` and `editor_panel.cpp`, not one large `editor_rendering.cpp`;
- `playable_package_codec.cpp`, not `playable_utils.cpp`.

Headers should describe stable interfaces. Implementation-only constants, Raylib rectangles, formatting helpers, and local algorithms should remain private to the responsible `.cpp` file.
