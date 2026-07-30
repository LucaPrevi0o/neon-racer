# Architecture

## Design rule

Gameplay data and rules do not depend on Raylib. Raylib belongs at the
application and presentation boundary.

```text
                         ┌─────────────┐
                         │     app     │
                         └──────┬──────┘
              ┌─────────────────┼─────────────────┐
              ▼                 ▼                 ▼
          editor             render              ui
              │                 │
              ├──────────┐      │
              ▼          ▼      ▼
          persistence  playable
              │          │
              └────┬─────┘
                   ▼
                  track ◄──────── race
```

The arrows express allowed knowledge, not necessarily direct build-target
links. Core modules exchange plain value types and interfaces; presentation
modules may read domain state but do not define track or race rules.

## Dependency rules

- `src/track`, `src/persistence`, `src/playable`, and `src/race` are
  Raylib-free.
- `src/editor`, `src/render`, and `src/ui` may use Raylib.
- `src/app` is the composition root and owns screen/session transitions.
- `src/track` and `src/race` do not depend on `src/playable`.
- `src/playable` may depend on track contracts and value-only race contracts.
- Rendering and input adapters may translate domain state, but may not decide
  validation, physics, persistence, or export policy.
- Persisted layout identity is defined by the track domain, not by generated
  runtime IDs or presentation state.

## Module ownership

### `src/track`: authoritative layout domain

The track module owns:

- track pieces and connectors;
- mutable layout state;
- grid and continuous path geometry;
- physical road-arm expansion and side axes;
- overlap detection;
- surface and guardrail queries;
- connector graph construction;
- race-readiness validation;
- the sample circuit;
- durable layout fingerprints.

Key implementation ownership:

| File | Responsibility |
| --- | --- |
| `track_layout.cpp` | Mutable layout operations |
| `track_rules.cpp` | Piece admissibility and sample layout |
| `track_piece_geometry.cpp` | Grid cells, connectors, and path points |
| `track_surface_geometry.cpp` | Sampled road frames |
| `track_road_geometry.cpp` | Physical road arms and side axes |
| `track_overlap.cpp` | Physical overlap checks |
| `track_surface_query.cpp` | Road contact and guardrail queries |
| `track_graph.cpp` | Indexed connector matching |
| `track_validation.cpp` | Race-readiness policy |
| `track_fingerprint.cpp` | Durable persisted-layout identity |

Track code must remain free of Raylib types.

### `src/editor`: mutable editing workflow

The editor owns the active preview, selection, commands, property adjustment,
validation feedback, undo/redo, and editor drawing. It changes layouts through
the public track API. Its cross-module declarations live under
`include/neon_racer/editor`; implementations are grouped by responsibility.

| Path | Responsibility |
| --- | --- |
| `core/editor.cpp` | Editor state construction, preview snapshots, cache coordination, and public access |
| `interaction/editor_input.cpp` | Raylib input polling, modal routing, selection, and camera bindings |
| `model/editor_preview.cpp` | Preview type conversion, dimensions, and property editing |
| `presentation/editor_scene.cpp` | In-world road, connector, validation, and preview drawing |
| `presentation/editor_panel.cpp` | Main editor panel and property inspector drawing |
| `persistence/editor_library_ui.cpp` | Draft-library input and modal presentation |
| `editor_commands.cpp` | Placement, transform, duplicate, delete, clear, and start/finish commands |
| `editor_history.cpp` | Undo/redo snapshots |
| `editor_library.cpp` | Draft listing, save, and load operations |
| `piece_catalog.cpp` | Raylib-free names, shortcuts, and thumbnail prototypes |
| `piece_palette.cpp` | Palette animation, hit testing, and presentation |
| `editor_camera.cpp` | Pan, zoom, orbit, elevation, and reset transforms |
| `editor_picking.cpp` | Grid placement and nearest-piece selection |

Device polling and interaction ordering stay in `TrackEditor::Update` within the
interaction unit. Extracted helpers receive values such as `Camera3D` and `Ray`
so their geometry remains independently understandable. See
[`source-layout.md`](source-layout.md) for the physical header/source convention.

### `src/persistence`: durable storage boundary

Persistence owns:

- editable draft reads and writes;
- playable-package reads and writes;
- storage-path resolution;
- regular-file and file-size preflight checks;
- bounded format-label parsing;
- the shared structural layout codec;
- temporary-file and atomic-replacement behavior.

The public playable storage declaration lives at
`include/neon_racer/persistence/playable_track_io.hpp`. New callers use the
`neon_racer/persistence/playable_track_io.hpp` include prefix, while
implementation-only interfaces stay under `src/persistence/internal`.

| Path | Responsibility |
| --- | --- |
| `common/atomic_file_writer.cpp` | Bounded temporary sibling writes, flush, close, cleanup, and atomic rename |
| `draft_io.cpp` | Editable draft listing, migration, save, and load orchestration |
| `playable/playable_library_storage.cpp` | Custom package paths, discovery, sorting, and export version selection |
| `playable/playable_package_codec.cpp` | Normative `.nrplay` grammar, embedded layout, ghost samples, and legacy migration |
| `playable/playable_package_validation.cpp` | Race readiness, metadata, fingerprint, and verified-ghost checks |
| `playable/playable_package_store.cpp` | File preflight plus package save/load orchestration |
| `storage_paths.cpp` | Runtime data directories, safe file stems, and filesystem preflight |
| `track_layout_codec.cpp` | Shared versioned track-layout serialization |

It does not decide how a loaded object is rendered or whether the application
changes screens after an operation.

### `src/playable`: frozen-package policy

The playable module owns the cross-domain package contract and export policy. It
builds either:

- an unverified in-memory preview of a race-ready editor layout; or
- a persistent verified package for the exact completed layout and ghost.

It may depend on track values and value-only race replay contracts. Track and
race remain independent of it.

### `src/race`: simulation and time-trial rules

The race module owns the plain input snapshot, vehicle state, surface-query
driving, ghost replay, lap policy, and verification lifecycle.

| File | Responsibility |
| --- | --- |
| `race_contracts.hpp` | Vector, vehicle, and ghost-transfer value types |
| `race_physics.cpp` | Raylib-free brake-damping policy |
| `vehicle_dynamics.*` | Suspension/contact, steering, traction, braking, drag, guide contact, and guardrail response |
| `ghost_replay.*` | Sample capture, fastest-run replacement, validation, interpolation, and playback |
| `time_trial.cpp` | Fixed-step scheduling, laps, resets, layout invalidation, verification, and status messages |

`VehicleDynamics` depends on the narrow `VehicleSurfaceQuery` interface rather
than presentation or editor state. `TimeTrial` receives one input snapshot per
application frame and reuses its held axes for each fixed simulation step.

### `src/render` and `src/ui`: presentation

These modules may use Raylib and read domain state.

- `src/render` owns track, car, ghost, and time-trial presentation.
- `time_trial_renderer.cpp` owns the chase camera, race composition, and HUD.
- `src/ui` owns shared neon visual primitives.

Presentation code does not define geometry validity, race results, package
eligibility, or storage behavior.

### `src/app`: composition root

The application module creates the window, selects the active screen, and wires
the other modules together.

`main_menu_flow.*` is deliberately Raylib-free: it stores the selected choice
and emits a one-shot semantic action. `main_menu.*` supplies input and
presentation. `RacerApplication` decides whether the action opens the playable
library or starts a new editor session.

`playable_library.cpp` owns the paged library and export metadata form.
`RacerApplication` validates launch/export results before changing session
state. A race remembers whether it originated from the main menu or editor so
`Tab` returns to the correct screen.

## Core runtime flows

### Editor preview

```text
Editor input
  → TrackEditor mutates or previews Track
  → Track validates geometry and graph state
  → Tab requests a frozen in-memory preview
  → TimeTrial consumes the frozen layout through surface queries
  → Render reads TimeTrial and Track state
```

The preview is transient. It does not create a `.nrplay` package.

### Verified export

```text
Race-ready frozen layout
  → completed three-lap run
  → verified ghost bound to layout fingerprint
  → playable export policy checks unchanged identity
  → persistence writes a complete temporary package
  → atomic rename replaces the destination
```

Any relevant layout or race-direction change invalidates the verification
relationship.

### Playable-package load

```text
storage preflight
  → package and embedded-layout parse
  → fingerprint and ghost validation
  → race-readiness validation
  → application replaces active package/session
```

A failed load leaves the caller's current editor or race state unchanged.

## Build targets

The build mirrors the module boundaries:

- `neon_racer_domain` owns Raylib-free track code, the draft layout codec, and
  the shared atomic-file primitive.
- `neon_racer_vehicle_dynamics` builds simulation on the domain.
- `neon_racer_time_trial` adds time-trial orchestration.
- `neon_racer_playable` combines package policy, persistence, and replay
  transfer contracts.
- `neon-racer` links the Raylib-free libraries with the Raylib-backed
  application, editor, renderer, and UI.

Source files are listed explicitly in both the Makefile and CMake targets. See
[`development.md`](development.md) before adding or moving a module.

## Data boundary

Versioned example drafts live under `assets/tracks/examples`. Runtime user data
is stored outside the repository in sibling `tracks` and `playables`
directories. `storage_paths.cpp` owns lookup and migration behavior.

The normative serialization rules are documented in:

- [`track-format.md`](track-format.md)
- [`playable-track-format.md`](playable-track-format.md)
