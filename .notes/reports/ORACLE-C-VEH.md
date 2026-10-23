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

## Continuation 2 (ORACLE-C-VEH-2, rules v4, 17:22-, worktree C:/gta-wt/oracle-c-veh-2)

### The 11 process-killing rows: cause = fixture (SafeSEH), not the port
cdb (`sxd av;g;k`) on CVehicle::UpdateWinch: the port's first fault (0x400000, unresolved CRopes symbol) was an agreed/skipped fault, the second was the EXE side:
`CRopes::RegisterRope 0x556B40 -> ... CPhysical ctor 0x542260 -> 0x54F4C0` read the uninitialised global matrix-link list (`mov ecx,[eax+0x50]`, eax = 0). The exe
function has a C++ EH frame (`push 0x83D2C8; push fs:[0]`), and a fault unwinding through an fs:[0] record whose handler is in the exe image (not in the test's
SafeSEH handler table) raises STATUS_INVALID_EXCEPTION_HANDLER (c00001a5) as an unhandled second-chance exception -> the process dies. Every killer row has such a frame
(CAutomobile::OpenDoor 0x6A6AE0 -> 0x847F98, CBike::CalculateLeanMatrix 0x848258, SpecialEntity 0x848718 ...). Fix: `/SAFESEH:NO` on wave3_vehicle_oracle_test (source/CMakeLists.txt).
With it all 11 run. A port-only fault where the exe survived did exist (ProcessAutoBusDoors, below) and was a real port bug.

| row | now | note |
|---|---|---|
| CVehicle::UpdateTrailerLink | 0/60 PC24 | generated row passes (16 of 120 cases fault in both: skipped) |
| CBike::FixHandsToBars | 0/60 | generated row passes (60 of 120 fault in both) |
| CBike::CalculateLeanMatrix | hand row, **fix**, 0/600 | see fixes |
| CAutomobile::ProcessAutoBusDoors | hand row, **fix**, 0/600 | see fixes |
| CAutomobile::CloseBoot | skipped | with a shared fake frame (matrix at +0x10 is the same layout in librw and RW 3.7) every case faults port-only inside CMatrix::UpdateRwMatrix at an unresolved symbol (the RwMatrixUpdate shim TU is not linked) |
| CVehicle::UpdateWinch | skipped | Ropes.cpp linked now; the port asserts on garbage rope segment counts (the exe writes past the array), and the exe constructs a CObject hook through the pools/global lists: needs a rope + sector fixture |
| CAutomobile::Fix | skipped | every case faults in both (RpClumpForAllAtomics on the null clump) |
| CAutomobile/CBike/CBoat/CTrain::Teleport | skipped | the port calls CWorld::Remove/Add whose TU is not linked (0x400000); the exe walks uninitialised sector lists |

### Port bugs fixed (one commit each; all pre-fix counts from the new rows)
| function | exe asm | bug | pre-fix | post |
|---|---|---|---|---|
| `CAutomobile::ProcessAutoBusDoors` 0x6A38A0 | 0x6A38DD / 0x6A3966 / 0x6A39FE / 0x6A3A28 `test [esi+0x486],mask / jne skip`; 0x6A392A `mov [esi+0x870],[0xB7CB84]` | the getting-in test had the wrong polarity (port moved the door when the bit was SET), and for a closed left-front door the port wrote m_nBusDoorTimerStart = 0 instead of m_nBusDoorTimerEnd = time | 67/600 | 0/600 |
| `CBike::CalculateLeanMatrix` 0x6B7150 | 0x6B71E1..0x6B7253 | position += (up * minZ) * float(1 - cos) with x/y rounded to float and z unrounded (port: (up * k) * minZ) -> 1 ulp | 11/600 | 0/600 |
| `CVehicle::SpecialEntityPreCollisionStuff` 0x6D6640 | 0x6D668F `test byte [esi+0x43],1` | tested physicalFlags.bSubmergedInWater (bit 8) instead of bDontCollideWithFlyers (bit 24: set by CHeli / CPlane) -> wrong collision exemptions for submerged vehicles and flyers | 112/3000 | 0/3000 |

Consumer audit (no layout/meaning change in any of the three): m_nBusDoorTimerEnd/Start only in ProcessControl (timer decrement), SetBusDoorTimer and this function; m_mLeanMatrix only through the matrix by rider/render code;
bDontCollideWithFlyers read at Collision.cpp:3159, Physical.cpp:2147/4149 with the same meaning. Soak: see the PHASE2_STATUS line.

### Numbers
| | before | after |
|---|---:|---:|
| game_sa/Entity/Vehicle covered | 239 | **244** |
| uncovered class C in Entity/Vehicle | 62 | **57** |
| whole game (oracle_coverage.py) | 4810 | **4815** |
| wave3_vehicle_oracle_test | 113 rows | 118 rows, PC24 **0**, PC53 14 (same informational x87 artefacts), exit 0, 2 min 35 s |

New hand rows live in tests/standalone/wave3_oracle_veh_hand3.inc (ProcessAutoBusDoors compares the end time relative to each side's clock and nulls the car nodes so OpenDoor returns at once;
CalculateLeanMatrix zeroes the 4 padding words of the matrix temp, which hold stack garbage in the exe and the debug fill in the port; SpecialEntityPreCollisionStuff uses pool ped/vehicle/object colliders
with the links between the two set in a share of the cases and a garbage model info for the collider).
Lesson: a "port faulted while the exe did not" mismatch in a skipped-looking row pointed at two real bugs - read the exe asm of the whole function once the row diverges.
Still open: CloseBoot/Teleports/UpdateWinch/Fix (above), lights (DoVehicleLights/DoHeadLight*), the RW-bound rows (clump + frames), TrainHitStuff, CopyObjectsCB, CustomCarPlate_BeforeRenderingStart.
