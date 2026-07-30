# Playable time-trial package format

Saved playable time trials use a versioned, human-readable `.nrplay` format.
They are frozen race artifacts, distinct from editable `.draft` files: one
package contains one race-ready layout snapshot, its export metadata, and one
verified three-lap ghost for that exact layout. The game never edits a package
in place. Re-exporting the same name writes a new snapshot and atomically
replaces that file's current version.

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
PIECE <same structural fields as a v6 draft>
...
VERIFICATION_GHOST 1 <sample-count> <duration-seconds>
GHOST_SAMPLE <time> <position xyz> <velocity xyz> <forward xyz> <up xyz>
             <heading-radians> <speed>
...
END_PLAYABLE
```

The byte-count fields preserve spaces in metadata without needing a
non-C++11 quoting convention. Metadata must be non-empty, printable,
single-line text. Writers use round-trip-safe float precision for ghost
samples.

## Validation and compatibility

The current writer emits package version 1 and layout version 6. A v1 package
reader accepts supported embedded layout versions 1 through 6, so an older
layout schema remains readable as long as its structural codec is supported.

The loader rejects unsupported versions, non-race-ready layouts, incomplete
metadata, zero or non-finite replay duration, empty or oversized replay data,
non-monotonic timestamps, invalid vehicle orientations, a mismatched layout
fingerprint, or non-whitespace data after `END_PLAYABLE`. A failed read never
replaces the caller's current package.

For resource safety, a package must be a regular file no larger than 64 MiB;
format labels are limited to 64 bytes. A ghost may contain at most 250,000
samples, last at most two hours, and use finite gameplay values within the
bounded replay envelope. Those checks happen before a package becomes active,
so malformed files cannot feed non-finite transforms to rendering.

Embedded layouts use the same safety envelope as drafts: entry coordinates and
elevation deltas must be between `-1,000,000` and `1,000,000`, and a persisted
layout has at most 128 pieces. The codec rejects violations before evaluating
road geometry.

`LAYOUT_FINGERPRINT` is a stable FNV-1a identity over persisted layout
semantics: piece order and fields, selected start/finish piece ordinal, and
race direction. It deliberately excludes generated piece IDs and the runtime
`LayoutRevision`, because neither survives a save/load cycle as a durable
identity. The verification state itself is derived on load from a valid bundled
ghost and matching fingerprint; it is not serialized as authority.

## Export policy

`Tab` creates only an in-memory preview, allowing a new race-ready draft to be
driven for the first time. A persistent export is enabled only after a completed
verified three-lap run for the unchanged frozen layout. Press `E` in the finished
time trial, enter a name, creator, and description, then save. Re-exporting the
same safe filename increments `EXPORT_VERSION` and atomically replaces that
filename's current package; this is latest-version storage, not a historical
archive. An unreadable, corrupt, oversized, or special existing path is never
silently overwritten. If two display names sanitize to the same filename, the
second export is rejected unless it is exactly the same metadata name.

## Storage

Personal packages live under `$XDG_DATA_HOME/neon-racer/playables`, falling
back to `~/.local/share/neon-racer/playables`. `NEON_RACER_DATA_DIR` overrides
the base directory. Names are converted to safe filename stems containing only
letters, digits, `_`, and `-` (spaces become `_`) and use the `.nrplay` suffix.
The library can also load a regular externally copied `.nrplay` file with
another filename and preserves that exact path when launching it. Writes use a
unique temporary sibling file, flush it, and rename it only after the complete
package is written.
