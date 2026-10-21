# WIN_HANDOFF_4 — Windows orchestration checkpoint (2026-10-21 01:45 WEST)

Continue from here after a compaction / new session. Previous: WIN_HANDOFF_3.md.

## State
- Branch reverse/interior_c (origin). run_all_tests: 70 targets green at fe908b7c; gate on 87ca9268 (+TRAP-AUDIT, LINK-CALLEES) running.
- Exe-oracle coverage 4685 / 9248 hook sites (was 1715 on 10-18). Remaining uncovered: A ~330, B ~400, C ~1175, S ~745, D ~1990.
- C:\gta-play refreshed after every gate (play.cmd drops dead agents' GPU locks).
- Agent rules: v3 (.notes/agent_rules/v3.md): 1 agent at a time, 100-min wall-clock cap, HARD token budget 200-250k in each brief,
  orchestrator runs the full gate + Release + C:\gta-play refresh after every landing. metrics.tsv / quota.tsv appended per agent.
- Quota pacing: weekly 72 % at 10-21 00:30, reset 10-23 11:00. With 30-60 min pauses between agents the pace is ~0.25-0.45 %/h (OK).
- Briefs live in .notes/briefs/<ID>.md; reports in .notes/reports/<ID>.md.

## Wave-3 slices done (oracle rows + port fixes; see reports)
VEH(+OPEN), PED(+OPEN x2), GAMEPLAY, EVENTS, TASKS(+2), COLL, COLLPOINT-ASSIGN, ANIM(+2), MODELS, AUDIO(+OPEN), ENTITY, INTERIOR,
CAMERA, SCRIPTS, WORLD, PATHS, UI, EFFECTS, INPUT(+HANG), AI(+COVER-REWRITE), IK (RW sqrt tables exact game-wide), FX, FXBP, VIS,
HOOK-ADDR (address audit), CARENV, MISC, STREAM, GP2(+2), TRAP-AUDIT (+W2_PORT_FAULT_IS_DIFF), LINK-CALLEES, ZONEINFO, AUDIO2.

## Next candidates (A/B left, coverage JSON via tools/standalone/oracle_coverage.py --json)
Replay.cpp 27, Audio/Loaders (MP3BankLoader 10, DataStream 9), Renderer 10, GrassRenderer 9, 2dEffect 9, PostEffects 9, platform 8,
remaining audio-entity ctors (AUDIO2 skip list), Vehicle/World hard leftovers (GP2-state), split wave3_audio target (610 s, >5 min).
Then class C/S (fake vtables / small worlds) by subsystem, starting with Tasks and Vehicle.

## Open user-facing
Playtest offer pending (non-blocking): new build differs a lot from playtest 2 (lighting tint, engine sound, pad steering, glass,
plants, messages queue, cover/attractor AI). SKIN-FLAKY crash still unreproduced.

## PAUSE 2026-10-21 14:40 WEST (user asked to pause all work)
- Everything committed and pushed: origin/reverse/interior_c (main line) + origin/consumer-audit (WIP branch, see below).
- Since the checkpoint above landed: ORACLE-W3-RENDER, POSTFX-RASTER (code; screenshots pending), NEWGAME-CRASH (505a4aef: CTheScripts::Process
  successor test — new game crashed since ~10-18), REGRESS-POLICE (729926c4: ToBeStreamed readers — no traffic/police), REGRESS-FLAKES,
  LSPD-POP (a7b39d64/ff4685f3: attractor ped placement), rules v4 (soak + gameplay_regress in every gate, consumer audit).
- Last full gate: 565fb3a0 — run_all_tests 72/72, soak OK; gameplay_regress 3 runs after REGRESS-FLAKES on 4a99e638: PASS/FAIL(interior,
  since explained and fixed by LSPD-POP)/PASS. The gate on 1190459b (LSPD-POP) was STOPPED by the pause: not gated yet.
- C:\gta-play = build of 565fb3a0 (police fix, soak OK). LSPD-POP and later are NOT in it.
- Open WIP: worktree C:/gta-wt/consumer-audit (kept on purpose), branch consumer-audit pushed: CONSUMER-AUDIT table complete (311 commits,
  89 non-local checked, 0 unexplained) + 1 fix 671d3437 (CGarages::PrintMessages number order). NOT soaked/gated, not merged.
- Resume order: (1) gate.sh regress on reverse/interior_c HEAD; (2) soak + gameplay_regress on consumer-audit, then merge it;
  (3) POSTFX-RASTER screenshots (command in its report); (4) next A/B slices (list above), then classes C/S, then record/replay for D.
- Agent rules: CURRENT = v4. Orchestrator gate script: tools/standalone/windows/orch_gate.sh (+ orch_watchdog.sh).
