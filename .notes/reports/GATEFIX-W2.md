# GATEFIX-W2 (gate stale-exe + wave2_oracle_test compile)
- Problem 1 (run_all_tests.sh): `cmd //c start /wait ninja` loses ninja's exit code, so BUILD_FAIL was never set and a test whose compile FAILED ran its old exe and reported ok.
- Fix: BUILD_FAIL is now derived from the ninja.log (`^FAILED:`, `ninja: build stopped`, `ninja: error`). On failure each test is NOBUILD when its own FAILED step names `<t>.dir/.exe`,
  or when `ninja -n <t>` is not "no work to do" (covers targets blocked by a failed dependency: -k 0 prints no FAILED line for those). NOBUILD never runs the exe, sets RC=1, counts as "other".
- orch_gate.sh already greps `NOBUILD` rows and fails on trc != 0 (run_all_tests exits 1) -> no change needed there.
- Evidence: built wave2_oracle_test, then injected `#error` into tests/standalone/wave2_oracle_gen_func.inc (worktree only, reverted): run with filter `^(wave2_oracle_test|pool_oracle_test)$`
  -> "wave2_oracle_test - - - 0 0 NOBUILD", `summary: 2 tests: 1 ok, 0 SKIP, 1 other`, rc=1 (stale exe not run). Logs: C:/gta-scratch/gatefix-w2/run2.txt.
- Problem 2 (wave2): a5fda036 (10-23) widened CTaskSimpleIKChain::m_OffsetBone to eBoneTag32 (exe 0x633E38 `mov [esi+2Ch], edi` dword store, 0x633F5F dword load); the checked-in
  generated row wave2_oracle_gen_func.inc:852 still said eBoneTag16 -> C2440. The member is right; row patched to eBoneTag32 (what gen_w2_funcs.py emits from the header; wave3 .inc already had 32).
- Result (fb5f8c02): wave2_oracle_test builds and runs: 1142 functions, PC24 0/0, PC53 0/0 (= last known). No mismatch rows. Log: C:/gta-scratch/run_all_tests.53322/wave2_oracle_test.1.log.
- Other tests in the 33894 ninja.log: only one FAILED step (wave2_oracle_test); no other test had a failed compile or link.
