# REGRESS-SETUP — gameplay_regress: setup flake in combat.free_start, and a retried setup must not fail the suite

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh regress-setup.
## Evidence
Gate 2026-10-23 16:25 (C:/gta-scratch/gate_soak/1023-1625.regress.log): combat.attempt1 "SETUP combat.free_start mission=0 ctl=1
invehicle=1 after mounting/dismounting the tutorial bike"; the retry passed all checks, but the suite prints TOTAL PASS=83 SETUP=1 => FAIL.
REGRESS-FLAKES (.notes/reports/REGRESS-FLAKES.md) fixed the same dismount race in routes/missions.txt with `until:onfoot` + one retry;
the combat route was not changed.
## Task (test side only)
1. Apply the until:onfoot gating + retry to every route that dismounts the tutorial bike (combat, and any other route with the same step).
2. Aggregation: when an attempt's SETUP failed and the retry ran clean, the scenario verdict is the retry's (report the failed attempt as info).
## Acceptance
3 consecutive full suites (GR_SKIP=render_firststart) PASS with no SETUP rows, or SETUP only as info; report ≤10 lines. HARD budget 150k.
