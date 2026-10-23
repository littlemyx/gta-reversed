# MAC-C6 — Windows scalar types + CRT-name shim, libc++ gaps (slices C-6 + C-4)

Host: the Apple Silicon Mac. Rules: `.notes/agent_rules/v4.md` (Windows-only parts — exe-oracle, gpu.lock, C:\ paths, soak — do NOT
apply here). Repo /Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (shared, pushed to remote `fork`).
Work in the main checkout; a parallel agent MAC-A1 works in the same checkout (zones below). Wall-clock cap 100 min.

## Context
- Goal: the same game built natively for macOS arm64 (`.notes/PORTABILITY_GOAL.md`). The Win32 x86 MSVC build is the fidelity reference
  and must stay bit-for-bit identical (**no-op rule**: everything new is under `#ifndef _WIN32` / `NOTSA_PORTABLE`, or proven neutral).
- Read: `.notes/STREAM_C_PLAN.md` sections 1, 2, 4, 6 (item 5), 7 rows C-4 and C-6, 8.
- Evidence: native census (tools/standalone/clang_census.py run --mode native; report via clang_census_report.py; 22–40 min, -j 8):
  libc++ gaps = 128 diagnostics in 74 files (`std::views::enumerate` 129 uses, `std::ranges::shift_right` 2, `<stacktrace>` in
  SoundManagerDebugModule.hpp, `std::from_chars(double)` in Configuration.hpp); CRT names + Windows scalar types ≈ 307 + 60 diagnostics in
  ~100 files, 5 names = 276 of them.

## Task
1. **C-6**: `source/portable/wintypes.h` (DWORD, BOOL, HANDLE, BYTE/`byte`, WORD, UINT, LONG as fixed-width — note LONG/DWORD are 32-bit
  on Win64 too, never `long`), `source/portable/crt.h` (`_stricmp`, `_strnicmp`, `strcpy_s`/`strcat_s`/`sprintf_s`/`sscanf_s`/`fopen_s`
  with the CRT's destination-size semantics, `_strlwr`/`_strupr`, `_getcwd`/`_chdir`, `MAX_PATH`, `Interlocked*` over std::atomic, …
  driven by the census list, not guesswork). Included from the existing Windows-include header (find it: winincl.h) only when `!_WIN32`.
  Do not paper over Win32 *API* calls (CreateFile, registry, DirectX …): those are stream W and get compiled out / reimplemented later.
2. **C-4**: libc++ gaps. First check whether brew llvm's libc++ (/opt/homebrew/opt/llvm, clang 23) already has `views::enumerate`,
  `ranges::shift_right`, `<stacktrace>`, `from_chars(double)`; if it does, record that and recommend that toolchain for the PortableMac
  preset (MAC-A1 owns the preset: write the recommendation in your report, don't edit CMakePresets.json). Otherwise provide
  `notsa::views::enumerate` etc. selected by `__cpp_lib_*` so MSVC keeps `std::`. Prefer a solution that does not touch the 74 user files
  (Windows fidelity agents edit those daily → merge conflicts); if a codemod is unavoidable, do it in one commit per directory and say so.
3. **Measure**: re-run the native census restricted to the affected files (or full) and report category counts before/after:
  "C: libc++" → 0 and "W: Win32 type/CRT" outside the platform files → 0 is the acceptance.
4. **No-op proof**: for each shared header you edit that MSVC sees (winincl.h etc.), compile 5 sampled TUs with MSVC under wine before/after
  and compare code bytes (method: STREAM_C_PLAN section 1 bullet 3; wine/MSVC recipe: memory note gta-reversed-tooling-mac — Wine at
  "~/tools/Wine Staging.app/Contents/Resources/wine/bin", WINEPREFIX=~/.wine-msvc, MSVC ~/tools/msvc). If the edit is entirely inside
  `#ifndef _WIN32` / new files MSVC never includes, say so instead.

## Zones
- Yours: source/portable/** (new), the Windows-include header(s) (winincl.h and the place it includes from), SoundManagerDebugModule.hpp
  (stacktrace guard), Configuration.hpp (from_chars fallback), any new notsa ranges header.
- NOT yours: CMakeLists.txt files, CMakePresets.json, tools/standalone/clang_census*.py, source/reversiblehooks/**, PluginBase,
  InjectHooksMain, dllmain, app/standalone/Fixups.cpp, DataImage (agent MAC-A1). Windows host files in flight: source/game_sa/platform*,
  CAnimManager, CAnimBlendAssocGroup, CTaskSimpleFall — don't touch.
- Never touch others' uncommitted changes; never push to reverse/interior_c.

## Rules
- Census/builds in the background with low priority (`taskpolicy -b nice -n 15`), wrap in `timeout`. ≤ 1 own sub-agent at a time.
- Commit often (`git commit -- <your files>`, message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`),
  `git push fork portable/main` after each commit (pull --no-rebase first if rejected; never force).
- No weakening of checks for green.

## Report
Full report `.notes/reports/MAC-C6.md` (header list, toolchain finding, census before/after, no-op table, open items for W/C-7).
Return ≤ 15 lines: commits, numbers, no-op result, toolchain recommendation, open items.
