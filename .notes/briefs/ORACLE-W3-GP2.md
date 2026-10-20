# ORACLE-W3-GP2 — gameplay root files slice 2: TrafficLights, Restart, Shopping, PedAttractor, Vehicle, World leftovers (A+B)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-gp2.
## Evidence
Coverage JSON (main checkout, 2026-10-20): uncovered A/B — TrafficLights.cpp 9 (light phases cars obey), Restart.cpp 9 (hospital/
police restart points after wasted/busted), Shopping.cpp 8 (prices, owned items), Attractors/PedAttractor.cpp 8 (peds using bus stops,
ATMs, benches), Entity/Vehicle/Vehicle.cpp 9, World.cpp 12 (leftovers after ORACLE-W3-WORLD/PATHS). Lessons: ORACLE-W3-STREAM.md,
ORACLE-W3-MISC.md, ORACLE-W3-AI.md, ORACLE-W3-WORLD.md.
## Task
Rows for these A+B functions with real fixtures; fix every mismatch (one commit per fix, asm address and pre-fix failure count in the
subject) or skip with reason. Target < 5 min. HARD budget under 250k tokens. Partial landing + state file at the cap is fine.
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time.
