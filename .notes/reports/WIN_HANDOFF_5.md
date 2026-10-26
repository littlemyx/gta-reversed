# Windows handoff 5 — 2026-10-24 15:05 WEST

## State
- reverse/interior_c 3b369d38+ (pushed): portable/main merged (136deaaf, full gate green), replay-diff (RD-ARM/RD-NOISE), ANIM-CRASH-3,
  CHEATS-ARMOUR, KILL-AUDIT. C:\gta-play = 136deaaf Release.
- Gating now (main checkout, local, not pushed): 14186305 = merge of origin/save-load (FID-MAC 6 fixes + SAVE-LOAD 4b231249 Win32
  CRunningScript save via FileRecord, cmd:savemenu / cmd:endmission, tools/standalone/saveload/). Push when green.
- Coverage 5049/9248 (oracle_coverage.py). Rules v5 (CURRENT). Weekly quota 25% at 10-24 15:00 (reset 10-30 11:00) — ahead of linear: pace 1 agent + pauses.
- Orchestrator commits notes via worktree C:/gta-wt/orch-notes (`git checkout -B orch-notes-tmp origin/reverse/interior_c`, commit, push HEAD:reverse/interior_c).
- Gates: run `bash tools/standalone/windows/orch_gate.sh regress` with run_in_background (visible task), never nohup. Gate reconfigures CMake
  with --compile-no-warning-as-error (new globbed .cpp files).
- Mac session: portable/main, goal = re3-level portable fidelity (user). Sync channel .notes/PORTABLE_SYNC.md; merge portable/main after a gate
  at least every 2 days.

## Next
1. Gate 14186305 -> push; PORTABLE_SYNC line.
2. RD-NOISE continuation (.notes/reports/RD-NOISE-state.md): reuse the Mac RR pins (NOTSA_RR_STEP_MS, seed, NOAUDIO) for the port leg;
   measure pin 128 (audio rand); RandomBool caller at port frame 8; orig-vs-port script hash at frame 1, rand count at frame 3 (189 vs 163).
3. RD-INPUT / RD-STATE / RD-FIRSTDIFF (REPLAY_DIFF_PLAN.md slices 3-5) — the A-B campaign is where new port bugs come from now
   (class-C oracle rows cost ~35k tokens each).
4. SAVE-LOAD leftover: cars/peds spawned before a save are missing after load (not analysed).
5. Open from earlier: POSTFX-RASTER screenshots; class S/D by subsystem; playtest offer open.

## Update 2026-10-26 13:40
- Lockstep A-B loop (RD-AB-3..15): orig-port equal prefix 3 -> 88 frames; ~25 port fixes found by it (RemoveBuildingsNotInArea inverted,
  ped-group slots/leader, SetupSuspensionLines, gang counters, AffectsPedGroup/AffectsPed, SetMoveAnim walk speed, ...). Loop per agent:
  brief .notes/briefs/RD-AB-N.md (derived from N-1 by sed + Evidence rewrite), gate after each landing, regress flakes re-run x2 and
  logged in .notes/regress_flakes.tsv. TP-HEAP fixed an 8-byte heap overflow on plane spawn (GetWheelPosn).
- Gate script now: rw_* tests under gpu.lock; refresh C:\gta-play only if tests + soak + regress pass. Agents' own token estimate runs ~25% low:
  briefs say "stop at 150k by your estimate".
- Scratch: agents leave C:/gta-scratch/rd-ab-N with soakdata junctions; ask the user before deleting (he wants to be asked), remove junctions
  with cmd rmdir first, then rm -rf; keep rdd/ab4, rd-ab-9/o100 and px_* proxies.
