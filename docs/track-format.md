# Editable draft track format

## Purpose

Editable custom tracks use a versioned, whitespace-delimited `.draft` format.
A draft may be incomplete or invalid for racing. It never contains replay data,
verification authority, or playable-export status.

Playable packages embed the same structural layout records, but are a separate
artifact documented in [`playable-track-format.md`](playable-track-format.md).

## Version 6 grammar

```text
NEON_RACER_DRAFT 6
STATUS DRAFT
START <piece-id> <race-direction>
PIECES <count>
PIECE <id> <type> <entry-x> <entry-y> <entry-z> <heading>
      <width> <exit-width> <length> <curve-turn> <curve-radius>
      <elevation-delta> <lateral-offset> <surface-material>
      <curve-degrees> <bank-angle-degrees>
```

There is one `PIECE` record for each declared piece.

## Field semantics

- Positions and structural dimensions are integer grid values.
- One grid unit represents one metre.
- `width` and `exit-width` describe the road width at the two connectors.
- `length`, `curve-radius`, `curve-degrees`, `bank-angle-degrees`,
  `elevation-delta`, and `lateral-offset` are interpreted according to the piece
  type.
- Version 6 stores an explicit corkscrew radius for a Twist.
- `START` stores the selected start/finish piece and race direction.
- Enum fields are serialized as their numeric C++ values. They must change only
  through a documented format-version migration.

The shared structural codec is also used inside `.nrplay` packages.

## Compatibility

The current writer emits version 6. The loader accepts versions 1 through 6.

| Version | Added serialized meaning |
| --- | --- |
| v1 | Basic pieces and start/finish |
| v2 | Explicit `STATUS DRAFT` record |
| v3 | Endpoint width, elevation, lateral offset, and surface material |
| v4 | Curve extent: 90°, 180°, or 270° |
| v5 | Curve bank angle from −45° through 45° |
| v6 | Explicit Twist corkscrew radius |

Versions 1 through 5 did not store a corkscrew radius. Their Twist pieces are
loaded with the legacy flat rolling-Twist geometry so old layouts preserve
their path and connector semantics when loaded and re-saved.

## Validation and safety limits

The loader applies structural limits before evaluating road geometry:

- entry coordinates: `-1,000,000` through `1,000,000`, inclusive;
- elevation delta: `-1,000,000` through `1,000,000`, inclusive;
- maximum persisted pieces: 128;
- maximum draft file size: 2 MiB;
- maximum format-label length: 64 bytes;
- input must be a regular file.

These broad numeric limits protect arithmetic and parser resources. They are
not normal editor-world limits.

A malformed or unsupported draft fails with an error and does not replace the
caller's current layout.

## Storage and migration

Shipped examples live in:

```text
assets/tracks/examples
```

Personal drafts live in:

```text
$XDG_DATA_HOME/neon-racer/tracks
```

When `XDG_DATA_HOME` is unset, the fallback is:

```text
~/.local/share/neon-racer/tracks
```

`NEON_RACER_DATA_DIR` overrides the base application-data directory.

When the draft library opens, it imports legacy
`tracks/custom/*.draft` files from the old repository-local location. It does
not overwrite a user-data draft with the same name.

## Changing the format

When serialized layout meaning changes:

1. increment the layout version;
2. preserve old-reader behavior where practical;
3. add a prior-version fixture or migration test;
4. update this document and the playable-format compatibility statement;
5. keep enum migrations explicit;
6. verify malformed input leaves caller state unchanged.
