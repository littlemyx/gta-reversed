- DONE (SYSTEMIC-2) GetSectorX/Y and the LOD sector helpers: x * 0.02f / x * 0.005f like the exe (247 + 18 inlined sites); oracle row.
- DONE (SYSTEMIC-2) Object.cpp ~326/759/783: no deviation (exe uses the virtual SetIsStatic at 0x5A28FC / 0x59F4FC / 0x59F875).
- DONE (SYSTEMIC-2) CGeneral::DoCoinFlip: no exe helper; 10 sites switched to their own exe form (RandomBit / RandomOverHalf / RandomLowBits1024AtMost512, AE (0,1), BeInShop, RoadRage); 5 sites verified exact.
- PARTIAL (SYSTEMIC-2) CTaskTimer::Start negative duration: exe is mixed. StartUnguarded added; used at FleePoint (2 sites) and UseMobilePhone. LEFT: read the exe of SitAtDesk / SitOnChair, SeekEntity*, FleeEntity, FleeShooting, FollowNodeRoute, GoToPointAndStandStillTimed, SmartFleeEntity, StareAtPed, WalkRoundCar, LeanOnVehicle, Facial, RunNamedAnim, SitIdle (callers with a variable duration) and switch to StartUnguarded where the exe has no `>= 0` test (verified guarded: StandStill, LieInBed, EnterCarAs{Driver,Passenger}Timed).
- CHeli::GetRopeId: reinterpret_cast<uintptr_t> to uint32 instead of NOTSA_PTR_ID32 (portability, ASM-S18b).
- FIX_BUGS audit: Automobile::ProcessEntityCollision had EXE behaviour under FIX_BUGS (not defined in the run build), so it never ran (ASM-S16d). Grep every FIX_BUGS / !FIX_BUGS block and check against the exe which side the exe does.
- Unguarded port fixes: FIXBUGS-AUDIT found 4 deviations that were unguarded fixes commented FIX_BUGS. Same risk in comments "bugfix" (~60) and "BUG:" (~250) where the port changed the behaviour; audit them (FIXBUGS-AUDIT-2).
- Literal division: the port writes x/125, x/20, x/1250 where the exe multiplies by a float constant (0.008f, 0.05f, ...) - ProcessBuoyancy (Ped, Automobile), S20d/S16h. Scan: for every hooked function, port `/ <float literal>` vs exe fdiv/fmul with a constant; switch the verified ones (SYSTEMIC-3).

## X87-POW (from S18e)
- [ ] Exe calls _CIpow (0x822130) where the port uses std::pow (e.g. CPlane::PreRender Vortex blend pow(0.9f, ts)). No x87::pow helper yet; add one (see .notes/reports/X87-TRIG.md) and grep all std::pow/powf sites.

## IsPointInSphere sum order (from S02b)
- [ ] Exe inlines the sphere test as (dz^2+dx^2)+dy^2 with NaN counting as inside (TaskComplexDestroyCarMelee::CreateFirstSubTask 0x62DB20); the port helper IsPointInSphere sums x,y,z. Remaining callers incl. DestroyCarMelee CreateNextSubTask/ControlSubTask (not in any slice list) need an exe check per site.

## Double literals in float compares (from S05c)
- [ ] Exe compares against float constants (0.119999997f, 0.200000003f) where the port wrote double literals 0.12 / 0.2 (KillCriminal::ControlSubTask). Scan source/game_sa for comparisons of float expressions against unsuffixed literals (like litdiv_scan.py) and check each against the exe constant.

## Runtime globals vs literals (from S07b/S15b)
- [ ] Exe reads .data globals where the port has literals: flee tolerance 0xC18CF0 (fixed in S07b), gun flash duration 0x8D2E88 (init 250, port literal 250). Check whether any exe code writes these globals; if yes, port them as globals.
