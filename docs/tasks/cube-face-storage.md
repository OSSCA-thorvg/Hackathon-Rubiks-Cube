# Cube face storage

## Status

Implemented on 2026-10-04 after the cleanup at `267de6c`; final verification is recorded below.

## Domain boundary

`Cube` replaces `CubeState` and owns exactly six outward-facing row-major grids.
`CubeDraft` owns an independent instance of the same internal `FaceStorage`.
The storage holds a runtime size and `6N²` colours, and does not promise physical
reachability. `Cube` does: its raw-storage constructor is private, its public
constructor creates a solved cube, and its only public mutation is rotation.
Only successful assembly can turn edited colours into a `Cube`. Solvers continue
to receive `const Cube&`, without an invalid-state flag or rejection path.

`Cube.hpp` keeps the small storage and borrowed view types together. `Face.hpp`
contains the face/colour vocabulary formerly mixed with the removed `Cubie` type.
`CubeDraft` contains editing and validation policy, and `Assembly` remains the
physical validator. `MoveTimeline` remains separate from sticker storage because
history is a session concern.

## Coordinates and iteration

A native cell is `(face, row, column)` in the unfolded net: rows down, columns
right. The one face basis in `Surface` defines both the mapping to integer XYZ
coordinates and its inverse. The net delegates coordinate queries to this basis;
resting faces and painted frames read the native grids directly.

`face(id)` returns a read-only `FaceView`, `row(row)` returns a `ColorView`, and
`flat()` exposes the contiguous colours without copying. `CubeView` gives graphics
the same queries for a valid cube or an unfinished draft. These views borrow
memory: rotation and colour edits preserve it, but owner destruction, moving or
assignment require reacquiring the view. Frame inputs own their painting snapshot.

`surface_cells(size)` lazily visits the exposed stickers. `layer_cells(axis,
layers,size)` lazily visits only selected side strips and outer caps; a selected
layer contributes `4N` side stickers and a selected outer end adds `N²` cap
stickers. Rotation derives destinations from `turned_sticker`, with no hand-written
strip table and no scan of interior cubies. Native source/destination indices are
prepared once per size, axis and layer, then reused. A thread-local colour workspace
keeps reads separate from writes and preserves the owner's buffer addresses.

`is_solved()` means that each of the six faces is uniform, including a completed
cube held in any rigid orientation. Equality compares visible colours in the same
frame, so an assembled cube equals the cube that was painted, including after
identical later moves.

## Painting and solver frames

An edit changes a sticker and maintained colour counts. Full validation runs on
request and is cached until another edit. Apply validates once, normalizes the
accepted painting, and builds an owned `Cube`; invalid edits cannot reach solvers.
The last refusal's highlights remain visible while the user corrects it.

The external painting/share byte order and blamed indices are unchanged. Conversion
happens at those boundaries; native painting frames do not round-trip through the
share format. The C ABI remains unchanged.

Odd cubes can move their six fixed centres by turning a middle layer. Public solvers
normalize their private copy using those centres and conjugate returned moves back
to the caller's axes, layer positions and turn directions. Already completed cubes
return an empty solution in every orientation. The fixed-colour target remains an
internal algorithm contract. Solver size support follows the layer mask, while the
application/share format keeps its existing limit of 28.

Reduction enumerates face interiors and borders rather than volume coordinates,
uses the shared sticker identity and a `6N²` inverse index, and preserves its former
piece ordering. Its prepared boards cache native colour indices so scoring reads
the flattened face storage directly. The three-by-three search scratch state
stores 54 surface colours.
Graphics visits exposed faces; the body sheets revealed during animation are
separate geometry, generated only at moving/still interfaces.

## Retained experiment

`16.6-clusterwise-centres.patch` is retained and rebased against the new `Cube`
and surface-coordinate representation. It is not applied to production. Both
`git apply --check` and C++17 syntax checks of the patched temporary source pass.

## Verification

The tests first froze the former XYZ/cubie rotation independently at `267de6c`.
The differential checks passed against that old implementation before being run
against the new storage. New storage tests cover sizes 2, 3, 4, 5, 7, 28 and 31,
each axis, inner/wide/whole/separated layers, negative and modulo-four turns,
inverse sequences, and every visible colour after each move. Native/legacy ordering,
borrowed views, copy ownership, semantic equality and layer-range membership are
checked independently.

Draft tests cover impossible paintings, lazy validation, incremental counts,
independent copies and orientation normalization. Solver tests cover all 24 held
frames and middle-layer states, applying returned moves to the original cube.
Graphics tests cover native painting snapshots, unchanged diagnostic positions and
animated body interfaces on every axis.

Final verification passed:

- `meson compile -C build/native` and the wasm build with Emscripten 6.0.0.
- `meson test -C build/native --no-rebuild --print-errorlogs`: 30 suites,
  458 cases. The solver suite completed in 48.38 seconds while the large cases
  ran separately.
- Large solver checks: the original `solver-test '[.big]' --reporter compact`
  run passed 3 cases and 24,700 assertions. The subsequently added native 31×31
  boundary case passed separately with 12,738 assertions, including a middle-layer
  change to its fixed-centre frame.
- After adding the direct completed-frame guard check,
  `solver-test '[solver-frame]' --reporter compact`: 3 cases, 16,049 assertions.
- Web unit tests: 369 tests in 20 files, including the unchanged C ABI contract.
- Production web build and browser e2e: 77 tests. Chromium required execution
  outside the local macOS sandbox because its Mach port registration was blocked.
- Four representative native render dumps (cube, net, combined view and glint)
  were byte-identical to the pre-refactor captures.
- The retained experiment passes `git apply --check` and an applied-source
  C++17 `-Wall -Wextra -Werror -fsyntax-only` check. `git diff --check` is clean.

The generated wasm is 506,480 bytes (previously 487,316); JS glue remains 80,871
bytes. Per-cube colour storage drops from `6N³` to `6N²`; rotation index tables
are shared within each thread and retained for the sizes used by that thread.

## Local performance check

The old commit and the final implementation were compiled separately with Apple
Clang C++17, `-O2 -g`, and assertions enabled. A standalone driver used the same
seeded inputs, alternated execution order, and applied every returned solution
outside the measured interval. Move counts and visible-state hashes matched.

| Workload | `267de6c` | Face storage |
|---|---:|---:|
| Warm 5×5 reduction, mean of six solves | 584.47 ms | 409.04 ms |
| 3×3 layer-by-layer, time per ten of 100 solves | 186.71 ms | 28.05 ms |
| Search scratch, 500,000 face turns | 224.23 ms | 22.89 ms |

First-use 5×5 reduction preparation remained comparable: 163.37–163.93 ms before,
141.31–167.51 ms after. These are local native measurements for those workloads,
not a browser performance guarantee. The temporary driver and raw runs are in
`/tmp/rubiks-core-performance/` and are not a permanent benchmark suite.
