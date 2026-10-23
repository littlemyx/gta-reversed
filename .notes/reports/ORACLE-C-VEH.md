# ORACLE-C-VEH report

Agent ORACLE-C-VEH (rules v4), worktree C:/gta-wt/oracle-c-veh (branch oracle-c-veh), started 16:34 WEST on 2026-10-23. First class-C wave: game_sa/Entity/Vehicle functions whose exe closure has indirect (vtable) calls.

## Result

| | before | after |
|---|---:|---:|
| game_sa/Entity/Vehicle covered | 215 | **239** (+24) |
| uncovered class C in Entity/Vehicle | 86 | **62** |
| exe-oracle coverage, whole game (tool, same tree) | 4786 | **4810** |
| `wave3_vehicle_oracle_test` | 85 rows | **113 rows**, PC24 **0** mismatches, exit 0, **2 min 10 s** (target < 5 min) |
| port bugs fixed | | 2 (below) |

## Helper: vehicle fixture with real vtables (tests/standalone/wave3_oracle_veh.inc)

* `W3FuncCase(r, d, sz, ents, portFn, exeFn, sub, dtor, vtCls)`: the new last argument `vtCls` (class name string, e.g. "CAutomobile") switches the helper on. Before the **exe** call it writes the exe's vftable
  (`RH_ScopedVirtualClass` VA, table `w3::kVt`) into word 0 of the vehicle, into the embedded `CAEVehicleAudioEntity` (`offsetof(CVehicle, m_vehicleAudio)`, exe vftable 0x862CEC) and into the model info (`g_mi`, exe 0x85C5C8);
  before the **port** call the port's vftables. After each call the words are cleared, so the object compare ignores them (no mask needed). Virtual callees run their real code on each side (port callee vs exe callee);
  a callee that leaves the closure (renderer, RW) faults on the side that reaches it: both fault = agreed and skipped, port-only fault at an unresolved symbol (EIP 0x400000) = skipped, any other port-only fault = mismatch (W2 fault policy).
* The port's vftable is reached without constructing an object: `W3_PORT_VT(Cls)` declares `extern "C" const uint32_t w3vt_Cls[]` plus `/alternatename:_w3vt_Cls=??_7Cls@@6B@`. `TestW3VtInfra()` prints the port vftables, lists any slot
  that is an unresolved symbol (none today) and checks that the exe vftable slots point into .text.
* Base-class rows (`CVehicle::`, `CPhysical::`) pick a random vehicle class per case (`kVt`, 10 classes), use that class's `sizeof` as the compared region and set `m_nVehicleType` (vehicle and model info) to the class's type.
* `gen_w3_veh.py` emits the extra `vtCls` argument for every class-C row; skip table `tests/standalone/tools/w3_skip.tsv` (73 entries added, each with its reason). Regenerate:
  `python -I tools/standalone/oracle_coverage.py --repo . --exe gta_sa_compact.exe --json sites.json`, then `python -I tests/standalone/tools/gen_w3_veh.py . sites.json tests/standalone --skip tests/standalone/tools/w3_skip.tsv`.
* Reuse for other class-C families (peds, tasks, models): table of exe vftable VA + `??_7` alternatename per class, written around the two calls. Link the callee TUs: `game_sa/Door.cpp` was missing and made IsDoorReady fault at 0x400000.
  To find the unresolved callee of a "port-only 0x400000" row: map the `ret=` address of its TRAPAUDIT line to a function with the linker .map, then read the call instruction before that return address in `dumpbin /disasm` of the object
  (set MSYS_NO_PATHCONV=1 in Git Bash, `/symbols` is otherwise turned into a path).

## Port bugs fixed (one commit each)

| function | exe | bug | pre-fix |
|---|---|---|---|
| `CVehicle::ProcessDelayedExplosion` 0x6D1340 | 0x6D1344 `movzx di,[this+0x4DE]`, `call _ftol`, `cmp ecx,eax / ja` | step truncated to int16 and a signed clamp; exe: 32-bit step, unsigned compare, timer = timer > step ? timer - step : 0 | 22/60 PC24 |
| `CAutomobile::BlowUpCarsInPath` 0x6AF110 | 0x6AF145 `test [this+0x868],0x20 / je leave` | test inverted (port left when `bTankExplodesCars` is set; the constructor 0x6B0B6B sets it for every vehicle); no caller in the exe or the port (dead code) | 19/60 PC24 (port-only access violations) |

Consumer audit: neither fix changes a layout or a shared value; `m_wBombTimer` is only touched by ProcessDelayedExplosion and the bomb setters (plain timer), `bTankExplodesCars` is read only by BlowUpCarsInPath. Soak: see the PHASE2_STATUS line.

## Skipped / not constructible (w3_skip.tsv has the per-row reason)

* every case faults in both exe and port (RW clump / frame / atomic layout or renderer): Render x5, SetModelIndex x4, SetupModelNodes x2, SetUpWheelColModel, SetupSuspensionLines, CustomCarPlate_*, Set/ClearWindowOpenFlag, upgrade create / replace / add / remove,
  SetupUpgradesAfterLoad, SetComponentAtomicAlpha, heli search-light cone, MonsterTruck ProcessControl / ProcessSuspension, Trailer::ProcessControl, QuadBike::Fix, StopNitroEffect.
* **kill the test process** (unhandled fault after a first agreed fault; cause not analysed): CAutomobile::Fix / ProcessAutoBusDoors / CloseBoot / Teleport, CBike::CalculateLeanMatrix / FixHandsToBars / Teleport, CBoat::Teleport,
  CTrain::Teleport, CVehicle::UpdateWinch / UpdateTrailerLink. These are the gameplay-relevant follow-ups (ORACLE-C-VEH-state.md).
* `CVehicle::RemoveWinch` (unused, the port asserts by design), `CAutomobile::GetComponentWorldPosition` (garbage component index trips the port std::array assert), lights functions (CMatrix& argument, not generated).

## Gates

wave3_vehicle_oracle_test: exit 0, PC24 0, PC53 14 (the same 14 informational x87 artefacts as before the wave). Release build of the worktree + soak: PHASE2_STATUS line.
