# INFRA-FAST report

Agent INFRA-FAST (rules v2), branch reverse/interior_c, worktree C:/gta-wt/infra-fast (removed at the end).

## Result

| | |
|---|---|
| 1. `NOTSA_STANDALONE_NOVSYNC=1` | boot to the menu **1.5 / 2.1 s** (two runs) vs **28.8 / 28.9 s** with vsync (windowed 960x600, silent, videos skipped, Release): **~15x** |
| 2. compile cache (sccache, opt-in) | works, **but does not pay on this PC and is not byte-equivalent**: default PCH build of a fresh worktree **1.5 min**, cached cold **36.4 min**, cached warm (other worktree, 1181/1181 hits) **4.3 min**; recommendation: keep it off |
| 3. `tools/standalone/gpu_locked.sh` | dead-holder detection (same shell + cross shell), USER-PLAY never removed, ownerless / hand-written leftovers handled; soak.sh and fps_run.sh switched |
| gates | run_all_tests **45/45** ok (build 3.3 min, run 21.7 min); soak without the switch: RESULT OK (7 min route, 22 markers, clean atexit, Windows host, runs under gpu_locked.sh); object code of the default Release build identical before/after the header edits (1119 objects) |

## 1. NOTSA_STANDALONE_NOVSYNC

* `RwCameraShowRaster` (source/standalone/rw/camera.cpp, inside the existing `NOTSA_STANDALONE_RUN` block) clears `rwRASTERFLIPWAITVSYNC` from the flags when the env var is set (`0/off/no/false/empty` = off).
  librw then presents with `D3DPRESENT_INTERVAL_IMMEDIATE` (its device starts that way and `showRaster` only changes the interval when the flag differs). The game asks for the flag in `psCameraShowRaster` when the
  menu option `m_bPrefsFrameLimiter` is on (default) or while `CLoadingScreen` is active - that is where the ~78 ms/frame Present of the dev PC came from.
* **Not affected**: all game logic and timing - `CTimer`, the software frame limiter (`WinMain.cpp` GAME_STATE_IDLE: one frame per 1000/`RsGlobal.frameLimit` ms while `m_bPrefsFrameLimiter` is on), the saved
  settings, `ResetDeviceNow` (it flips the interval itself and asks `showRaster` for the original one). **Affected**: display pacing only (the frame rate is no longer capped by the vertical blank, tearing is possible in exclusive
  fullscreen; in the windowed mode the runs use, DWM composes anyway). `standalone.log` gets one `NOTSA_STANDALONE_NOVSYNC:` line when active. Never use it for frame-time measurements.
* Boot-to-menu (script `until:menu` + `mark:menu`, wall time from process start to the `[mark] menu` line, GPU lock held, `C:/gta-scratch/infra-fast/boot_results.txt`):

  | run | NOVSYNC | vsync (default) |
  |---|---|---|
  | 1 | 2061 ms | 28854 ms |
  | 2 | 1482 ms | 28752 ms |

* Wiring: `soak.sh` passes it through (opt-in, documented in its header; nothing sets it by default), `play.cmd` clears it, docs/STANDALONE.md + STANDALONE_FIDELITY.md + standalone_deviations.tsv + PLAYTEST_AGENT_ENV.md
  ("Fast boot"). The PROBE-SUITE scripts only need `export NOTSA_STANDALONE_NOVSYNC=1` and `gpu_locked.sh` around the run.

## 2. Compile cache

Opt-in `-DGTASA_COMPILE_CACHE=ON|<sccache.exe>` (default OFF: the default configure and the default object code are unchanged). sccache 0.18.0 from the official mozilla/sccache GitHub release, sha256
`8965c74d5e8a225244f741e18ad2f3f504f48228dc1bac948fc22761a348363d` verified by `tools/standalone/windows/get_sccache.sh`, installed user-local under `C:/gta-tools/sccache` (no admin). `tools/standalone/windows/new_worktree.sh <id> [--cache]`
creates a worktree with the untracked conan files and configures it.

What it took (each point was a failure first):
1. sccache cannot cache a compile with `/Yu`: `target_precompile_headers` is replaced (function override in the cache branch) by a target-level `/FI` of the same header.
2. Quoted includes of a forced-include header are searched in the directory of the COMPILED .cpp first: TUs in `game_sa/RenderWare/` picked `game_sa/RenderWare/rw/rwplcore.h` instead of `fakerw/rw/` (redefinition errors) -
   `Zone.h` / `Pad.h` now use `<rw/...>`.
3. The PCH object used to hold the single copy of non-inline header definitions: 15 symbols (7 `notsa::PoolOf<>` specialisations, 8 deprecated `Compress*/Uncompress*Vector`) were LNK2005 in a forced-include link - `inline` added.
   Object code of the DEFAULT build is unchanged by 2+3: `obj_code_sig.py` over all 1119 objects before/after, no section missing or added.
4. Hits across worktrees need `SCCACHE_BASEDIRS` (the key contains absolute paths); it is a server setting, so every source tree gets its own server (port from the path hash) over one shared disk cache. Verified: 125/125 hits in
   another directory, and 1181/1181 for the full build.

Measurements (StandaloneRelease `gta_reversed.exe`, -j4 BelowNormal, 16-core PC, via build_locked.sh):

| build | wall |
|---|---|
| default PCH, fresh worktree (1184 steps) | **87 s** (89 s for a full rebuild after a header edit) |
| default PCH StandaloneRW + all 45 `*_test` (1469 steps) | **199 s** |
| cache ON, empty cache, 1119 TUs | **2182 s = 36.4 min** (an earlier interrupted attempt: 603 TUs in 18 min) |
| cache ON, second worktree in another directory, all hits | **255 s = 4.3 min** (compile 1181/1181 hits + link) |
| same 63-TU subset, cold / warm | 124 s / 18 s |

Why it does not pay: without the PCH every miss re-parses the headers (~2 s per TU wall at -j4 vs 0.08 s with the PCH), and even a hit costs ~0.2 s (sccache hashes ~1000 included files per TU). It is not byte-equivalent either: the cached
exe is 53 MB (default 17 MB; header-defined inline functions / globals and their dynamic initialisers are emitted in every object instead of once in the PCH object, so the static-init order differs), and ~1 % of the code sections of the
default objects have no byte-identical counterpart (example: a `Quaternion` function with a different frame layout). So never use a cache-built exe or test as fidelity evidence. Recommendation: keep it off.

## 3. gpu_locked.sh

`tools/standalone/gpu_locked.sh [-o OWNER] [-t SECONDS] <cmd...>` (+ `--status`, `--reap`), same lock dir and `owner` file as the hand-written snippet and play.cmd. Details in the script header and docs/WINDOWS_SETUP.md ("GPU mutex").
Tests (scratch lock dir, all passed): normal run + exit code + `GPU_LOCK_HELD`; same-shell `kill -9` of the holder -> the next waiter takes over; cross-shell (holder started by PowerShell `Start-Process bash.exe`, killed with taskkill) ->
taken over; USER-PLAY on a 2-3 h old lock: the waiter keeps waiting (rc 124 by `timeout`), `--status` 3, `--reap` leaves it; ownerless dir older than the grace -> dropped, younger -> "FORMING" first; hand-written lock (owner only) 45 min old
with no game process -> dropped, with a game/test process running or fresh -> kept; nested call of the same holder runs directly. `soak.sh` (Windows host) and `fps_run.sh` re-exec under it (`SOAK_GPU_LOCK=0` opts out); `gameplay_regress.sh` / `gameplay_regress/orig_run.sh` (PROBE-SUITE, landed meanwhile; they took and re-took a hand-written lock every 0.3 s / 2 s) use `gpu_locked.sh --acquire "<owner>" $$` / `--release $$` (holder = that script).
Limits: a hand-written lock cannot be tested for liveness, only by age + running game processes (GAMEPLAY-REGRESS still uses that form and re-takes the lock back-to-back, which can starve a waiter); the PT2-*_tools scripts under .notes/reports
still use the old snippet (historical); play.cmd's waiter has no stale detection (batch).

## Notes
* Mistake on my side: a `kill -9` loop over `ps | awk /sleep/` killed other agents' `sleep` children for a moment (their wait loops just continue); no other process was touched.
* `python` / `py` in Git Bash here is the Store stub / launcher: use `C:/Users/littl/AppData/Local/Programs/Python/Python312/python.exe` for tools (`obj_code_sig.py` runs `sys.exit(main())` on import).
* The commits carry `Co-Authored-By: Claude Sonnet 5.5` (the model that did the work; the brief said Opus 5.5).
* Cleanup: worktrees ifc3/ifc4/infra-fast removed, sccache servers stopped, `C:/gta-tools/sccache-cache` (1.4 GB) deleted, sccache binary kept in `C:/gta-tools/sccache`, scratch in `C:/gta-scratch/infra-fast`.
