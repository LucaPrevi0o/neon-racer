# Draft track format

Editable tracks use a whitespace-delimited text format. They are drafts, not
playable exports: they may be incomplete and never contain a ghost recording.

```text
NEON_RACER_DRAFT 5
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

## Compatibility

The writer emits version 5. The loader accepts versions 1 through 5:

- v1: basic pieces and start/finish;
- v2: explicit `STATUS DRAFT` record;
- v3: endpoint width, elevation, lateral offset, and surface material;
- v4: curve extent (90°, 180°, or 270°);
- v5: curve bank angle (−45° through 45°).

When extending the format, increment the version, retain old-reader behavior
where practical, add a fixture or unit test for the prior version, and update
this document.

## Storage

Shipped examples live in `assets/tracks/examples`. Personal drafts live under
`$XDG_DATA_HOME/neon-racer/tracks`, falling back to
`~/.local/share/neon-racer/tracks`. `NEON_RACER_DATA_DIR` overrides the base
directory for development and portable installs.
