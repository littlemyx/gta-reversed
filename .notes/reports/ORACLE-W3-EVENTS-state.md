# ORACLE-W3-EVENTS state (hand-back at the tool-call cap)

Done and landed (branch oracle-w3-ev, rebased on origin/reverse/interior_c, pushed fast-forward): see ORACLE-W3-EVENTS.md. wave3_events_oracle_test: 44 rows, PC24 0 / PC53 0. 10 port-fix commits, rows commit, report / coverage (3082) / PHASE2_STATUS commit.

NOT done: the full `run_all_tests.sh` gate (48 tests) in the worktree. A first attempt died with the tool shell (nohup), a second was stopped at the cap before it built anything beyond wave3_events_oracle_test. Worktree C:/gta-wt/oracle-w3-ev is KEPT (configured, wave3_events_oracle_test built) for the continuation.

Next steps:
1. `cd C:/gta-wt/oracle-w3-ev && source tools/standalone/windows/msvcenv.sh && GTA_EXE=C:/Users/littl/github/gta-reversed/gta_sa_compact.exe tools/standalone/run_all_tests.sh build/StandaloneRW` with run_in_background (NOT inside build_locked.sh); expect 48/48. Risk: wave2_oracle_test rows touching CEventGroup / KillCriminal ctor / Compute*Response are the only ones that could change.
2. Then `git worktree remove --force C:/gta-wt/oracle-w3-ev && git branch -D oracle-w3-ev`.
3. Optional: class C (65) of game_sa/Events; unobservable label fixes (SeenCop TASK_NONE, Special TASK_NONE / KILL_CRIMINAL); make CEventHandler::IsKillTaskAppropriate static (cdecl in the exe).
Scratch: C:/Users/littl/AppData/Local/Temp/w3ev (run logs run1..run12.log, helper notes). Tool calls used: about 146.
