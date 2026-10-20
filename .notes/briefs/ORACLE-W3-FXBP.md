# ORACLE-W3-FXBP — game_sa/Fx/Blueprints classes A+B (+ Fx.cpp RotateVecAboutVec, CarFXRenderer)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-fxbp.
## Evidence
.notes/reports/ORACLE-W3-FX.md: Fx root files done (wave3_fxs_oracle_test); not started: Fx/Blueprints (A 26, B 4 — the per-prim
info blueprints: emission rate, size, colour, rotation, flat, noise, wind, jitter, underwater… key-frame interpolation over a particle's
life), plus Fx_c::RotateVecAboutVec and the three CCarFXRenderer functions (need Fx.cpp / CarFXRenderer.cpp linked into the target).
## Task
Extend wave3_fxs_oracle_test: rows for these with real FxInfo key-frame fixtures (interp info with 1..N keys, times inside and outside
the key range, edge values); fix every mismatch (one commit per fix, asm address and pre-fix failure count in the subject) or skip with
reason. Target < 5 min. HARD budget: under 250k tokens, targeted reads only.
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time.
