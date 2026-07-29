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
time trial and should eventually receive a plain input value instead of polling
Raylib directly.

`src/render` and `src/ui` are presentation code. They may depend on Raylib and
on read-only domain types, but neither may define game rules. `src/app` is the
composition root: it creates the window, chooses the active screen, and wires
the modules together.

## Current refactoring boundaries

The present folders establish module ownership without changing gameplay.
`track_rules.cpp` owns component admissibility and the sample circuit.
`track_surface_query.cpp` owns road contact and guardrail queries. Later commits
will split the remaining `track.cpp` into model and geometry services.
`track_graph.cpp` owns indexed connector matching and `track_validation.cpp`
owns race-readiness policy. `editor_history.cpp` owns undo/redo snapshots;
`editor_library.cpp` owns draft-library interaction; later commits will split
the remaining editor code into state, picking, and UI.
`race_input.cpp` translates Raylib devices into a plain `RaceInput`; later
commits will split `race.cpp` into vehicle dynamics, time-trial, and
ghost-replay services.

## Data locations

Versioned example tracks are assets under `assets/tracks/examples`. Editable
runtime drafts are written below `tracks/custom`, which is ignored by Git. A
future persistence milestone will relocate runtime drafts to an operating-system
application-data directory and import existing local drafts.
