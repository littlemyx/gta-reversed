# SYSTEMIC-1 — three systemic deviations found by the ASM sweep

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh systemic-1 on origin/reverse/interior_c.
## Findings to resolve (reports in .notes/reports/ASM-*.md)
1. CTimer::GetTimeStepInMS() (ASM-S12): the port rounds to float after the first multiply; the exe keeps x87 extended precision through
   ftol. At timeStep == 1.0 the exe yields 19 ms, the port 20 (verify the claim with the exe asm of 3+ callers). ~25 callers use
   (uint32)GetTimeStepInMS(). Find the exe pattern (inlined at each caller?) and make the port produce the same integer for all inputs —
   an oracle row over timeStep values (incl. 1.0, 0.5, 2.0, denormal-free range) against an exe caller is the proof.
2. CVector operator/(float) (ASM-S26b): Vector.h:255 divides; the exe's operator/ (0x4119D0) multiplies by 1/x. BEFORE changing the
   operator, check a sample of >= 10 exe call sites of vector division (exedis): if they call 0x4119D0 or inline fld1/fdiv + fmul, the
   operator change is right; if some inline a plain fdiv per component, change only the operator semantics that match and list the
   exceptions. Same question for CVector2D. Physical.cpp has ~15 `/ m_fMass`-style sites.
3. SetDoorOpen((eDoors)X) no-op (ASM-S11): TaskSimpleCarCloseDoorFromOutside.cpp:118 and TaskComplexEnterCar.cpp:730 — check the exe callers
   (it used SetDoorOpen_Component 0x6C2250 / 0x6C21E0 in 0x64DD60 / 0x648180) and fix; grep for other `SetDoorOpen((eDoors)` sites.
## Gate (these touch core headers -> full rebuild)
Release build; run_all_tests on your worktree for the oracle targets that include Vector.h/Timer.h (at least physics, gameplay, game,
wave3_vehicle, wave3_coll, wave3_tasks, wave3_ped): 0 PC24 mismatches excluding NaN-payload; soak OK. HARD budget: stop at 120k by your
estimate (it runs ~30% low). One commit per finding. Never kill by image name; wait on locks; scratch under C:/gta-scratch/systemic-1/.
