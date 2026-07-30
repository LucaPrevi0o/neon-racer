# Architecture

Neon Racer is organized around one rule: gameplay data and rules do not depend
on Raylib. Raylib belongs at the application's presentation boundary.

```text
app ───────► editor ───────► persistence
 │              │                 │
 │              └──────► track ◄──┘
 │                            ▲
 ├──────► race ───────────────┤
 ├──────► render ─────────────┤
 └──────► ui                  │
```

`src/track` is the authoritative layout domain. It owns track pieces,
connectors, geometry sampling, overlap checks, surface queries, validation, and
the frozen playable-track snapshot. It must stay free of Raylib types.

`src/editor` owns mutable editing state and communicates with the track domain
through its public model API. `src/persistence` reads and writes drafts without
knowing how they are displayed. `src/race` uses track surface queries to run a
time trial from a plain input snapshot supplied by the application boundary.

`src/render` and `src/ui` are presentation code. They may depend on Raylib and
on read-only domain types, but neither may define game rules. `src/app` is the
composition root: it creates the window, chooses the active screen, and wires
the modules together.

The CMake build mirrors these boundaries: `neon_racer_domain` owns the
Raylib-independent track and persistence layer;
`neon_racer_vehicle_dynamics` builds vehicle simulation on that domain; and
`neon_racer_time_trial` adds time-trial orchestration on top. The `neon-racer`
executable links those Raylib-free libraries to the Raylib-backed application
modules.

## Current refactoring boundaries

The present folders establish module ownership without changing gameplay.
`track_rules.cpp` owns component admissibility and the sample circuit.
`track_layout.cpp` owns mutable layout state, `track_piece_geometry.cpp` owns
piece grid and path geometry, `track_surface_geometry.cpp` owns sampled road
geometry, and `track_road_geometry.cpp` turns layout components into their
physical road arms and side axes for all consumers. `track_overlap.cpp` owns
overlap checks.
`track_surface_query.cpp` owns road contact and guardrail queries.
`track_graph.cpp` owns indexed connector matching and `track_validation.cpp`
owns race-readiness policy. `editor_history.cpp` owns undo/redo snapshots;
`editor_library.cpp` owns draft-library interaction; `editor_camera.cpp` owns
editor-view pan, zoom, orbit, elevation, and reset transforms; and
`editor_picking.cpp` converts editor rays into grid positions and nearest
track-piece selections. `editor.cpp` remains the coordinator for input
dispatch, mutable editor state, commands, and drawing.
`app/raylib_race_input.cpp` translates Raylib devices into a plain `RaceInput`;
`TimeTrial` receives that frame snapshot and reuses its held axes for every
fixed step. `race_contracts.hpp` owns the simulation's vector, vehicle, and
ghost values. `vehicle_dynamics.hpp/.cpp` owns surface-query-driven suspension,
steering, traction, braking, air drag, and guardrail response behind a narrow
`VehicleSurfaceQuery` interface. `ghost_replay.hpp/.cpp` owns candidate sample
capture, fastest-run replacement, replay interpolation, and looping playback.
`time_trial.cpp` keeps the `TimeTrial` facade, fixed-step scheduling, lap policy,
layout-revision invalidation, verification policy, and player-facing status
messages. The Raylib-free `race_physics.cpp` owns brake-damping policy and its
digital-input cap.

## Data locations

Versioned example tracks are assets under `assets/tracks/examples`. Editable
runtime drafts are written outside the repository under the operating-system
application-data directory; see `track-format.md` for the exact lookup order.
