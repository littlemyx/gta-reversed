# USERDIR - test runs no longer touch Documents\GTA San Andreas User Files

## Change
* `NOTSA_STANDALONE_USERDIR=<path>` (run build only, `#ifdef NOTSA_STANDALONE_RUN`; unset/empty = exe behaviour = Documents): `InitUserDirectories` (0x744FB0, source/game_sa/FileMgr.cpp)
  uses `<path>` as the user-files dir (priority over the ini `SaveDirectoryPath`), creates it (`create_directories`) and Gallery / User Tracks below it. It is the only place that resolves
  the dir (gta_sa.set, saves, FastLoader slot check all go through it). Listed in standalone_deviations.tsv, STANDALONE.md, STANDALONE_FIDELITY.md, WINDOWS_SETUP.md.
* `hostenv.sh`: `userdir_new <tag> [seed]` / `userdir_reset <dir> [seed]` (private dir `C:/gta-scratch/userdir/<tag>-<time>-<pid>`, seed `playtest1` | file | none) and, for the ORIGINAL exe which
  cannot be redirected, `docs_backup <dir>` / `docs_restore <dir>` (whole Documents dir, content + mtimes, the restore also deletes what the run created).
* Scripts: `soak.sh` (own dir, re-created before every start attempt; the old gta_sa.set.soak-bak / delete-the-live-file logic is gone), `gameplay_regress.sh` (own dir per suite, re-created per scenario,
  gta_sa.set of the run copied next to the scenario output), `gameplay_regress/orig_run.sh` and `windows/fps_run.sh orig` (docs_backup/restore), `fps_run.sh ours` (own dir, holds the generated set file),
  `windows/startup_probe.py` (own dir for gta_reversed.exe, backup/restore Documents for any other exe). `play.cmd` only CLEARS the variable (behaviour unchanged: Documents).
* PLAYTEST_AGENT_ENV.md updated (manual restore step dropped, "never touch Documents" kept + how to do it in own launches).

## Decision to flag: default seed
The brief said "seeded from the playtest1 gta_sa.set". soak.sh / gameplay_regress.sh have ALWAYS started WITHOUT a gta_sa.set (first start; render_firststart even tests exactly that, and a start with a saved
video mode used to crash in ResetDeviceNow), so the default stays EMPTY to keep all baselines comparable; `USERDIR_SEED=playtest1` (or a path) seeds the playtest-1 settings on demand.

## Verification (Documents dir hash = sha256 of the sorted per-file sha256 list, all files)
| step | result |
|---|---|
| hash before | ee1d48c3...de867 |
| after `soak.sh` (7 min, NOVSYNC, windowed; RESULT OK, 0 errors; private dir got gta_sa.set + Gallery + User Tracks) | identical |
| after `gameplay_regress.sh cheats render_vmode` (14 PASS; private dir per run) | identical |
| `docs_backup`/`docs_restore` on a fake dir (modified, deleted, extra file, extra dir, mtime) | restored exactly (the ORIGINAL-exe legs themselves were not run: they switch the desktop mode) |
| Release build (StandaloneRelease, -j4) | ok |
| run_all_tests (StandaloneRW worktree build) | 45 ok, 0 skip, 0 fail |
