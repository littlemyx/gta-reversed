# ORACLE-W3-RENDER — Renderer, PostEffects, GrassRenderer, 2dEffect classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-render.
## Evidence
Coverage JSON: Renderer.cpp 10, PostEffects.cpp 9, GrassRenderer.cpp 9, Plugins/TwoDEffectPlugin/2dEffect.cpp 9 uncovered A/B —
which entities get into the render lists (LOD/visibility decisions, sector scanning), post-effect parameters (colour filter, heat haze,
night/IR vision maths), grass density/placement maths, 2D effect (lights, particles, attractors) data helpers. Lessons: ORACLE-W3-VIS.md,
ORACLE-W3-CARENV.md, ORACLE-W3-IK.md (RW maths), ORACLE-W3-FX.md (compare only the fixture block), TRAP-AUDIT.md (W2_PORT_FAULT_IS_DIFF=1;
link callee TUs). Note X87-TRIG flagged CPostEffects ImmediateModeFilterStuffInitialise / SetupBackBufferVertex using std::bit_ceil
where the exe uses ln/pow (.notes/reports/X87-TRIG.md) — check.
## Task
Rows for these A+B functions; RW calls recorded by stubs; fix every mismatch (one commit per fix, asm address and pre-fix failure count in
the subject) or skip with reason. Target < 5 min. HARD budget under 250k tokens; partial landing + state file at the cap is fine.
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time.
