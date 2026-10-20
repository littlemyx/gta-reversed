# INPUT-HANG — wave3_input_oracle_test hangs in the orchestrator's full run (passes in the agent's worktree)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh input-hang.
## Evidence
Orchestrator gate on dbf765e5 (main checkout, run_all_tests.sh, 2026-10-20 08:03): wave3_input_oracle_test timed out on attempt 1
(15 min) and attempt 2, at DIFFERENT rows: attempt 1 stopped after AffectControllerStateOn_ButtonDown_VehicleAndThirdPersonOnly 0x52FD20,
attempt 3 after CControllerConfigManager::Constructor 0x531EE0. Logs: C:/gta-scratch/run_all_tests.116581/wave3_input_oracle_test.{1,2,3}.log.
ORACLE-W3-INPUT (.notes/reports/ORACLE-W3-INPUT.md) ran the same target green in its worktree. Nondeterministic → suspect a row that reads
real OS input state (GetAsyncKeyState/GetKeyState, DirectInput, SDL, the user's live mouse/keyboard), an uninitialised loop bound in
the fixture, or a port/exe loop that spins on such state.
## Task (one target)
Reproduce (run the target in a loop, W2_PROGRESS=1 to see the row before the hang; attach cdb or dump the stack of the hung process),
find the root cause, fix the harness or the port as the exe behaves (if a port function can spin forever where the exe cannot, that's a
port bug — fix it). The target must finish in <2 min deterministically.
## Acceptance
20 consecutive runs of wave3_input_oracle_test, each < 2 min, PC24 0; root cause stated with evidence; report ≤15 lines with start/end time.
