# ASM-S14b (23 TaskSimple* leftovers of S14) - 2026-10-28
Result: 17 compared (8 FIXED in 8 commits, 9 MATCH, 0 UNSURE), 6 NOT REACHED (all CTaskSimplePlayerOnFoot except MakeAbortable; token budget). Table: S14b.result.tsv.
Fixes (exe range, one line each):
- d0d1acd2 SetPedAsAutoDriver::ProcessPed (0x649059-0x64907E): bEngineOn = !bEngineBroken (port stored bEngineBroken).
- e7aa3233 PlayerOnFire::ProcessPed (0x63368C-0x633783): scream when health != 0 or (zero and event not yet added), silent true when zero+added; m_bAddedDamageEvent set after Add (0x63371D; port never set it, event re-added every frame).
- 010de470 SitIdle::ProcessPed (0x637A9F-0x637CBB): lookTime 3000..8000 (port ..5000); cone dot (z,y,x) before the 25% roll (port rolled first); forward-rand then right-rand sequenced.
- 96dace32 SitIdle::StartAnim (0x631266-0x631272): StartUnguarded (SYSTEMIC_TODO; guarded Start left the timer off for a negative duration).
- 81e486b2 PickUpBike::ProcessPed (0x64D91A-0x64D99F): null vehicle returns true (port dereferenced), only the 4 align anim ids, threshold 0.4667 (port 0.47).
- d6f20998 Jump::StartLaunchAnim (0x67D7F6-0x67D85D): steep slope decides by the dot alone; IsPlayerAllowedToGoInThisDirection only on non-steep ground.
- 74ac10e0 Jump::CheckIfJumpBlocked (0x67D5C7-0x67D63E): probe z = ((pos.z+fwd.z*.25)+center.z)+.25 (port added center.z+.25 as one term).
- cd97d3fd Jump::Launch (0x679CB3-0x679FF9): slow test mag2 < hs*hs (port hs); footprint shadow front=fwd.xy*0.26, side=RIGHT.xy*0.14 (port fwd*0.2*0.26 / fwd*0.14), z=(v.z-0.1)+v1.z; unsigned death counter.
MATCH: Jump::ProcessPed, PickUpBike StartAnim/SetPedPosition, PlayerOnFoot::MakeAbortable, Say::ProcessPed, ShakeFist x2, SitDown x2. SitIdle RandomBool(5/25) equals the exe form (rand&0xFFFF)/32768*100 < 5/25 for every rand value.
Consumers: all fixes are inside the tasks' own ProcessPed/StartAnim; no shared state or signatures changed.
Gate: Release build OK (1186 objects); wave2_oracle_test (StandaloneRW) 1142 functions PC24 0/0 before (origin 4c4a844d) and after; soak 3 min NOVSYNC RESULT: OK (0 errors, 43900 frames). Dumps and logs: C:/gta-scratch/asm-s14b/.
