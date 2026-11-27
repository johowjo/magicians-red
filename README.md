# Adaptive GPU Points-to Analysis

The first implementation is a deterministic CPU reference solver for the
versioned constraint format in [docs/semantics.md](docs/semantics.md).

## Build and run

Enter the reproducible development shell first:

```sh
nix develop
```

Then, from that shell:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
build/pta_ref workload.pta
```

To capture a compilation database with Bear, run configuration and compilation
inside the same Bear invocation. This lets Bear's macOS wrapper mode see which
compiler CMake selects:

```sh
mkdir -p build-bear
bear --output compile_commands.json -- bash -c \
  'cmake -S . -B build-bear -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-bear --clean-first'
```

The flake pins nixpkgs in `flake.lock` and provides CMake, Ninja, Clang, and
`clang-format`. On macOS, the dev shell also supplies Nixpkgs' pinned Apple SDK;
CMake selects the Nix Clang wrapper and uses that SDK.

The solver prints one `pt VARIABLE OBJECT...` line per variable. An empty
points-to set prints only its variable ID. It validates its result with an
independent fixed-point check before printing.

## Current status

The CPU reference and semantic schema are implemented. HIP is not available in
the development environment recorded in [docs/environment.md](docs/environment.md),
so the fixed GPU baseline has not started. Per the development gate, adaptive
representations and scheduling remain downstream of CPU/GPU agreement.
