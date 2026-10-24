# MAC-P4-REV: line-by-line review of MAC-P4 (P-4a..g, 98a8a26b..2be44cad) — 2026-10-24

Verdict: **no clear mistakes found, no source change made.** Every hunk of the 7 commits (84 files, +172/-156) was read (`git show -U0` per commit, ~820 diff lines).
Debatable items at the end. Wine A/B on 12 extra TUs: 12/12 identical.

## 1. MSVC (x86) semantics unchanged — per class
| commit | check | result |
|---|---|---|
| P-4a (32 lines, 28 files) | pure signature change `unsigned/uint32` -> `size_t` on `operator new`; bodies use `size` only to forward to `CMemoryMgr::Malloc(size)` / `GetMem(size)`; `size_t` == `unsigned int` on x86. Hook lines `RH_ScopedOverloadedInstall(operator new,... void*(*)(unsigned))` (Ped.cpp:77-78) stay valid on x86 (same type) and are `((void)0)` in portable. Leftovers not touched: `Core/LinkList.h:24` (`unsigned`, non-standalone/dead branch), `MemoryMgr.cpp:460` global `operator new(uint32)` (dead, known in P4_PLAN) | OK |
| P-4b (15 sites) | the original call compiled on MSVC only if both args had ONE type, so the deduced T is fixed by the operand that is not a literal. Checked each: AEPedSpeech (`0u`,`20u` -> unsigned), AEVehicle (`HORN_*` size_t, `m_HornCounter` uint32 -> unsigned), AEAudioHardware (`10u`), Birds (size_t/uint), Entity.cpp, Vehicle.cpp (`0u`), GangInfo (`1u`, size_t), LoadingScreen (`1u`), Population (`0u`), RoadBlocks (`(uint32)` cast), WaterCreature (uint32 vs `GetNumItems`), World (`255u`) all deduced `unsigned int` == `size_t`. `Pool.h:311` `<int32>`: `m_LastFreeSlot` is `int32`, `GetIndex` returns `ptrdiff_t` == `int` on x86 -> `int32` correct. No int/float/uint8 promotion hazards: no site is float/uint8 | OK |
| P-4c | macro `static_cast<uint32>(reinterpret_cast<uintptr_t>(p))`: on x86 `uintptr_t` == `unsigned int`, same as `(uint32)p`/`reinterpret_cast<uint32>(p)`/`bit_cast`. Result types preserved at every site: int32 sites keep `static_cast<int32>(...)` (Vehicle.cpp:6532, RunningScript.cpp:375, ObjectCommands:53, StareAtPed x2), uint32 sites stay uint32; `+ rand()`, `+ C`, `+ id`, `/0x70`, `& 0xFFF` precedence safe (macro is one cast expression). `HighlightImportantArea` has only uint32-id overloads, no overload shift | OK |
| P-4d | `(void*)(uintptr_t)x` = `(void*)x` on x86; `(uint32)(uintptr_t)data`, `static_cast<eAtomicComponentFlag>(uintptr_t)` (was `bit_cast`, enum is 4 bytes) same bits; MemoryMgr `~uintptr_t(align-1)` == `~(align-1)` on x86 (both unsigned 32); `(void*)Func` = the implicit conversion MSVC did | OK |
| P-4e | pointer replaces `int32` in same 4 bytes on x86; `VALIDATE_SIZE(CAttractorScanner,0x90)`, `(CPlane,0xA04)` unchanged on x86 (no-ops in portable). All readers/writers grepped: AttractorScanner.cpp (31,32,214,215,225-236; leftover `(C2dEffect*)field_40[i]` casts are redundant, harmless), Plane.cpp:199, Vehicle.cpp 7012-7058 (5 sites, all converted) | OK |
| P-4f | `ptrdiff_t{0}` (other branch `GetIndex()+1` is ptrdiff_t/int), `size_t{0}` (other `return bytesRead` size_t), `size_t modelSizeSectors` (only passed to `size_t&`/compared), `enum E : int` == MSVC default underlying type (mangling `W4` unchanged); all forward declarations of the two enums (Weapon.h, TaskSimpleBeHit.h, eCarWheel.h) updated, no other bare `enum ePedPieceTypes;`/`eCarPiece;` left | OK |
| P-4g | portable-only (winapi_compat.h, wintypes.h, snap_census.sh) + `Dummy.cpp` instantiation moved inside the unnamed namespace (A/B identical) + `OALBase::trashCan` out of line. `OALBase.cpp` is NOT in the Win32 StandaloneRelease compile_commands (OpenAL not built there) -> no x86 effect; definition is constant-initialised (`vector{}`), no init-order hazard | OK |

Line counts: numstat insertions == deletions everywhere except Base.h (+6, macro + comment; `__LINE__`-sensitive code in Base.h: none, A/B identical), OALBase.cpp (+2), winapi_compat.h (+2), wintypes.h (+4), snap_census.sh (+2). Matches the report. No logic edits beyond the class recipes.

## 2. 64-bit honesty
### NOTSA_PTR_ID32 — 65 expansions on 60 lines (the report said 58; five lines carry two expansions). Callees: `RegisterCorona/UpdateCoronaCoors(uint32 id)`, `CRopes::FindRope/RegisterRope(uint32)`, `RegisterStreak(uint32)`, `CWanted::RegisterCrime[_Immediately](uint32 IdKey)` -> `CrimeBeingQd::m_nCrimeId uint32`, `C3dMarkers::PlaceMarkerCone(uint32)`, `Shadows::m_nId uint32`, `CWaterCannon::m_nId uint32`, `CTheScripts::HighlightImportantArea(uint32)`, `RemoveEntityFromList(int32)`. All by value, stored as uint32, compared only. Nothing converts back to a pointer anywhere (grepped for pointer casts from those fields). Consistency: all computations of the same key were migrated together (script highlight id: PlayerCommands:57/72, RunningScript:375/1834, Group09_12.hpp:44; rope id: Object.cpp 8 sites + Vehicle.cpp:429 + Vehicle.h GetRopeID + Heli:1600).
| file:line(s) | purpose | verdict |
|---|---|---|
| AngledRect.cpp:43 | marker id + rand() | compare/hash only OK |
| Entity.cpp:502,532,555,570 | corona ids | OK |
| Entity.cpp:1056 | srand seed | OK (seed; arm64 value differs, nondeterministic anyway) |
| Entity.cpp:1409 | tree-in-wind phase `& 0xFFF` | OK (low bits preserved) |
| Entity.cpp:2000,2066,2097,2123,2212,2232 | corona/light ids `+C` | OK |
| Object.cpp:708,714,720,726,1303,1395,1407,1412,1416 | rope find/register | OK, FindRope/RegisterRope pair consistent |
| Automobile.cpp:6694 | WaterCannons::UpdateOne id | OK |
| Heli.cpp:1281,1600 | corona index, rope id | OK |
| Plane.cpp:1343,1350 | streak ids | OK |
| Vehicle.cpp:429, Vehicle.h:751 | rope id (`flags+1`) | OK, bug-compatible (`this+1` vs `&m_nFlags+1` as in the original) |
| Vehicle.cpp:6532 | shadow/corona id `&m_matrix + 2` | OK |
| EntryExitManager.cpp:119 | marker cone id | OK |
| EventDamage.cpp:218,224,230; EventKnockOffBike:102; EventVehicleDamage:71; EventVehicleHitAndRun:41,43; TaskSimpleEvasiveDive:71,72 | crime victim id | OK (queue dedupe key only) |
| FireManager.cpp:455,484 | corona id of strongest fire | OK |
| Pickups.cpp:194,213,249,268 | corona ids | OK |
| CustomBuildingDNPipeline.cpp:162 | phase from atomic address | OK (nondeterministic, as exe) |
| ObjectCommands.cpp:53 | `RemoveEntityFromList(handle)` gets object address | OK for the cast; the original BUG (address is not a handle) is preserved, see debatable 2 |
| PlayerCommands.cpp:57,72; Group09_12.hpp:44; RunningScript.cpp:375,1834 | script-area highlight id `S + IP` | OK |
| Shadows.cpp:2005,2040; SpecialFX.cpp:95; TrafficLights.cpp:202,266 | shadow / streak / corona ids | OK |
| TaskComplexStareAtPed.cpp:97,112 | `LookAt(.. (int32)&gDefaultTaskTime ..)` duration | bug-compatible but see debatable 1 (value is the low 32 bits of an address on arm64: arbitrary, possibly negative) |
Collision risk: two live objects with equal low 32 bits need addresses differing by a multiple of 4 GiB; pools are contiguous allocations << 4 GiB. Accepted.

### `(void*)(uintptr_t)x` / `(uintptr_t)data` round trips (P-4d, 21 sites) — all integer-in/integer-out, no pointer ever stored
| site | writer -> reader | verdict |
|---|---|---|
| Entity.cpp:913,914 -> `SetCompAlphaCB` `(RwUInt8)(uintptr)data` | alpha int32 (0..255) | OK |
| Vehicle.cpp:1981,1982 -> `SetVehicleAtomicVisibilityCB` (uintptr_t -> enum) | uint32 state; other writers `(void*)ATOMIC_OK` (Vehicle.cpp:2609,2610,6107,6108, enum->void*, pre-existing, int->pointer warning only) | OK |
| VehicleModelInfo.cpp:539 -> `SetEnvMapCoeffAtomicCB` -> `SetEnvMapCoeffCB` `(int32)(uintptr_t)data` | int32 coeff, sign extends then truncates back exactly | OK |
| VehicleModelInfo.cpp:923 `HideAllComponentsAtomicCB` | writers Automobile.cpp:2677, QuadBike.cpp:90 pass small flag constants | OK |
| RwHelper.cpp:263,270,276,282 | RwTextureFilterMode via `(unsigned)` | OK |
| VisibilityPlugins.cpp:310,316,1077,1084 | int32 flag id; sign extends then truncates back | OK |
| ColModel.cpp:100 | `std::align` on a fake address that is a FILE OFFSET (< 4 GiB) | OK |
| MemoryMgr.cpp:133,341 | `(uintptr_t(p+align) & ~uintptr_t(align-1))` full-width mask, correct on 64-bit (the old uint32 mask would have cut the pointer) | OK, improvement |
| PedModelInfo.cpp:40, WeaponModelInfo.cpp:32 | function pointer as callback data `(void*)RenderPedCB` | OK |
| CTeleportDebugModule.cpp:314,374 | imgui settings handler version through `void*` (intptr_t) | OK |
No remaining `(int32)data` / `(uint32)data` / `bit_cast<..>(data)` readers left (grepped).

## 3. Wine cl.exe A/B on 12 TUs not in MAC-P4's 71
Missed touched TUs: only `OALBase.cpp`, which is not in the Win32 build (not in compile_commands.json). A = 98a8a26b~1 (0635a914), B = 2be44cad, fresh `git archive` trees + `source/libs`, flags of StandaloneRelease (MAC-P4's driver drv.py/cmp.py/obj_code_sig.py, NOTSA_SIG_NORM_STRINGS=1), scratch /private/tmp/claude-501/p4rev/ab.
Weapon, TaskSimpleBeHit, CarCtrl, Coronas, Wanted, Ropes, QuadBike, PlayerPed, Camera, Trailer, CopPed, Collision: **12/12 code+relocs identical and data identical** (they include the changed headers Vehicle.h, Ped.h, Plane.h, Weapon.h, Pool.h, Base.h, Group09_12-independent). Combined with MAC-P4's 71 (68 identical + 3 only differing in a dead `bit_cast` COMDAT) this covers every touched .cpp and a spread of header users. No stray processes left (ps checked).

## 4. Debatable (not changed)
1. TaskComplexStareAtPed.cpp:97,112: on arm64 `static_cast<int32>(NOTSA_PTR_ID32(&gDefaultTaskTime))` is an arbitrary (maybe negative) look-at duration. Win32-neutral fix would be `#ifdef NOTSA_PORTABLE` -> `gDefaultTaskTime` (9'999'999). Host decision (already listed by MAC-P4).
2. ObjectCommands.cpp:53 passes the object address as script handle (never matches); same remark.
3. Report/plan say "58 sites" for NOTSA_PTR_ID32; actual 65 expansions / 60 lines (+1 define). Cosmetic.
4. Remaining unflagged int->pointer casts of enums/ints into `void*` (`(void*)ATOMIC_OK`, `(void*)FALSE` BrightLights.cpp:63, `(void*)2` QuadBike.cpp:90): compile with a width warning only, value-correct (zero/sign extension of small values). Could be spelled `(void*)(uintptr_t)` for warning hygiene.
5. P-4e changes the 64-bit layout of CAttractorScanner/CPlane (pointers widen). Intended; only relevant if either struct is ever (de)serialised (not the case: grepped, in-memory only).
