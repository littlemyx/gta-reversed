# ORACLE-W3-GAMEPLAY state (for a continuation agent)

Landed: see ORACLE-W3-GAMEPLAY.md (121 rows, 8 fix commits, root files covered 855 -> 976, whole game 3037 -> 3158).
Worktree recipe: `tools/standalone/windows/new_worktree.sh <id> --preset StandaloneRW`, then `cmake -S . -B build/StandaloneRW -DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON`. Build: `tools/standalone/build_locked.sh ninja -j4 -C build/StandaloneRW wave3_gameplay_oracle_test` (background; one TU `wave1_oracle_test.cpp` holds all rows, ~5 min). Run: `RW_EXE_ORACLE=C:/Users/littl/github/gta-reversed/gta_sa_compact.exe build/StandaloneRW/source/wave3_gameplay_oracle_test.exe [row substring] [-v] [W2_ASSERT_STACK=1]`; per-row lines are `Class::fn 0xADDR  N [true:K]  PC24 <mismatches> ...`, "NOT ORACLE-ABLE" = every case faulted in both.
Regenerate rows: `py -X utf8 -I tools/standalone/oracle_coverage.py --repo . --exe gta_sa_compact.exe --json <sites.json>`, then `py -X utf8 -I tests/standalone/tools/gen_w3_gp.py C:/<wt> <sites.json> tests/standalone --classes A,B --skip tests/standalone/tools/w3_gp_skip.tsv`. Exe asm: `py -X utf8 -I tools/standalone/exedis.py --exe <exe> --repo . 0xADDR` (data bytes: read the PE sections yourself, exedis shows code only). `oracle_coverage.py --out .notes/ORACLE_COVERAGE.md` overwrites the history bullet: restore it from git and append the new entry.

## Remaining
1. Skipped, every case faults in both (reasons in tools/w3_gp_skip.tsv): CGangWars::AddKillToProvocation 0x443950 / ClearSpecificZonesToTriggerGangWar 0x443FF0 / SetSpecificZoneToTriggerGangWar 0x444010 (CTheZones tables, CTheScripts), CStats::DisplayScriptStatUpdateMessage 0x55B980 and PopulateFavoriteRadioStationList 0x558EC0, CPedStats::GetPedStatType 0x6088D0, CGarage::IsPlayerOutsideGarage 0x448E50 (needs CWorld::Players[0].m_pPed + FindPlayerVehicle), NeatlyLineUpStoredCars 0x448330, CStoredCar::StoreCar 0x449760 (vehicle model info + handling), CGarages::Shutdown 0x4471B0 (CAEDoorAudioEntity::Reset), StopCarFromBlowingUp 0x448890 (CAutomobile damage manager), CPickups::TestForPickupsInBubble 0x456450. Use W2_ASSERT_STACK=1 and the .map to find the first fault.
2. Hand rows I removed because they fault in both (source at the end of this file): CPlayerInfo::EvaluateCarPosition 0x56DAD0 (needs matrices on the car / ped: WireWorld is only applied to the garage and pickup classes, add CPlayerInfo), CPedGroupMembership::From 0x5F7FE0.
3. Not generated: constructors CPedGroup 0x5FC150, CPedGroupIntelligence 0x5F7250, CPlayerInfo 0x571920; CPickup::GetPosn 0x4549A0 (CVector return through a hidden pointer); ModifyStringLabelForControlSetting 0x454B70 (global function, no class: DECL note in wave3_oracle_gen_gp_report.txt).
4. Low-information rows (return value constant over the 60 cases, see the `true:N` column): CGangWars::CanPlayerStartAGangWarHere, AttackWaveOvercome, CGarages::IsCarSprayable, FindGarageForObject, CPedGroupIntelligence::IsGroupResponding, GetTaskSecondarySlot, CPedGroups::GetGroupId (nullptr argument). Give them real arguments (a CPedGroup of ms_groups, a vehicle with model info) before trusting them.
5. Class C / S / D functions of the same files (Weapon.cpp 20 D, Pickups 9 C / 7 D, Stats 12 D ...) were out of scope.

## Removed hand rows (source)
```cpp
    Run("CPlayerInfo::EvaluateCarPosition 0x56DAD0", [&](Rng& r, std::string& d) {
        CEntity* car = GEntN(r); CPed* ped = static_cast<CPed*>(GPedN(r)); const float dist = GFl(r); float o1 = GFl(r), o2 = o1; CVehicle* v1 = r.below(2) ? nullptr : static_cast<CVehicle*>(GVehN(r)); CVehicle* v2 = v1;
        return W3GpCase(r, d, "CPlayerInfo", sizeof(CPlayerInfo), {}, [&](void* p) -> uint64_t { static_cast<CPlayerInfo*>(p)->EvaluateCarPosition(car, ped, dist, &o1, &v1); return mix(FBits(o1), (uintptr_t)v1); },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, CEntity*, CPed*, float, float*, CVehicle**)>(0x56DAD0)(p, 0, car, ped, dist, &o2, &v2); return mix(FBits(o2), (uintptr_t)v2); });
    }, 100);
    Run("CPedGroupMembership::From 0x5F7FE0", [&](Rng& r, std::string& d) {
        alignas(16) static uint8_t src[sizeof(CPedGroupMembership)]; FillGarbage(r, src, sizeof src);
        return W3GpCase(r, d, "CPedGroupMembership", sizeof(CPedGroupMembership), {}, [&](void* p) -> uint64_t { static_cast<CPedGroupMembership*>(p)->From(*reinterpret_cast<CPedGroupMembership*>(src)); return 0; },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, const void*)>(0x5F7FE0)(p, 0, src); return 0; });
    }, 40);
```
(`mix` = `(a * 0x9E3779B97F4A7C15) ^ (b + 0x1234567)`, defined at the top of the out-parameter section of TestW3GpHand.)
