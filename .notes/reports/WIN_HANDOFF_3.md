# Windows host — state 2026-10-18 ~05:40 (orchestrator checkpoint before context compaction)

* Stream D COMPLETE (A–G): fresh clone builds + runs with no original exe (D-G.md). FIX-AUDIT/NOTSA-AUDIT: run build follows the exe
  (notsa bugfix switches off, ~20 NOTSA sites gated). ORACLE-W1: coverage 1715/9240 functions (`.notes/ORACLE_COVERAGE.md`, tool
  oracle_coverage.py), 10 port bugs fixed.
* User playtest 1 (`.notes/USER_PLAYTEST_1.md`, 14 findings) → agents PT-AUDIO, PT-RENDER, PT-COMBAT, PT-MISSION, VT-OVERRIDES (all done,
  reports in .notes/reports/). Root-cause classes: unported vtable overrides (audit now 0 unexplained), pools handed out 0xFD storage
  (run build zero-fills), NOTSA reverse pool loops UB (GetAllValidReverse), ResetIP stack, Pad Enter/extenter swap, heat-haze buffer on
  mode change, SDL relative mouse. Open: U1 first-start window on 4K@150% needs the user's re-test; U4 corpse pose; U8 likely fixed;
  ghosting in photo 4; "mini-jack" crackle needs the user's ears; flaky BuildShadowVolume AV (FLAKES.md).
* Infra: build_locked.sh holder PID (a9e85c7b); GPU lock C:/gta-scratch/gpu.lock (owner USER-PLAY = user playing, agents wait and
  don't build); shared brief `.notes/PLAYTEST_AGENT_ENV.md`; watchdog script in the orchestrator scratchpad.
* In progress at checkpoint: orchestrator gate run (Release build, run_all_tests StandaloneRW, soak 3 min) on ac70ea8a, logs
  C:/gta-scratch/gate_pt1/. Then: restore the user's gta_sa.set from C:/gta-scratch/playtest1/gta_sa.set if soak deleted it, refresh
  C:\gta-play (copy build/StandaloneRelease/bin/gta_reversed.exe + tools/standalone/windows/play.cmd), ask the user for playtest 2.
* Next work candidates: oracle wave 2 (allocator/lock-import patching unlocks 773 functions), scripted gameplay probes as a regression
  suite (PT-COMBAT probe, PT-AUDIO stats, cheat probe), whole-game record/replay vs the original; portability streams (Mac).
* Quota 2026-10-18 05:40: weekly 36 % (resets 10-23 11:00Z).
