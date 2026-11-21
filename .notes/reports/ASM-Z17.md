# ASM-Z17: link check of the pushed tip, review of the Z16 fixes, oracle-only gameplay rows (oracle: gta_sa_compact.exe via exedis)

## A. Link of origin tip (b05e000b1)
`ninja -n` in the StandaloneRelease tree (GTASA_STATEHASH=OFF) at the origin tip reported no work (the 06:49 link did include the Z16 fixes). After the Z17 source fix the tree was rebuilt under buildslot.2 (owner Z17): PedGeometryAnalyser.cpp recompiled, gta_reversed.exe linked, exit 0.

## B. Independent review of the Z16 fixes (C:/gta-scratch/review_results.tsv)
| sha | function | verdict |
|---|---|---|
| 47cb7e405 | CPlayerPed::GetWeaponRadiusOnScreen (0x609CD0) | CONFIRMED (fxch / fmul st2 / fmulp = A * (rangeProg * accuracyProg)) |
| fd6198ad1 | CTaskComplexFleePoint::SetFleePosition (0x65B7B0) | CONFIRMED (x, y, z, safeDist compared; scream byte always stored) |
| d2e31e679 | CTaskSimpleCarWaitForDoorNotToBeInUse::SetPedPosition (0x646CC0) | CONFIRMED (null m_veh skips) |

## C. Spot-check of ten Z16 MATCH rows (flags / several float terms)
HitHurtsPed, SetFiringRateMultiplier (clamp, NaN kept, ftol), CTaskSimpleFight::IsComboSet / IsHitComboSet, KillPedOnFootMelee::CalculateSearchPositionAndRanges, MedicTreatInjuredPed::CalcTargetPosWithOffset (sum order and 0.5 scale), CEventKnockOffBike::From (bit-0 flag merge, field map), CCheat::MoneyArmourHealthCheat, CanSunbathe weather ids, CPlane::IsAlreadyFlying. No miss found.

## D. Oracle-only rows compared
Result: .notes/asm_sweep/Z17.result.tsv. 1 FIXED, 1 UNSURE (two x87-audited CCarCtrl helpers not re-derived), the rest MATCH.

| fix | exe | what |
|---|---|---|
| CPedGeometryAnalyser::LiesInsideBoundingBox | 0x5F3880 | bounding-sphere distance is summed (dz^2 + dy^2) + dx^2 (the port summed x, y, z) and a NaN distance returns false (fcompp / test ah,0x41 / jne) |

Compared MATCH: CWanted (ReportCrimeNow with the crime jump table and all constants decoded, RegisterCrime*, Cheat/NoDrop/Parole, RemoveExcessPursuitCops, CanCopJoinPursuit x2), CWeapon type predicates and range multiplier (byte tables decoded), CGarage / CGarages (open, arrested-or-died table, mod type, touching test, target car), CPedGeometryAnalyser hit-side / dirs / centre / segment planes / blocking target, CCarCtrl weave-through-peds/objects and vehicle of interest, CPedGroupMembership, CCover / CCoverPoint, CPopulation sex predicates, CPed IsPlayer / IsAlive, CPedIntelligence helpers, CTaskSimple* flag setters (ped bits 21, 22 at 0x46C, bit 9 at 0x478), CStats converters, CDamageManager, CExplosion / CFireManager getters, CPhysical moving list.

Not compared: CCheat vehicle-spawn / skill cheats, Interior_c helpers, vehicle component-rule helpers, Partner*::GetPartnerSequence, CCarCtrl::FindGhostRoadHeight / FindPercDependingOnDistToLink (UNSURE).
