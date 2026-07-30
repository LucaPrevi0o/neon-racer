# Frozen playable time-trial package format

## Purpose

Saved playable time trials use a versioned, human-readable `.nrplay` format.
They are frozen race artifacts, not editable drafts.

One package contains:

- one race-ready layout snapshot;
- required export metadata;
- a durable layout fingerprint;
- one verified three-lap ghost for that exact layout.

The game never edits a loaded package in place. Re-exporting the same metadata
name creates a new complete snapshot and atomically replaces that filename's
current package.

## Package version 1 grammar

```text
NEON_RACER_PLAYABLE 1
STATUS PLAYABLE
NAME_BYTES <byte-count>
<name bytes>
CREATOR_BYTES <byte-count>
<creator bytes>
DESCRIPTION_BYTES <byte-count>
<description bytes>
EXPORT_VERSION <positive integer>
LAYOUT_FINGERPRINT <uint64>
LAYOUT <layout-version>
START <piece-id> <race-direction>
PIECES <count>
PIECE <same structural fields as the selected draft layout version>
...
VERIFICATION_GHOST 1 <sample-count> <duration-seconds>
GHOST_SAMPLE <time> <position xyz> <velocity xyz> <forward xyz> <up xyz>
             <heading-radians> <speed>
...
END_PLAYABLE
```

The current writer emits package version 1 with layout version 6.

## Metadata encoding

`NAME_BYTES`, `CREATOR_BYTES`, and `DESCRIPTION_BYTES` are followed by exactly
the declared number of bytes. This preserves spaces without relying on a
non-C++11 quoting convention.

Metadata must be:

- non-empty;
- printable;
- single-line text.

Ghost floats are written with round-trip-safe precision.

## Layout identity

`LAYOUT_FINGERPRINT` is a stable FNV-1a identity over persisted layout
semantics:

- piece order and persisted piece fields;
- selected start/finish piece ordinal;
- selected race direction.

It excludes generated piece IDs and runtime `LayoutRevision`, because neither
is a durable save/load identity.

For embedded layout versions 1 through 5, the reader first validates the
serialized fingerprint using the historical flat-Twist representation. It then
binds the in-memory package and ghost to the migrated runtime fingerprint. This
preserves old package verification while allowing version-6 runtime semantics
to distinguish legacy flat Twists from configurable corkscrews.

Verification state is derived from a valid bundled ghost and matching
fingerprint. It is never trusted as a separately serialized flag.

## Reader validation

The reader rejects:

- unsupported package or embedded layout versions;
- a layout that is not race-ready;
- incomplete or invalid metadata;
- a mismatched layout fingerprint;
- a missing, empty, or oversized ghost;
- zero, negative, non-finite, or excessive replay duration;
- non-monotonic sample timestamps;
- non-finite or out-of-envelope vehicle values;
- invalid vehicle orientation vectors;
- non-whitespace data after `END_PLAYABLE`;
- non-regular or oversized input files.

Resource limits:

| Resource | Limit |
| --- | --- |
| Package file | 64 MiB |
| Format label | 64 bytes |
| Ghost samples | 250,000 |
| Ghost duration | Two hours |
| Embedded layout pieces | 128 |
| Entry coordinates | `-1,000,000` through `1,000,000` |
| Elevation delta | `-1,000,000` through `1,000,000` |

Validation completes before the package becomes active. A failed load never
replaces the caller's current package, editor, or race state.

## Export lifecycle

`Tab` starts only an in-memory preview of a race-ready editor layout.

Persistent export becomes available after a completed verified three-lap run
for the unchanged frozen layout:

1. finish the run;
2. press `E`;
3. enter a name, creator, and description;
4. save the package.

The package stores the exact raced layout and its fastest verified ghost.

Re-exporting the same metadata name increments `EXPORT_VERSION` and atomically
replaces the package stored under that safe filename. This is latest-version
storage, not an archive of every historical export.

The exporter does not silently overwrite an unreadable, corrupt, oversized, or
special destination. If two display names sanitize to the same filename, the
second export is rejected unless it uses exactly the same metadata name.

## Storage and filenames

Personal packages live in:

```text
$XDG_DATA_HOME/neon-racer/playables
```

When `XDG_DATA_HOME` is unset:

```text
~/.local/share/neon-racer/playables
```

`NEON_RACER_DATA_DIR` overrides the base directory.

Export names are converted to safe filename stems containing only letters,
digits, `_`, and `-`; spaces become `_`. Files use the `.nrplay` suffix.

The library may also discover a valid externally copied `.nrplay` file with a
different filename. It preserves that exact path when launching the package.

Writes use a unique temporary sibling file, flush the complete package, and
rename it only after serialization succeeds.

## Compatibility

A package-version-1 reader accepts embedded layout versions 1 through 6 as long
as the corresponding structural codec remains supported.

When package structure or verification authority changes:

1. increment the package version;
2. keep embedded layout-version handling explicit;
3. add fixtures for the prior package and layout versions;
4. update this document and the draft-format compatibility table;
5. verify atomic replacement and failed-load state preservation.
