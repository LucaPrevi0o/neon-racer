# Product direction and design specification

## Document purpose

This document records Neon Racer's product intent and design invariants. It is
not the build guide, architecture reference, control manual, or serialization
specification.

Use these documents for implementation details:

- [`../README.md`](../README.md): build, launch, controls, and project overview;
- [`architecture.md`](architecture.md): module ownership and dependencies;
- [`development.md`](development.md): contributor and release workflow;
- [`track-format.md`](track-format.md): editable draft format;
- [`playable-track-format.md`](playable-track-format.md): frozen package format.

## Product vision

Neon Racer is a 3D Raylib/C++ game in which players construct custom tracks
from reusable components and test them in a physically motivated three-lap time
trial. It keeps the neon/space visual language of the earlier mini-game
collection while remaining a self-contained project.

The editor and race mode form one loop:

```text
build a layout
  → validate the race graph
  → preview and drive it
  → complete a verified run
  → export a frozen playable package
```

## Content classes

### Official tracks

Official tracks are shipped, versioned assets. They are read-only in the game
and remain separate from player-authored content.

### Editable drafts

A draft is the player's working layout:

- it may be incomplete or invalid for racing;
- it can be saved and loaded without a ghost;
- it remains editable;
- it never claims playable verification.

### Frozen playable packages

A playable package is a race-ready snapshot with metadata and a verified
three-lap ghost for the exact persisted layout. It is loaded as a frozen race
artifact rather than edited in place.

Any change to geometry, width, material, bank, race direction, or start/finish
identity breaks the relationship with the prior verification ghost.

## Track design invariants

### Grid and connectors

- One grid cell represents one metre.
- Persisted layout values remain integer-based where practical.
- Piece rotation and connector headings use the four cardinal grid directions.
- A connector carries position, heading, elevation, width, and zero bank at the
  boundary.
- Two connectors are compatible only when their position, travel heading,
  elevation, and width agree.
- Tangent slope is deliberately not part of connector compatibility, allowing
  differently sloped pieces to meet at a shared boundary.
- Compatible connector-boundary contact is allowed and is not treated as
  physical overlap.

### Physical geometry

Placement is decided from physical road geometry, not a component's coarse
bounding box. This permits:

- elevated roads to cross lower roads;
- a path to pass through the empty interior of a loop;
- joined pieces to share their connector boundary.

Actual road-surface overlap remains a placement error.

All consumers should derive roads from the shared path/surface geometry rather
than maintaining separate interpretations for rendering, picking, overlap, and
vehicle contact.

### Widths and materials

Each component has an incoming and outgoing road width from 5 through 11 grid
cells. A differing endpoint width produces a continuous width transition.

Every component has a surface material. The baseline materials are regular,
slippery, and high-resistance. Material affects handling properties such as
grip and rolling resistance without changing structural geometry.

### Current component model

- **Straight**: editable run length, endpoint widths, elevation delta, lateral
  offset, and material. An elevated straight can form a bridge.
- **Curve**: left/right turn, 90°/180°/270° extent, radius, endpoint widths,
  elevation delta, midpoint bank angle, and material. Bank returns smoothly to
  zero at both connectors.
- **Loop**: vertical full loop with editable radius, endpoint widths, elevation
  delta, lateral offset, and material.
- **Twist**: one-turn corkscrew with independent forward run and loop radius,
  endpoint widths, elevation delta, lateral offset, and material. For the
  standard 5-cell road, the default is a 25-cell run and 3-cell radius. Wider
  roads increase the safe minimums. Layout versions 1 through 5 retain their
  historical flat rolling-Twist path.
- **Branch/Merge**: split or join road arms using a run length, width, arm
  spread, and material.

Exact current parameter limits belong in track-domain code and behavior tests;
serialized compatibility belongs in the format documents.

## Race-ready graph

A draft becomes race-ready only when it has:

- exactly one selected start/finish on an eligible non-branching straight;
- no hanging connectors or branch arms;
- one connected race graph reachable from the start/finish;
- no unintended independent loops;
- every reachable branch merged back before returning to the start/finish;
- no forbidden physical road overlap.

Race direction may be selected in either orientation, but eligibility and graph
rules must remain valid for the chosen traversal.

A stronger checkpoint-based route proof is planned. Until then, a lap is
recognized when the car crosses the start/finish in the configured direction.

## Editor experience

The editor provides:

- a component library and grid-snapped placement preview;
- immediate validity feedback;
- selection and property editing;
- move/rotate-style preview manipulation, duplication, and deletion where
  supported by the active workflow;
- undo and redo;
- start/finish and race-direction selection;
- separate libraries for editable drafts and frozen playables;
- in-world indicators and actionable explanations for invalid race graphs.

New editor sessions begin empty. Clearing the current track affects only the
in-memory session and remains undoable; it does not delete saved content.

## Time-trial design

The primary mode is a single-car, continuous three-lap time trial.

A completed run records:

- individual lap times;
- best lap;
- total three-lap duration;
- a timestamped vehicle replay suitable for ghost playback and verification.

Reset restarts the complete run. Pause freezes both simulation and timer.
Ghosts are non-colliding visual cars and never affect physics.

## Vehicle behavior

The vehicle model aims for readable, realistic-feeling handling rather than a
full wheel-and-tyre simulator.

Required behavior includes:

- fixed-timestep updates;
- position, orientation, and linear velocity;
- acceleration, braking, reverse force, gravity, drag, rolling resistance, and
  lateral grip;
- surface-query-driven contact with local tangent and normal frames;
- surface-dependent handling;
- airborne motion when contact is lost;
- stable guardrail and landing response;
- manual full-run reset;
- camera behavior suitable for banks, ramps, loops, Twists, and airborne
  motion.

Ordinary roads use one-sided contact behavior. Configurable corkscrew Twists may
apply bounded local guidance while the vehicle remains close to their enclosed
road frame; a car that genuinely leaves the component must still be released.

Fall recovery must not reset a legitimate jump merely because it remains
airborne above the track.

## Verification and export invariants

A playable export is permitted only when:

- the frozen layout is race-ready;
- a complete three-lap ghost exists;
- the ghost belongs to the unchanged durable layout fingerprint;
- required metadata is valid.

The export contains the frozen layout, race direction, metadata, export
version, and verification ghost. Re-exporting the same name increments its
version and atomically replaces the current package rather than building an
implicit historical archive.

Malformed input and failed exports must leave the current editor or race state
unchanged.

## Roadmap

Planned extensions include:

- checkpoints and stronger lap-route validation;
- respawning at the latest checkpoint;
- more detailed branch-arm configuration;
- additional curve variants where the shared geometry model can support them
  safely;
- AI opponents and races;
- additional camera modes;
- audio for engine, collision, UI, music, and ambience;
- richer package discovery metadata such as tags, difficulty, and thumbnails.

These items are product direction, not claims about the current build.
