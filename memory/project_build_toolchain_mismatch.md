---
name: project-build-toolchain-mismatch
description: Bash tool's PATH resolves cmake/gcc/g++/ninja/ctest to MSYS2 mingw64 by default in this project, not the ucrt64 toolchain the project requires.
metadata:
  type: project
---

When building/testing AIS-pipeline from the Bash tool, `cmake`/`gcc`/`g++`/`ninja`/`ctest` resolve to `/c/msys64/mingw64/bin/...` by default, because mingw64's bin dir appears earlier on `PATH` than ucrt64's. The project (`CLAUDE.md`) requires the **ucrt64** toolchain — that's where GTest and other pacman deps (`mingw-w64-ucrt-x86_64-*`) are actually installed.

**Why it matters:** running a bare `cmake .`/`cmake --build` in an existing `build/debug` (configured for ucrt64) silently triggers a cache reconfigure onto the mingw64 compiler, which can then fail to find ucrt64-only packages or produce a build inconsistent with the user's normal MSYS2 UCRT64 shell workflow.

**How to apply:** When building/running tests via Bash tool, explicitly use the ucrt64 binaries and put ucrt64 first on PATH for the command:
```
export PATH="/c/msys64/ucrt64/bin:$PATH"
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/c/msys64/ucrt64/bin/g++.exe ...
```
`pacman` itself is not on the Bash tool's PATH at all (checking installed packages requires reading `/c/msys64/ucrt64/include/...` or asking the user, not `pacman -Qs`).

See [[ais-pipeline-fase2-pipeline-design]] for the broader Fase 2 concurrency work this came up during.
