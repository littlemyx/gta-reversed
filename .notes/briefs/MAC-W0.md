# MAC-W0 — platform layer for the compile gate: existing SDL3 / OpenAL paths, Win32-only code out of the portable build

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Main checkout. Agent MAC-P2-DESIGN runs in
parallel read-only (it only writes .notes/P2_PLAN.md). `.notes/` is gitignored (`git add -f`). Wall-clock cap 100 min.

## Context
- Portable build: preset `PortableMac` (brew llvm 23, `GTASA_PORTABLE=ON` -> `NOTSA_PORTABLE`, librw RW_GL3 + SDL3). After a compiler
  change delete build/PortableMac/CMakeCache.txt + CMakeFiles (keep generators/). Census command and numbers: `.notes/reports/MAC-A1b.md`,
  `.notes/reports/MAC-A1b-census.md` + `-classified.tsv`. Now: 1105 TUs, 845 clean, unique 3033; **W 915** (Audio 527, app 269, rest
  elsewhere), R 1315, F 208, P 159, unclassified 421.
- The upstream tree already has CMake options `GTASA_USE_SDL3` and `GTASA_WITH_OPENAL` (source/CMakeLists.txt:8-9) and code paths behind
  them (grep `NOTSA_USE_SDL3`, `NOTSA_WITH_OPENAL` or whatever the defines are). Find out how complete they are.
- Plan (PORTABILITY_INVENTORY stream W): W-1 window/main loop, W-2 input on SDL3, W-3/W-4 audio on OpenAL, W-6 threads/CdStream,
  W-7 file paths, W-8 misc. This slice is for the COMPILE GATE, not for running.

## Task
1. Inventory (short table in the report): what the SDL3 and OpenAL paths already cover; which W diagnostics they remove if enabled in
   PortableMac.
2. Enable them in the portable build where they exist (CMake under GTASA_PORTABLE; conan/brew deps: SDL3 and OpenAL — on macOS prefer
   openal-soft from conan or brew; document).
3. Win32-only translation units/blocks with an existing SDL3/OpenAL replacement: exclude/`#if` them out under NOTSA_PORTABLE.
   Where no replacement exists (DirectSound/EAX hardware, CdStream on Win32 overlapped I/O, threads, registry, DirectInput, Bink):
   provide the smallest portable implementation that is **behaviourally honest** — std::thread / std::mutex / std::filesystem / POSIX
   where it is a straight equivalent; otherwise a stub that logs and returns failure, listed in the report as a W-n item with file:line.
   Never fake success silently.
4. Re-run the full portable census; table W (and total) before/after by directory.
5. No-op proof: every edit is under NOTSA_PORTABLE/!_WIN32 or in portable-only files; StandaloneRelease compile commands + build.ninja
   identical; wine MSVC A/B (code+relocs) on 5 TUs you touched (method/script: .notes/reports/MAC-A1b.md, MAC-A1.md).

## Zones
Yours: source/app/** (incl. platform), source/oswrapper/**, source/game_sa/Audio/Hardware/**, source/game_sa/Core/CdStream*/FileMgr*
(platform parts only), source/portable/**, CMakeLists.txt files, CMakePresets.json, conan/profiles/macos-arm64.txt, tools/portable/**.
NOT yours: fakerw/**, standalone/rw/** (stream R, later), x87/SIMD headers (stream F), anything outside the list. Hot on the Windows host:
Renderer.cpp, Entity.h, CAnimManager/CTaskSimpleFall — don't touch. Function bodies of game logic: no edits beyond `#if` guards.
Commit only your files (`git commit -- <files>`, trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), push after each
commit (pull --no-rebase on reject; never force; never push reverse/interior_c). Builds/census background, low priority, `timeout`.
≤ 1 own sub-agent.

## Report
`.notes/reports/MAC-W0.md` (inventory, deps route, stubs list as W-n items, census table, no-op table). Return ≤ 15 lines.
