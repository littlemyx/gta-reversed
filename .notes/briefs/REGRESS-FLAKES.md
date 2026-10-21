# REGRESS-FLAKES — make gameplay_regress deterministic again (interior_lspd.cops_on_floor, missions.car.enter_return)

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh regress-flakes.
## Evidence (.notes/reports/REGRESS-POLICE.md, gate 2026-10-21 10:54: C:/gta-scratch/gate_soak/1021-1030.regress.log)
1. interior_lspd.cops_on_floor: with world streaming fixed, exterior police exist while CJ is inside the LSPD; the checker counts every cop
   within 40 m in all snapshots → |dz| of an outside cop (−21..−26 dm) fails ~1 run in 3. The check is meant for the cops PopulateInterior
   places (U15: they spawned above CJ).
2. missions.car.enter_return failed once in a full run, passed 3/3 alone.
## Task (test-side unless the evidence says otherwise)
1. Restrict cops_on_floor to the interior's own peds (area code == the interior's area, or peds created by PopulateInterior — whichever the
   probe can report; extend the GameplayRegress probe if needed), and confirm on the ORIGINAL exe (-o leg) that the check passes there too.
2. Investigate enter_return: run the missions scenario ~10x in a full-suite context; if it is a timing/route flake fix the route/threshold
   with a `why` in baseline.json; if the port behaves differently from the original exe, it is a port bug → brief-sized note in the report.
## Acceptance
Full gameplay_regress (render_firststart skipped) PASS 3/3 consecutive runs; report ≤15 lines with start/end time. HARD budget under 200k.
