# Common environment for the playtest-1 fix agents (Windows host, 2026-10-17)

Project: gta-reversed standalone — a GTA SA that behaves EXACTLY like the original exe (its behaviour, bugs included, is the spec;
proven from its asm and with the exe-oracle). Win32 x86 MSVC = reference. Findings: `.notes/USER_PLAYTEST_1.md`, evidence in
`C:/gta-scratch/playtest1/` (crash log, game log.log, the user's gta_sa.set, screen photos 2/3/4.webp).

## Repo, toolchain, builds
- Repo C:\Users\littl\github\gta-reversed (Git Bash /c/Users/littl/github/gta-reversed), branch `reverse/interior_c`, remote `origin`.
- In every Bash call that compiles/runs: `source tools/standalone/windows/msvcenv.sh` first. Python: `python -X utf8 -I`.
- **Work in your own git worktree** with a SHORT path (`git worktree add C:/gta-scratch/<id> HEAD`), configure presets there with
  `--compile-no-warning-as-error` (conan per docs/WINDOWS_SETUP.md; StandaloneRelease = the user's exe, needs no original exe;
  StandaloneRW for oracle tests needs `GTASA_ORACLE_EXE`/`RW_EXE_ORACLE` = the main checkout's `gta_sa_compact.exe`). Remove the worktree at the end.
- Builds only via `tools/standalone/build_locked.sh <cmd>`, **ninja -j4 max, low priority** (the user works on this PC). Never kill all
  cl.exe — only your own process tree. LNK1201 on a pdb: rename it and relink.
- Land commits in the main checkout: `git commit -- <your files>` (cherry-pick from your worktree branch is fine). Never `git stash`,
  never touch others' uncommitted files. Push: `git pull --rebase` (if refused because of others' unstaged files: push only if the
  branch is just ahead) then `git push origin reverse/interior_c`. English messages, trailer
  `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. If the Write tool refuses a report file, use a Bash heredoc.

## Running the game
- **GPU lock** around every game run / device test: `until mkdir C:/gta-scratch/gpu.lock 2>/dev/null; do sleep 15; done;
  echo "<ID> $(date)" > C:/gta-scratch/gpu.lock/owner; <run>; rm -rf C:/gta-scratch/gpu.lock`. Hold it only for the run itself
  (≤ 20 min), release between runs; 4–5 agents share it. **If the owner is USER-PLAY, the user is playing: wait, and do NOT start
  builds or heavy work either** (it would cause stutter) until that lock is gone.
- Data dir: build your own like soak.sh does (links to the Steam folder
  `C:\Program Files (x86)\Steam\steamapps\common\Grand Theft Auto San Andreas` — read-only; its gta-sa.exe is v3.0 and NOT the
  oracle). Never touch `C:\gta-play` (the user's copy) and never touch the user's `Documents\GTA San Andreas User Files\gta_sa.set`
  (copy it if you need its settings; soak.sh deletes/backs up the live one — run it with a private user-files dir if possible, or
  restore the user's file afterwards from `C:/gta-scratch/playtest1/gta_sa.set`).
- Silent runs: `NOTSA_STANDALONE_MUTE=1` (+ DirectShow mute); `NOTSA_STANDALONE_WINDOWED=960x600` so the desktop isn't switched
  (unless the bug is about fullscreen). Scripted input: `NOTSA_STANDALONE_INPUT` (docs/STANDALONE.md: wait/key/down/up/until:control/
  until:invehicle/until:menu, cheats can be typed as keys), screenshots `NOTSA_STANDALONE_SCREENSHOT`, logs standalone.log + logs/log.log.
- **The original exe as reference**: gta_sa_compact.exe (main checkout root, never commit) can be run natively for A/B — see
  `.notes/reports/WIN-PERF.md` / `BUG-VIDEO.md` (private data dir, dsound mute proxy, radio auto-select off; DirectPlay is installed now).
- **Older builds for bisecting**: branch `standalone-snapshot-2026-10-16` (state before the Windows sessions), exes
  `C:/gta-scratch/bv_play_old_gta_reversed.exe` (pre stream-D phase C, with its original_data.* next to it if needed) and
  `C:/gta-scratch/play_prev_gta_reversed.exe` (BUG-VIDEO's snapshot); any commit can be rebuilt in your worktree (`git bisect`).
- Debugger: x86 cdb in C:\gta-scratch\dbg. Exe-oracle tests: tests/standalone/*_oracle_test.cpp, runner tools/standalone/run_all_tests.sh.

## Rules
- Root-cause every bug against the exe asm; fix the port, never mask (no extra null checks / clamps the exe doesn't have). If the
  original exe shows the same behaviour on this PC, it's not a bug — report it as "original behaviour" with evidence.
- Recent risky changes (likely regression sources): stream-D phases B/C/F (objects zero-initialised instead of repo defaults,
  NOTSA_GLOBAL_* conversions, ExeZero), FIX-AUDIT (17 notsa bugfix switches OFF in the run build), CAM-INIT, D-C56 audio zero-init,
  D-F2/F3 (hook registry and data image removed), FLAKES (IplStore), BUG-REFS (pool shutdown).
- Each fix: asm evidence in the commit message, an exe-oracle row if the function is oracle-able, and a scripted repro run before/after.
- Report: `.notes/reports/<ID>.md` (per finding: original-exe behaviour, repro, root cause, first bad commit if bisected, fix commit,
  verification). One status line in `.notes/PHASE2_STATUS.md`. Final message ≤15 lines. Do not spawn sub-agents.
