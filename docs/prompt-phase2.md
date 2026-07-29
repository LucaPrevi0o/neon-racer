# Neon Racer — Historical milestone 2 brief

> This is a historical implementation brief. Paths and build commands below
> describe the pre-refactor project and are not current instructions.

Implement Milestone 2 for the existing Raylib/C++ project.

Read `README.md`, `docs/prompt.md`, and the current source before changing anything. Milestone 1 is complete and working: preserve its build command, existing editor workflow, race-preview flow, controls where practical, and neon visual style.

The objective is to evolve the current straight/curve closed-loop prototype into a robust custom-track editor with playable exports, ghost replays, and physically motivated 3D driving. Do not rewrite the project as a new engine or introduce external dependencies.

## Delivery strategy

Implement this in small, compiling vertical slices. After each meaningful slice, run `make build` and `make test` and fix regressions before continuing.

Prioritize features in this order:

1. Extend the track domain and draft persistence.
2. Extend editor placement, selection, property editing, validation, and undo/redo.
3. Render the new track geometry clearly in the existing neon style.
4. Add playable-track export/import and verification state.
5. Replace the prototype race movement with fixed-timestep 3D vehicle physics.
6. Add ghost recording, replay selection, and HUD integration.

Keep domain rules, rendering, editor UI, physics, replay, and persistence in separate modules. Avoid coupling Raylib drawing/input code directly into `Track`.

## Track model and editor

Extend the existing `TrackPiece` system instead of replacing it. Add the component types and parameters needed for:

- Straights with editable length, endpoint elevation, lateral offset, endpoint widths, and material.
- Curves supporting 90°, 180°, and 270° turns; radii; elevation change; and smoothly varying midpoint bank angle.
- Loops and twists, limited to a 20×20×20-cell bounding volume.
- Branch/merge components with individually configurable arms.
- Surface materials: regular, slippery, and high-resistance.

All layout values must remain grid-based integers where possible. Connectors must retain position, cardinal heading, width, and zero bank angle. Compatible connectors share position, heading, elevation, and width. Tangent slope deliberately does not affect compatibility.

Placement must reject overlapping *physical road geometry* while allowing track passages beneath bridges and through the empty interior of loops. Connector-boundary contact is allowed. Drafts may contain disconnected pieces, but validation must identify the affected components and explain why a track cannot be raced.

Implement editor support for:

- Component-library selection and an informative placement preview.
- Move, rotate, duplicate, delete, undo, and redo.
- A practical in-editor property inspector for the selected piece.
- Immediate validation feedback and in-world issue indicators.
- Start/finish selection only on eligible non-branching straight pieces.
- Forward/reverse race direction.

A race-ready graph must have one start/finish, no hanging connectors or branch arms, no unintended separate loops, and every reachable branch must merge before the start/finish.

## Persistence and exports

Maintain separate custom-draft and playable-track storage.

- Drafts are always editable and can be saved/imported/exported without a verification ghost.
- A playable export includes editable layout, physics-relevant parameters, metadata (name, creator, description, version), race direction, and a verification ghost.
- Playable export is allowed only after a saved completed three-lap ghost exists for the exact layout.
- Increment the playable-export version on each export.
- Any geometry, width, surface, bank, race-direction, or start/finish edit invalidates verification status and returns the track to draft state.
- Official tracks remain read-only; do not add in-game duplication/editing of them.

Use a documented, versioned, human-readable file format. Handle malformed files gracefully with an actionable error in the UI.

## Vehicle physics and race behavior

Replace the current scalar-speed prototype with a fixed-timestep rigid-body-style vehicle model containing:

- Position, orientation, linear velocity, angular velocity, mass, and collision body.
- Gravity, aerodynamic drag, acceleration, braking, reverse force, rolling resistance, lateral grip, and surface-dependent handling.
- Track-contact detection that derives local surface normal and applies traction/contact response.
- Airborne motion governed by momentum, gravity, drag, and angular motion.
- Stable simplified landing and guardrail collision responses.

The vehicle must follow ramps, banks, twists, and loops through contact response, not permanent attachment. It must become airborne when contact cannot supply the needed normal force.

Track pieces must expose a continuous drivable collision/query surface and separate guardrail collision geometry. Compatible joins must be gap-free, though slope discontinuities are allowed.

Implement fall recovery carefully: reset only when the bottom of the collision body has stayed airborne below the last driven surface for a configurable duration. Do not reset legitimate jumps above the track. Keep manual reset behavior: it restarts the complete three-lap run.

Preserve keyboard and gamepad support. Update the chase camera to follow steep, inverted, and airborne motion smoothly.

## Ghosts

Record a deterministic or sufficiently stable timestamped replay of each completed three-lap run. At completion, allow saving the replay as a ghost associated with the current custom track.

- Let the player select a saved ghost before racing.
- Render ghosts as non-colliding visual cars.
- Show time difference to the selected ghost in the HUD.
- Use the fastest saved valid three-lap ghost as the verification ghost in playable exports.

## Acceptance checks

Before finishing:

- `make racer` succeeds from the workspace root.
- Existing simple-loop drafts still load, or migration/fallback handling is provided.
- A player can create, save, reload, and test a valid elevated/banked custom track.
- Invalid geometry and graph states visibly explain why race mode/export is blocked.
- A completed three-lap run can be saved, selected as a ghost, and used for playable export.
- Editing the verified layout invalidates its playable-export eligibility.
- The car can drive, become airborne, land, collide with guardrails, and recover from a below-track fall.
- Update `racer/README.md` with all new controls, storage formats/locations, and any intentionally deferred limitations.

At the end, provide a concise summary of changed modules, verification performed, and known limitations.
