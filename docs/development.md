# Development guide

## Build and test

Use either supported build entrypoint from the repository root:

```sh
make build
make test
```

or:

```sh
cmake -S . -B build/cmake
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

The domain tests must not include or link Raylib. Add behavior tests under
`tests/unit` whenever changing track geometry, validation, serialization, or
playable-export rules.

## Dependency rules

`src/track` and `src/persistence` are the domain layer and must remain free of
Raylib types. `src/editor`, `src/race`, `src/render`, and `src/ui` may use
Raylib, but rendering and input adapters must not define track or race rules.
`src/app` is the composition root.

Before adding a new source file, add it explicitly to both the Makefile and the
appropriate CMake target. This is intentional: a missing module should fail a
review visibly rather than being silently discovered by a wildcard.

## Git workflow

`main` represents integrated, verified releases. Create a focused branch for a
coherent change, for example `refactor/editor-commands` or
`feature/checkpoints`. Keep every commit buildable and run the relevant tests
before committing. Tag release candidates and releases with semantic versions,
for example `v0.2.0-alpha.0`.

Do not commit generated executables, build directories, or editable user drafts.
Versioned example tracks belong in `assets/tracks/examples`.
