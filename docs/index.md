# Documentation map

This page defines where Neon Racer documentation belongs. When behavior changes,
update the canonical document listed here instead of repeating the same detail
in several files.

## Canonical documents

| Topic | Canonical document | Scope |
| --- | --- | --- |
| Build, launch, controls, repository overview | [`../README.md`](../README.md) | User and newcomer entry point |
| Module ownership and dependency rules | [`architecture.md`](architecture.md) | Current code architecture |
| Contributor workflow, testing, and releases | [`development.md`](development.md) | Engineering process |
| Editable draft serialization | [`track-format.md`](track-format.md) | Normative `.draft` specification |
| Frozen playable serialization | [`playable-track-format.md`](playable-track-format.md) | Normative `.nrplay` specification |

## Documentation rules

1. Keep the README operational and concise. It may summarize a topic, but should
   link to the canonical detailed document.
2. Treat the two format files as normative specifications. Code and tests must
   change with them.
3. Put code ownership and dependency decisions in `architecture.md`.
4. Put testing and release procedures in `development.md`, not in the README.
5. When a value is duplicated for usability, prefer a link and a short summary
   over copying a full list that can drift.

## Status language

Use these labels consistently:

- **Current**: implemented behavior expected to work now.
- **Required invariant**: a rule the implementation must preserve.
- **Planned**: an accepted future direction that is not yet implemented.
