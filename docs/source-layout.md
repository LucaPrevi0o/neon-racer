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
    presentation/
    persistence/
  persistence/
    common/
    draft/
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

## Naming

Prefer responsibility names over generic suffixes:

- `editor_input.cpp`, not `editor_helpers.cpp`;
- `editor_scene.cpp` and `editor_panel.cpp`, not one large `editor_rendering.cpp`;
- `playable_package_codec.cpp`, not `playable_utils.cpp`.

Headers should describe stable interfaces. Implementation-only constants, Raylib rectangles, formatting helpers, and local algorithms should remain private to the responsible `.cpp` file.
