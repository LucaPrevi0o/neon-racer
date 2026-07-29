# Neon Racer - 3D racing and track building game

The goal is to build a 3D game based on Raylib/C++ that allows the player to build a custom
track, using pre-defined assets (straights, curves, and obstacles like ramps and loops) and testing
them in a racing environment.

The game inherits the shared neon/space visual style of the existing games through
the `shared/neon` module. Reusable extensions may be added to that module when the
racer needs them, while preserving visual consistency with the rest of the games.

## Technical baseline

The immediate technical requirement is that the project compiles and runs through
the provided Makefile. No platform-specific compatibility or frame-rate target is
required at this stage.

## Core gameplay loop

The game provides an in-game track editor and a time-trial racing mode.

The developer uses the editor to create and ship official tracks. Players can use
the same editor to create, save, export, import, and manage custom tracks.
Official and custom tracks are kept separate in the user interface and storage.
Official tracks are read-only and cannot be copied or edited in-game; all
player-authored content remains in the custom-track space.

A playable track must form one complete closed race graph using compatible,
correctly connected track pieces. All permitted branches must merge back before
the start/finish line, with no hanging routes. The editor must clearly identify
incomplete or invalid connections and explain why the track cannot be tested.

The primary race mode is a single-car, three-lap time trial. Each run records both
the fastest individual lap and the total duration of all three laps. Players can
race against a selected saved ghost replay.

AI opponents are a future extension and are not required for the first playable
version.

## Track persistence and verification

At the end of each completed time-trial run, the player may choose to save its
ghost replay for later selection in races. Tracks can be saved and exported at any
time as editable draft tracks. A completed custom track can be exported as a
playable track only when at least one ghost has been saved for it. The exported
track includes the fastest saved three-lap run as its verification ghost, proving
that the exact exported layout is drivable. During a race, ghosts are
non-colliding visual replays and never affect vehicle or track physics.

A layout without a verification ghost may be saved or exported only as an editable
draft and cannot be raced as a playable track. Playable track exports include their
editable layout, physics-relevant parameters, required metadata, race direction,
and verification ghost.

Playable-track metadata must include a track name, creator name, description, and
an automatically managed version. The version increments on every playable export.
Tags and difficulty are optional; a track thumbnail may be generated automatically.

Any edit to track geometry, surface material, width, banking, race direction, or
start/finish placement invalidates the verification ghost and returns the track to
draft status until another successful lap is recorded.

## Implementation milestones

### Milestone 1: playable foundation

- The project builds and runs through the provided Makefile, using Raylib/C++ and
  the shared neon visual system.
- A player can create a closed, non-intersecting track from basic straights and
  curves, select a start/finish line and race direction, and receive validation
  feedback.
- A single car can be driven with keyboard or gamepad using the fixed chase camera.
- The game supports a basic time trial, lap timing, and reset-to-start recovery.

### Milestone 2: full editor and vehicle feature set

- Add the complete component and editing system: variable widths, elevation,
  ramps, bridges, offset straights, banking, surface materials, loops, twists,
  branches, merges, drafts, and custom-track import/export.
- Add saved ghost replays and verification-ghost export for playable custom tracks.
- Complete the physically motivated contact, airborne, collision, and
  last-surface fall-recovery behaviour described in this document.

### Future extensions

- Checkpoints and stronger lap-route validation.
- Respawning at the latest checkpoint.
- AI opponents and races.
- Additional camera modes.
- Audio, including engine, collision, UI, and music sound.

## Track editor, placement, and camera

The track editor is separate from race mode and uses an external isometric 3D camera.
The player selects pieces from a component library and receives a placement preview
before confirming it.

The editor uses a three-dimensional grid with 1-metre cells. All component
dimensions and positions are expressed as whole multiples of this grid size.
Components can be rotated only in 90-degree increments: 0°, 90°, 180°, and 270°.
Curve components are available only as 90°, 180°, or 270° sections.

The player controls a component's grid position and rotation. Placement is
permitted only when the component's physical track geometry does not overlap
existing track geometry. Empty space inside a component's overall bounds is not
reserved: this permits a track to pass beneath a bridge or through the empty
interior of a loop.

The placement preview displays the selected component's bounding box, orientation,
entry connector, and exit connector. It clearly indicates when its connectors
align with an existing adjacent piece, and changes appearance when placement is
invalid.

Each track piece has defined connectors at its start and end. Consecutively placed
pieces must connect through compatible connectors. Compatibility requires equal
grid position and elevation, matching cardinal travel heading, 0° bank angle, and
equal track width. Tangent slope is deliberately not part of compatibility, so a
ramp may connect directly to a level or differently sloped component. Compatible
pieces may share their connector boundary without being treated as overlapping
geometry. Component parameters—such as height, ramp steepness, curve radius, and
banking—change the position and orientation of the exit connector; the next
component must be placed at a compatible resulting connector.

When the player selects a placed component, the editor displays a property
inspector for that component. The inspector exposes the parameters relevant to
its type, including dimensions, endpoint position and elevation, width, surface
material, banking, and branch-arm settings. Changes update the placement preview,
connection indicators, and track-validity feedback immediately.

The first editor version supports selecting, moving, rotating, duplicating, and
deleting components, together with undo and redo for editing actions.

Overlapping physical track geometry is a placement-blocking error: an overlapping
component cannot be placed in the grid. Disconnected connectors and hanging branch
arms are permitted in drafts, but prevent the track from becoming race-ready. The
editor displays simple in-world UI indicators to identify the current track issues
and their affected components.

## Track width and guardrails

Every component has small guardrails intended to prevent ordinary accidental exits
from the road, while still allowing sufficiently high jumps or impacts to leave the
track.

All components use the same default track width. The editor allows the player to
edit the width at each endpoint, from 5 to 11 grid cells inclusive. A connector
can join only to another connector with the same width. When a component's entry
and exit widths differ, its road width changes linearly along its length, allowing
smooth transitions between compatible pieces of different widths.

## Branches and route validation

Most track components have one entry connector and one exit connector. A dedicated
branch component has one normal variable-width track connector and multiple
outgoing connectors. Its geometry begins as a straight track section and divides
into independent straight branch arms. Each arm has its own exit connector and can
use a different steepness and exit width. The same component, when rotated in the
opposite direction, may merge multiple incoming paths into one exit.

Every connector on a branch or merge component must be connected for a track to be
race-ready; hanging branch arms are not permitted.

A playable track has exactly one start/finish position. Once the track is complete
(a closed loop with no intersecting surfaces or hanging routes), the editor finds
all eligible straight components: components that are non-branching and have no
branch or merge immediately before or after them. The player chooses one eligible
straight on which to place the start/finish line, and selects clockwise or
counter-clockwise race direction. Eligibility and branch validation must hold for
both possible traversal directions.

Every branch reachable from the start/finish line must merge back into a single
path before returning to it. The editor must reject a race-ready track if a branch
has no valid route back to the finish, or if it creates an unintended separate
loop.

During a race, the player chooses a branch by steering into the desired path.
Any branch route that merges back into the main route before the start/finish line
is a valid continuation of the lap. A checkpoint system is a future extension and
is not required for the first playable version. For now, a lap counts whenever the
car crosses the start/finish line in the configured race direction; stronger
route-validation rules are deferred to the checkpoint system.

## Racing controls and camera

The first playable version uses a fixed third-person chase camera positioned behind
and above the car. The camera follows the car smoothly and remains suitable for
steep ramps, banked curves, loops, and airborne movement. Additional camera modes,
such as cockpit or free cameras, are future extensions.

The game supports keyboard and gamepad controls from the first playable version.
Keyboard input uses digital steering, acceleration, braking, reversing, and reset
actions. Gamepad input uses the analogue left stick for steering, analogue inputs
for acceleration and braking, and a separate reverse action.

At any time, the player can reset the car to the race start position. Resetting
restarts the entire three-lap time trial and clears its timer; a run must be
completed continuously from start to finish to be recorded or saved as a ghost.
Respawning at the last checkpoint is a future extension, introduced together with
the checkpoint system.

The first-version racing HUD displays the current lap, current lap time, best lap
time, total time, current speed, time difference from the ghost replay, and reset
and pause controls. Pausing freezes both the vehicle simulation and the time-trial
timer.

## Physics engine

Implement a fixed-timestep, rigid-body vehicle simulation that provides
realistic-feeling—rather than fully simulation-grade—handling.

The vehicle model tracks position, orientation, linear velocity, angular velocity,
mass, and a collision body. It is not required to simulate individual wheels,
tyres, or suspension. While the car is in contact with the track, the simulation
calculates the local track normal and applies contact, traction, braking, rolling
resistance, and lateral-grip forces.

Gravity is a constant world-down force. The car is not artificially attached to
the track: on ramps, banked curves, and loops, loss of sufficient normal contact
force causes the car to become airborne. Its airborne movement is determined by
its current linear and angular momentum, gravity, and aerodynamic drag.

While contact is maintained, the car aligns with the local track normal so it
follows ramps, banks, twists, and loops. This alignment is a physical contact
response, not a permanent constraint: when the required contact force is no longer
available, or collision forces overcome it, the car detaches and its orientation
continues according to its angular momentum and external forces.

Landing and barrier collisions must be stable and readable. A simplified
bounce/spin response is sufficient; the goal is believable recovery rather than
high-fidelity crash simulation. Guardrails are normally present to prevent common
accidents, but jumps and large elevation changes may allow the car to leave the
track and fall. The player can then reset to the race start position.

Vehicle handling must account for acceleration, separate braking and reverse
forces, momentum in all axes, aerodynamic drag, rolling resistance, and surface
grip. Grip influences steering response, lateral sliding, and the likelihood of
drifting or losing control. Drifting is an emergent result of insufficient lateral
grip, not a separate player-triggered action.

Track geometry and surface material are independent. Every playable track piece
has a valid surface material, defaulting to `regular track` when placed from the
library. Surface materials define properties such as grip and rolling resistance;
examples include regular, slippery, and high-resistance surfaces.

Each component exposes one continuous drivable collision surface. Compatible joins
must be gap-free, but may intentionally have a normal discontinuity where tangent
slope changes. Guardrails are separate collision geometry.

The car resets only when the bottom of its collision body has remained airborne
below the track surface on which it was last driving for a configurable duration.
It does not reset merely because it has been airborne for a long time above the
track, allowing legitimate jumps.

## Track components

Intended track components are listed here:
- **Straight**: a track piece with a length from 1 to 8 whole grid cells. Its
  endpoint elevation is independently editable, so it can be level or form a
  ramp. Ramp steepness is determined by the endpoint elevation change relative
  to its horizontal length; both gentle and steep ramps are supported.
  A straight at a higher elevation can act as a **bridge**, allowing a track to
  pass beneath it for figure-8 layouts.
  A straight may also offset its exit connector to the local left or right while
  preserving grid-aligned entry and exit connectors. The road surface transitions
  smoothly between those connectors, and the offset can be combined with an
  elevation change to create a slanted ramp. The maximum offset is 2 grid cells
  for a straight shorter than 4 grid cells, and 3 grid cells otherwise.
- **Curve**: a curved track piece with a turn of 90°, 180°, or 270°. A 90° or
  270° curve may be circular or elliptical, defined by independent forward and
  lateral integer-grid radii from 1 to 8 cells. Its entry and exit connectors
  remain grid-aligned, while their elevation may differ to create a rising or
  falling curve. The editor rejects a curve when its minimum radius of curvature
  cannot safely contain the selected track width, guardrails, and clearance.
  A 180° curve is always a semicircle: its ends have opposite directions and may
  differ only in elevation and lateral distance, and its radius is from 1 to 8
  grid cells. A curve can also be a **banked curve**. Its bank angle is always 0° at both connectors; the player sets a
  signed maximum bank angle at the curve midpoint, and the banking is smoothly
  interpolated from each endpoint to that midpoint. Positive and negative values
  create inward and outward banking respectively. The maximum absolute bank angle
  is 45°.
- **Twist**: a 360-degree rotation around the direction of travel. Twists are
  parameterised 3D components whose exit connector's height and direction can be
  edited, allowing non-planar variants. The exit heading is one of the four
  cardinal grid headings and its bank angle is 0°. The resulting entry and exit
  connectors must still satisfy the normal grid, collision, and connection
  validation rules.
- **Loop**: a 360-degree loop around the local left/right axis.
  Loops are parameterised 3D components: their exit connector's height and
  direction can be edited, allowing non-planar loop or corkscrew variants. The
  exit heading is one of the four cardinal grid headings and its bank angle is
  0°. The resulting entry and exit connectors must still satisfy the normal grid,
  collision, and connection validation rules.

  A loop or twist may occupy at most a 20 × 20 × 20 grid-cell bounding volume.
  This provisional limit is expected to be tuned through play testing.
- **Surface materials**: every track component has a surface material, defaulting
  to regular track. Slippery materials reduce lateral grip and make it more likely
  for the car to drift or lose control; other materials may vary rolling resistance.
