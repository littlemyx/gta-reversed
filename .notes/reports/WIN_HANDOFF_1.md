# Windows host — hand-over after the first (aborted) orchestrator session, 2026-10-16

The user stopped the session to start a fresh one (Claude Code meta settings moved from the Mac). Nothing is running; no build lock.

## Done
* Environment: repo `C:\Users\littl\github\gta-reversed` (not `C:\src`), remote `origin`. `gta_sa_compact.exe` in repo root, sha256 verified.
  Game data = Steam folder `C:\Program Files (x86)\Steam\steamapps\common\Grand Theft Auto San Andreas` (its `gta-sa.exe` is v3.0 — never use). No `C:\gta` copy.
  Disk C: ~130 GB free. GPUs: RTX 4080 + AMD iGPU + Meta Virtual Monitor (check which adapter the game uses before frame-time numbers).
* Installed (WINTOOLS agent, sonnet): Python 3.12 (`%LOCALAPPDATA%\Programs\Python\Python312`, unicorn+capstone+conan), CMake 4.4.4 + Ninja (winget), VS Build Tools 18 with
  MSVC toolset dir **14.51.36231** + SDK. `git config --global core.autocrlf false`. `vendor/librw` now clean (CRLF artefacts gone).
* Commits (cc24627c, caec4700 — pushed? see below; d3557c28): `tools/standalone/windows/msvcenv.sh` — agents not started from gtash.cmd must `source tools/standalone/windows/msvcenv.sh`;
  configure with `--compile-no-warning-as-error` (native cl stops at C4018/C4715 that the Wine flow's /WX- hid).
* conan install Release + Debug done (logs `C:/gta-scratch/conan_{Release,Debug}.log`); `build/StandaloneRelease` configured.

## Open / where to continue
1. **Toolchain check**: `cl` banner says `19.51.36260` while the toolset dir is `14.51.36231`. Compare with the cl banner of the Mac/Wine build (e.g. a `.notes/reports/*` or the Mac's `cl` output);
   if the Mac also printed 36260 it is fine (dir version != compiler build number), otherwise this is a fidelity-contract mismatch -> stop and decide.
2. **StandaloneRelease build** stopped at 365/1121: `FxInfoUnderwater.cpp.obj` FAILED with code 3221225794 = 0xC0000142 (STATUS_DLL_INIT_FAILED, cl.exe failed to start — most likely
   too many parallel cl processes / desktop heap; not a code error). Re-run `tools/standalone/build_locked.sh ninja -C build/StandaloneRelease gta_reversed.exe` (try `-j8`). Then confirm
   `bin/gta_reversed.{pdb,map}`, `original_data.bin/.json`, `data_pointers.bin`.
3. WINTOOLS report `.notes/reports/WINSETUP_TOOLS.md` was never written (agent stopped) — fold it into the next setup report.
4. Then the orchestrator prompt steps 2..6 unchanged: run_all_tests.sh (StandaloneRW) at PC24, soaks 3+3+15 min, WIN_FPS frame times, user listen test, stream D B07 verification -> B09+.
