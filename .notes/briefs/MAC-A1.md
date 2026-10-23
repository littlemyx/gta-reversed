# MAC-A1 — `GTASA_PORTABLE` option, `PortableMac` preset, hook machinery compiled out (slices A-1 + C-0)

Host: the Apple Silicon Mac. Rules: `.notes/agent_rules/v4.md` (Windows-only parts — exe-oracle, gpu.lock, C:\ paths, soak — do NOT
apply here). Repo /Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (shared, pushed to remote `fork`).
Work in the main checkout; a parallel agent MAC-C6 works in the same checkout (zones below). Wall-clock cap 100 min.

## Context
- Goal: the same game built natively for macOS arm64 (`.notes/PORTABILITY_GOAL.md`). The Win32 x86 MSVC build (`StandaloneRelease`)
  is the fidelity reference and must stay bit-for-bit identical (**no-op rule**).
- Read: `.notes/STREAM_C_PLAN.md` sections 1, 2, 4, 5 "A", 7, 8; `.notes/PORTABILITY_INVENTORY.md` line about slices A.
- Native census already exists: `tools/standalone/clang_census.py run --mode native` (follows build/StandaloneRelease/compile_commands.json,
  22–40 min with -j 8) + `clang_census_report.py`. Last result: 0/1,115 TUs clean, 4,340 unique root diagnostics; stream A = 157 in 9 files
  (standalone/Fixups.cpp 90, standalone/DataImage.cpp 41, reversiblehooks/HooksUtility.hpp 13, RHManager.cpp 4, StaticOneWayHook.cpp 3,
  VMTInfo.cpp 2, PluginBase.cpp 1). The native census uses the Windows run build's -D set, so W/R overstate.

## Task
1. **CMake option** `GTASA_PORTABLE` (`option(${RE_PROJECT}_PORTABLE ...)` OFF, in source/CMakeLists.txt next to the others). When ON:
   define `NOTSA_PORTABLE=1`; do not build the hook registry / ReversibleHooks UI / InjectHooksMain registration / exe-oracle / DataImage
   / Fixups / dllmain sources (exclude from the target or `#if` them out — your call, keep it minimal and documented); `RH_Scoped*`,
   `RH_ScopedInstall`, `plugin::Call*`, `StaticRef` usage must compile to nothing / to the port's own symbols without touching the 896
   hook-registration files (macro level in the hook headers). With OFF every compile command and every compiled byte stays as today.
2. **Preset `PortableMac`** in CMakePresets.json: native arm64, Apple clang or brew llvm (`/opt/homebrew/opt/llvm`; record which and why),
   `GTASA_PORTABLE=ON`, Ninja, `build/PortableMac`. Configure must succeed on this Mac (conan deps: if the existing conan generators are
   Windows-only, find the minimal route — e.g. a mac conan profile or system/brew packages — and document it; do not rewrite
   ConanPresets.json used by the Windows presets). Building it fully is NOT required in this slice; producing `compile_commands.json` is.
3. **C-0 census in portable mode**: teach clang_census.py to take the TU list + -D set from `build/PortableMac/compile_commands.json`
   (flag, e.g. `--compdb`), run `--mode native` on it, and write the per-directory table (TUs clean / unique root diagnostics by stream
   and by top-level directory) to `.notes/reports/MAC-A1-census.md`. Acceptance for A: stream A diagnostics = 0.
4. **No-op proof**: (a) configure StandaloneRelease-equivalent compile commands before/after (diff the -D/flag set of compile_commands —
   if StandaloneRelease cannot be configured on the Mac, diff `build/StandaloneRelease/compile_commands.json`-relevant CMake logic by
   reasoning and say so); (b) for every shared header you change, compile 5 sampled TUs that include it with MSVC under wine before/after
   (method: STREAM_C_PLAN section 1 bullet 3; wine/MSVC recipe in memory note gta-reversed-tooling-mac: Wine at
   "~/tools/Wine Staging.app/Contents/Resources/wine/bin", WINEPREFIX=~/.wine-msvc, MSVC ~/tools/msvc) and compare code bytes. Put the table
   in the report.

## Zones
- Yours: source/CMakeLists.txt, CMakeLists.txt, CMakePresets.json, source/reversiblehooks/**, source/PluginBase.*, source/InjectHooksMain.*,
  source/dllmain.*, source/app/standalone/Fixups.cpp, DataImage.*, tests/standalone CMake glue only, tools/standalone/clang_census*.py,
  new cmake/ or tools/portable/ files.
- NOT yours: source/portable/**, source/game_sa/**/winincl.h and CRT/Windows-type shims, any `std::views::enumerate` / libc++ gap code
  (agent MAC-C6). Windows host files in flight: source/game_sa/platform*, CAnimManager, CAnimBlendAssocGroup, CTaskSimpleFall — don't touch.
- Never touch others' uncommitted changes; never push to reverse/interior_c.

## Rules
- Builds/census in the background with low priority (`taskpolicy -b nice -n 15`), wrap in `timeout`. ≤ 1 own sub-agent at a time.
- Commit often (`git commit -- <your files>`, message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`),
  `git push fork portable/main` after each commit (pull --no-rebase first if rejected; never force).
- No weakening of checks for green; anything not provably MSVC-neutral and not behind GTASA_PORTABLE: list it, don't land it.

## Report
Full report `.notes/reports/MAC-A1.md` (what was compiled out and how, preset/toolchain/conan route, census table, no-op table, open
items for C-6/C-7/P-1/W). Return ≤ 15 lines: commits, census numbers before/after, no-op result, open items.
