# Draft track format

Editable tracks use a whitespace-delimited text format. They are drafts, not
playable exports: they may be incomplete and never contain a ghost recording.
The shared structural records are also embedded in saved playable packages, but
the `.draft` file itself always remains editable-only; see
`playable-track-format.md` for that separate artifact.

```text
NEON_RACER_DRAFT 6
STATUS DRAFT
START <piece-id> <race-direction>
PIECES <count>
PIECE <id> <type> <entry-x> <entry-y> <entry-z> <heading> <width> <exit-width>
      <length> <curve-turn> <curve-radius> <elevation-delta> <lateral-offset>
      <surface-material> <curve-degrees> <bank-angle-degrees>
```

All positions and structural dimensions are integer grid values. One grid unit
is one metre. Enum fields are stored as their numeric C++ enum values, so they
must only be changed through a documented format-version migration.

## Safety limits

Persisted entry coordinates are accepted in the broad inclusive range
`-1,000,000` through `1,000,000`; a piece's elevation delta uses the same
range. These are arithmetic-safety bounds, not ordinary editor-world limits.
Each persisted layout may contain at most 128 pieces. The shared codec applies
these limits to both drafts and embedded playable layouts before road geometry
is evaluated, preventing malformed files from overflowing grid arithmetic or
triggering pathological overlap work.

Draft loaders accept only regular files no larger than 2 MiB and constrain
format labels to 64 bytes before comparing them. This keeps malformed text
from turning a filename or a single unbounded token into excessive parser work.

## Compatibility

The writer emits version 6. The loader accepts versions 1 through 6:

- v1: basic pieces and start/finish;
- v2: explicit `STATUS DRAFT` record;
- v3: endpoint width, elevation, lateral offset, and surface material;
- v4: curve extent (90°, 180°, or 270°);
- v5: curve bank angle (−45° through 45°).
- v6: an explicit Twist corkscrew radius. Versions 1–5 retain their flat
  rolling-Twist geometry when loaded.

When extending the format, increment the version, retain old-reader behavior
where practical, add a fixture or unit test for the prior version, and update
this document.

## Storage

Shipped examples live in `assets/tracks/examples`. Personal drafts live under
`$XDG_DATA_HOME/neon-racer/tracks`, falling back to
`~/.local/share/neon-racer/tracks`. `NEON_RACER_DATA_DIR` overrides the base
directory for development and portable installs. When the draft library opens,
the game imports legacy `tracks/custom/*.draft` files from the old repository
location without overwriting an existing user-data draft of the same name.
