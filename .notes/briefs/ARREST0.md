# ARREST0 — arrest at 0 stars hits NOTSA_UNREACHABLE (GameLogic.cpp:733)

Rules: .notes/agent_rules/v2.md. Worktree: C:/gta-wt/arrest0.
## Evidence
PROBE-SUITE (.notes/reports/PROBE-SUITE.md): when the player is arrested with 0 stars, the port's CGameLogic fee switch reaches
NOTSA_UNREACHABLE (source/game_sa/GameLogic.cpp:733); the exe falls through to a fee of 100.
## Task (one function)
Disassemble the exe's function containing that switch, port the default/fall-through exactly, audit the rest of that function line by line
against the asm (helper sub-agent allowed for the review). Add an exe-oracle row (wanted level 0..6 → fee/money) if the function is
oracle-testable; otherwise a gameplay_regress.sh check (arrest:0 at 0 stars).
## Acceptance
Oracle row or regress check passes on HEAD and fails with the old code; Release builds; run_all_tests 45/45. Report ≤15 lines.
