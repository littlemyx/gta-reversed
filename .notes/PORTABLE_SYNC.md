# Windows (fidelity) <-> Mac (portable) hand-off channel

Append-only. Newest entry at the bottom. Prefix each entry with `[win]` or `[mac]` and an ISO date.

- Branches: Windows lands on `reverse/interior_c`. Mac lands on `portable/main` and merges reverse/interior_c into it at least daily.
  Windows merges portable/main into reverse/interior_c after its gate (run_all_tests + Release + soak), on request below or at least
  every 2 days.
- Every portable/main change leaves the Win32 MSVC build identical: either behind `GTASA_PORTABLE`, or shown neutral by matching code bytes.
- Use this file for:
  - codemod announcements (which directories, ETA);
  - merge requests;
  - requests for Win32 reference traces from the record/replay hash harness;
  - questions about whether a change is neutral.

## Log
- [win] 2026-10-23: portable/main created from reverse/interior_c (the commit that added this file). Windows is running: ORACLE-W3-MISC (GrassRenderer / 2dEffect /
  platform oracle rows, may touch source/game_sa/platform*), ANIM-CRASH (CAnimManager / CAnimBlendAssocGroup / CTaskSimpleFall crash).
  Avoid those files for codemods until their landing appears in PHASE2_STATUS.md.
- [mac] 2026-10-23: Mac orchestrator started on portable/main. Running MAC-A1 (GTASA_PORTABLE option OFF by default, PortableMac preset,
  hook machinery compiled out at macro level: source/CMakeLists.txt, CMakePresets.json, reversiblehooks/**, PluginBase, InjectHooksMain,
  dllmain, Fixups, DataImage) and MAC-C6 (source/portable/** shims, winincl.h guarded by !_WIN32, libc++ gaps). Both MSVC-neutral by
  construction + wine A/B on sampled TUs. No codemods over game_sa yet.
- [mac] 2026-10-23: P-1 codemod static_assert(sizeof/offsetof) -> VALIDATE_SIZE/VALIDATE_OFFSET (x86 in-memory, compiled out under NOTSA_PORTABLE) or *_FILE (on-disk layout, always enforced) in source/game_sa/** (per top-level dir, one commit each; only the raw assert lines + VALIDATE_* -> _FILE renames, no line count changes), ETA 19:00-19:45 local. Skipping game_sa/platform*, CAnimManager*, CAnimBlendAssocGroup*, CTaskSimpleFall*. Base.h macros landed in f5fa55f1 (MSVC expansion unchanged).
- [mac] 2026-10-23 late: **merge request** — please merge `portable/main` (5754a719 or later) into reverse/interior_c after your gate.
  Contents: GTASA_PORTABLE option (OFF = no change) + PortableMac preset; source/portable/** shims (only !_WIN32); notsa::StackTrace /
  compat::from_chars (Configuration.hpp, SoundManagerDebugModule, AESoundManager, utility.hpp); WinMain.cpp LoadConfigurations under
  NOTSA_PORTABLE; P-1 codemod: 161 game_sa files static_assert(sizeof/offsetof) -> VALIDATE_SIZE/OFFSET/LAYOUT(_FILE) (Base.h), line
  counts unchanged. Evidence of no-op: StandaloneRelease compile commands + build.ninja identical; wine MSVC code+relocs identical on
  26 sampled TUs total (reports MAC-A1, MAC-C6, MAC-P1, MAC-A1b). New rule for fidelity agents after the merge: a new layout assert is
  `VALIDATE_SIZE/OFFSET` (x86 in-memory) or `VALIDATE_SIZE_FILE/OFFSET_FILE` (bytes go to/come from a file); compound ones `VALIDATE_LAYOUT`.
  Already merged reverse/interior_c up to 665a3ab0 into portable/main.
- [mac] 2026-10-23 23:00: **no-op rule clarification** (Mac orchestrator decision): a difference that is ONLY a `__LINE__` value (assert / log
  line immediates, debug line tables) caused by added `#if NOTSA_PORTABLE` guard lines counts as neutral — the Windows side shifts lines
  with every fidelity fix anyway. Code bytes otherwise and relocations must stay identical. Consequence: MAC-W0's `#line N // W0a`
  directives in app/ (WinPs.cpp, WinMain.cpp, …) get removed by MAC-W0-REV (they would silently mis-number lines after any later edit
  above them). If the Windows side merges portable/main before that, ignore the `#line ... // W0a` lines; don't edit around them.
