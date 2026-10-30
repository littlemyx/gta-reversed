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

## Update 2026-10-26 20:20 — strategy switch (user: "simpler than frames")
- Main loop is now the ASM sweep (.notes/asm_sweep/PLAN.md, INDEX.md, Snn.tsv): agents compare port source vs `tools/standalone/exedis.py`
  output per function, fix sure deviations, log UNSURE. Rate ~5-12 fixes per agent (vs ~2 per lockstep agent). Continuations via
  scratchpad cont.py (`py cont.py Snn Snnb "status"` in .notes/asm_sweep: builds the NOT_REACHED list + INDEX row). Small functions first;
  exedis caps output at 400 lines; huge ones (GenerateOneRandomCar ~2169 lines) need their own agent.
- Done: S22 (CarCtrl, 40/55 + S22c 3; S22d 15 left), S23 (Population, done 32/33), S26 (Physical, 26/28; S26c 2 left), S20 (Ped, 54/75;
  S20b 21 big left). Running: S16 (Automobile). Next small-function slices: S01-S15 Tasks, S17-S19 Vehicle, S21, S24, S25, S27-S34.
- Lockstep (RD-AB-*) paused at prefix 145 (earliest real: driven car slot 7 shifted from fc 612). Combat scenario flaky in 3 checks ->
  brief COMBAT-FLAKY written, launch after the gate (note: ASM-S20 fixed CreateDeadPedWeaponPickups ClearWeapons — maybe related).
- Systemic finding to schedule: CVector operator/ divides, the exe multiplies by 1/x (Vector.h:255) — ~15 sites in Physical.cpp + more.
- Agent token self-estimates run 30-50% low: briefs say "stop at 120k by your estimate".

## 2026-10-28 04:00 checkpoint
- portable/main merged and pushed (09267fa2); PortableWin64 configured in the main checkout; orch_gate.sh has the portable step (prc).
- Gates green: 145493f7, 09267fa2, 0b3d3f12 (incl. portable). Running: gate on 304805a6.
- ASM sweep done/partial since: S20 complete (Ped.cpp); S16 Automobile down to S16i (PreRender tail); S17 down to S17e (running); S13c, S14b, S18c, S21c, S22e, S27c pending; S01-S10, S15, S19, S28, S30-S34 untouched.
- Systemic: SYSTEMIC-1/2/3 landed (ExeDivide, sector helpers, coin flips, TaskTimer partial, literal division scanner tools/standalone/litdiv_scan.py + LITDIV.tsv). FIXBUGS-AUDIT 1-2: unguarded port fixes moved under FIX_BUGS (FIXBUGS.tsv). Open items: SYSTEMIC_TODO.md.
- Pending user OK: delete agent scratch C:/gta-soak-s16e, C:/gta-scratch/asm-s16e/f/g/h, systemic-3, asm-s17c/d (keep asm-s17c, asm-s16g/tools until S17e is done).

## 2026-10-28 21:00 checkpoint
- Gates green today: 74b2b51e, 38b266a1, 8ca88bfe (C:\gta-play refreshed to 8ca88bfe). Gate on e0b8b173 running (tests 74/74, soak OK, regress pending).
- ASM sweep done today: S13, S16, S18, S21, S22, S27 (all complete); Tasks S02–S10 complete; S15 to S15e (1 row: LineUpPedWithCar::ProcessPed 0x6513A0); S19 at S19d (101 rows: CTrain from AddNearbyPedAsRandomPassenger 0x6F8170, then CVehicle per-frame, then Trailer).
- Not started: S28 Streaming, S30 Camera, S31 Cam, S32 Radar, S33 WaterLevel, S34 Interior_c.
- Agent brief template: scratchpad asm_brief_template.md (copy kept in this note's spirit: landing at ~110k, RW_EXE_ORACLE=<exe path>, no bare double, own dis.sh copy, msvcenv before new_worktree, --force --force worktree remove when clean+pushed).
- SYSTEMIC_TODO new: X87-POW, IsPointInSphere order, double literals in float compares, runtime globals vs literals.
- Flakes: combat.foot.weapon_pickup / chase.no_zombie_cops fixed by COMBAT-FLAKY (16429ddc); new chase.player_shot_at once (maybe heal-every-2.5s side effect) — watch.
- Pending user decision: delete C:/gta-wt/asm-s04 (busy dir) and old scratch folders.

## 2026-10-29 03:30 checkpoint
- Gates green: 74b2b51e, 38b266a1, 8ca88bfe, e0b8b173, 877b0375 (C:\gta-play = 877b0375). 20f92a94 failed only PortableWin64 (stale double fwd decl, fixed 5ae8c9b8). Gate on 5ae8c9b8 running.
- Complete: S02–S10, S13, S16, S18, S19 (trains+vehicles+trailer), S21, S22, S27, S28 (Streaming), S34 (Interior_c). S15 left S15e (1 row LineUpPedWithCar::ProcessPed). S33 left S33b (render). S32 at S32b (agent running).
- Not started: S30 Camera, S31 Cam. 
- Brief template (scratchpad asm_brief_template.md) now also: no NaN-only leftovers, no "relies on pool memory", PortableWin64 compile when changing x87d signatures, own dis.sh copy.
- Pending user decision: delete C:/gta-wt/asm-s04, asm-s19j (busy dirs) and old scratch folders.

## 2026-10-30 00:15 checkpoint
- Gates green through 454531de9 (C:\gta-play = 454531de9). Gate bb1pbdkk4 on 1b17f2660 running.
- Phase 1 sweep complete; systemic items done (dbllit, x87::pow (+float), CRect, sphere, timers, LITDIV, FIXBUGS 2, clock ranges); clean -WX from-scratch build.
- Phase 2 (PLAN.md "Phase 2", S41-S66): done S41/S41b, S42-S42e, S43-S43d, S44, S47-S47c, S49/S49b, S50/S50b, S52, S55/S55b, S56/S56b/S56c. Open/next: S45 (running), S48c (running: CGame + CGarage::Update handlers + GameLogic::Update tail), S51b+S58 (running), S53c (1 row), S54 (40), S57c+S64 (running), S59, S60-S63 (task allocators/tasks), S65, S66 (RunningScript).
- User asked (10-29 ~22:00) to burn the remaining weekly quota: run 5 agents in parallel. Weekly resets 10-30 11:00.
- Briefs: scratchpad phase2_brief.md (+ asm_brief_template.md); cont.py carries NOT/PARTIAL/UNSURE/OPEN rows.
- Flake to watch: combat.foot.weapon_pickup (after TryToMerge radius 5.0->5.5, SYSTEMIC-6).
- Pending user decision: delete busy empty dirs C:/gta-wt/asm-s04, asm-s19j, asm-s43c + old scratch.

- [win] 2026-10-30 02:15: S46 was skipped by mistake in the phase-2 launch order — still pending.

## Checkpoint 2026-10-30 (day)
- Gate 1b17f2660 GREEN (74/74, soak OK, regress 70 PASS, portable 0 errors); C:\gta-play = 1b17f2660. Next gate on df4d307ec (205 commits: S45-S66 + continuations).
- Phase 2 queue done except S45c/S57e; coverage audit (COVERAGE.md): 487 gameplay hooks uncovered -> S67-S80; 1521 audio/fx/UI hooks left for a phase 3.
- Gate df4d307ec GREEN (74/74, soak OK, regress 70 PASS, portable 0); C:\gta-play = df4d307ec. Gate GPU tests starved ~2h by 5 agents' combat regress (gpu.lock has no queue) -> phase-3 agents run regress once at the end. Next gate 8f00acb1d (229 commits: S67-S80 phase 2b, S81/S82/S86/S91 phase 3).
- 10:2x gate 8f00acb1d stopped (GPU-starved 3h, mis-read as stuck: its log only prints the first wait line); restarted on 71979d2d1 with GPULOCK-PRIO (gpu.prio). Deleted 9 empty worktree dirs (user OK).

## Checkpoint 2026-10-30 (evening)
- Gate 4ca75523c GREEN (74/74, soak OK, regress 70/70, portable 0); C:\gta-play = 4ca75523c.
- Gate on 67dedce7a started (S106, S111-S114 not yet gated).
- Pacing: <=2 sweep agents (quota). Running: S96, S115. Left: S97 (after S96), S117, S101b (3 rows).
- Gate 67dedce7a GREEN (74/74, soak OK, 70/70, portable 0); C:\gta-play = 67dedce7a. Gate on 09b19a8c6 started (S96, S97, S101b, S115, S117).
- Phase 3 left: S97b (running), S96c (8 Draw rows, after S97b).
- Gate 09b19a8c6 GREEN (74/74, soak OK, 70/70, portable 0); C:\gta-play = 09b19a8c6. Gate on ea87a5842 started (S97b, S97c, S97d/S96c). S97e+S96d (last menu rows) running.
- Phase 3 COMPLETE (S97f landed 16e80bf4d). INDEX 0 pending. Next gate after ea87a5842 must cover S97e/S96d/S97f.
