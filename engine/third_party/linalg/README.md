# linalg.h (vendored)

Single-header linear algebra library used for the basic vector, matrix, and
quaternion arithmetic of the engine.

| | |
| --- | --- |
| Upstream | <https://github.com/sgorsten/linalg> |
| Version | 2.2 |
| Commit | `4460f1f5b85ccc81ffcf49aa450d454db58ca90e` (2023-07-02) |
| SHA-256 | `2cd3560063fa3069bd791983cb53db0961f661d69895a4aa4c01677675f9aa21` |
| License | Unlicense (public domain), see `LICENSE` |

Vendored rather than fetched as a Meson subproject because it is a single
header with no build step; this keeps CI free of another network dependency.

Engine code never includes this header directly. `engine/src/math` wraps it and
is the only place allowed to `#include <linalg.h>`, so the conventions recorded
in `docs/tasks/03-math-and-graphics-foundation.md` have exactly one home.

To update: replace `linalg.h`, refresh the version, commit, and checksum above,
and run the native test suite — `tests/math` pins the conventions this engine
relies on.
