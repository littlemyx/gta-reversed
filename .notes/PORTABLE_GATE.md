# Portable gate for the Windows host (from the Mac orchestrator, 2026-10-27)

Why: `portable/main` builds the same sources for macOS arm64, Linux x64 and Win x64. New code on `reverse/interior_c`
compiles fine in the Win32 x86 MSVC gate but can still break the portable builds; today the Mac finds that only after the
next merge (example: the REPLAY-DIFF hooks in General.cpp/Lockstep.cpp broke PortableMac, fixed in MAC-MERGECHK).
Goal: catch it on your side, before landing, so a merge into portable/main needs no follow-up edits.

## 1. Add the portable build to the gate (x64 Native Tools shell, VS 2022)

```
bash tools/portable/win64/conan_install_win64.sh Release        # once; cached in ~/.conan2 (CONAN_EXTRA_ARGS="-s compiler.version=194" for VS 2022)
cmake --preset PortableWin64 -DGTASA_PYTHON=python              # once
cmake --build build/PortableWin64 -- -k 0                       # every gate run; incremental
```
Gate rule: 0 compile errors and the link succeeds (build/PortableWin64/bin/gta_reversed.exe). Running it is not required.
This configuration compiles with `NOTSA_PORTABLE` (SDL3 + librw GL3, x64/LLP64), so it catches Win32-only code outside
guards, 32-bit pointer assumptions and inline asm. Details: tools/portable/win64/README.txt, .notes/reports/MAC-WIN64.md.
The x86 StandaloneRelease gate stays as it is.

## 2. Coding rules for new code (please add to the next agent_rules version)

| new code does | write it as |
|---|---|
| Win32 API, the exe-oracle / REPLAY / Lockstep / standalone-run tooling, D3D9, DirectSound | inside `#if !defined(NOTSA_PORTABLE)` (or the existing `NOTSA_STANDALONE_RUN && !NOTSA_PORTABLE`), each guard on its own lines |
| x87 math the exe does at PC24 (temporaries, casts, literals that were `double`) | `x87d` instead of `double`; `X87R(expr)` around an op with a genuine 53-bit operand; `X87W(expr)` when a math overload must stay double (`log10`, `pow`, ...). No extra parentheses inside such chains (they changed MSVC codegen) |
| float → int conversion | `NOTSA_FTOL(T, expr)` for `(T)(expr)`; `NOTSA_FTOL_SC` for `static_cast<T>(expr)`; `NOTSA_FTOL_FN` for `T(expr)`; `NOTSA_FTOL_P` for `(T)x` without parentheses — keep the original spelling's variant, Win32 expands token for token |
| size / offset asserts | `VALIDATE_SIZE` / `VALIDATE_OFFSET` / `VALIDATE_LAYOUT` = x86 in-memory layout; `VALIDATE_SIZE_FILE` / `VALIDATE_OFFSET_FILE` when the bytes go to or come from a file |
| raw struct file I/O | through the struct's `notsa::FileRecord<T>` if it has one (source/game_sa/FileRecords/); never `sizeof` of a struct with pointers as a file size; save prefixes as `uint32` |
| a pointer stored in a 32-bit field / used as an id | `NOTSA_PTR_ID32(p)`; never cast a pointer to `int32`/`uint32` |
| copies with literal sizes, offsets into scratch buffers (`PC_Scratch`, `gpTempBuffer`) | `sizeof` of the real type, never an x86 constant; static_assert the layout |
| falling off the end of a non-void function | `NOTSA_UNREACHABLE_RET(value, ...)` |

## 3. Cheap pre-commit check (any shell, no build needed)

```
git diff --cached -U0 -- source | grep -nE '^\+' | grep -nE '\bdouble\b|\((u?int(8|16|32|64)|short|long|size_t)\)\s*\(?[a-zA-Z_][^;]*\b(f[A-Z]|m_f|fl|[0-9]\.[0-9]*f)\b|\b(GetTickCount|QueryPerformance|CreateFile|Sleep|HANDLE|HWND|DWORD WINAPI)\b'
```
Every hit needs one of the spellings from section 2 (or a guard). It is a heuristic; the authoritative scanners
(`tools/portable/pc24_scan.py`, `tools/portable/ftol_scan.py`) need the clang AST of the Mac build and keep running on the Mac
after each merge.

## 4. Merge cadence

- Mac merges `reverse/interior_c` into `portable/main` daily (+ build/ctest/scan check).
- Please keep merging `portable/main` into `reverse/interior_c` after your gate (last time: 40f9335f -> 136deaaf, 2026-10-24).
- Nothing here changes the Win32 x86 build: all portable code is behind `NOTSA_PORTABLE` or token-identical on MSVC.
