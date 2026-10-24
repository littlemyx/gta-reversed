# MAC-P4: remaining 64-bit integer/pointer-width diagnostics (2026-10-24, branch portable/main, pushed to fork)

## Commits (all pushed; 0635a914..2be44cad)
| commit | class | files | what |
|---|---|---|---|
| 98a8a26b P-4a | operator new/delete signature | 28 | `operator new(unsigned|uint32 size...)` -> `size_t` (32 lines). clang rejects other first parameters, which also broke every `new T` of the class ("no matching function for call to operator new") |
| a1793813 P-4b | std::min/max/GetRandomNumberInRange | 13 | explicit template arg = x86 deduced type (`<size_t>`, `<int32>` in Pool.h) |
| 24fd2c04 P-4c | pointer used as 32-bit id | 26 | `NOTSA_PTR_ID32(p)` (Base.h macro) in 58 places (coronas, ropes, streaks, crimes, markers, shadows, srand seeds, script highlight ids) |
| 1741bf0e P-4d | int through `void*` callback data, aligned masks | 10 | `(void*)(uintptr_t)x`, `(int32)(uintptr_t)data`, `uintptr_t` masks in MemoryMgr, function pointer as `void*` |
| d2b188e3 P-4e | pointer in int32 field | 5 | `CAttractorScanner::field_18/field_40`, `CPlane::field_9E0` typed as pointers |
| 3148679d P-4f | scalar typing | 8 | lambda returns `ptrdiff_t{0}`/`size_t{0}`, `size_t modelSizeSectors`, `enum ePedPieceTypes/eCarPiece : int` forward decls |
| 2be44cad P-4g | shims / tooling | 6 | `GetACP` = CP_UTF8, `_BASETSD_H_` (libjpeg INT32), `OALBase::trashCan` out of line, Dummy.cpp explicit instantiation inside the unnamed namespace, `snap_census.sh` rsyncs conan-generated `source/libs` (the 4 imgui "header not found" rows were a census-snapshot artefact) |

## Triage (non-asm, non-R rows of .notes/w0/r3b_after.tsv / p4_before.tsv)
| root class | rows | mechanical? | fixed |
|---|---|---|---|
| `operator new` first parameter not size_t (+ the "no matching operator new" it causes) | 20 + 85 | yes (same type on x86) | all |
| min/max/GetRandomNumberInRange mixed unsigned/size_t | 16 | yes, explicit template arg | all |
| pointer -> uint32 key (never converted back) | ~58 | yes with macro (low 32 bits on LP64, ids only compared) | all |
| int <-> `void*` callback data, `uint32` masks on addresses | ~21 | yes (`uintptr_t`) | all |
| pointer stored in int32 field of an in-memory struct | 10 | yes (field type) | AttractorScanner, CPlane |
| scalar typing (lambda returns, size_t&, forward enums) | 8 | yes | all |
| shim / tooling (GetACP, INT32, OALBase, Dummy, imgui paths) | 11 | yes, portable-only | all |
| pointer in int32 plugin slot / file record / restore table | 6 | NO, design | P4-S2 VisibilityPlugins, S3 VehicleModelInfo restore entries, S4 CKeyArray |
| RAND_MAX 0x7FFF assumption | 1 (+70 silent) | NO, design | P4-S1 (high value) |
| AnimManager sizeof static_assert | 1 | no | P4-S5 |
| UIRenderer DX9 init / SetFocus | 2 | no (W/R) | P4-S6 |
| nested DMI / PType (Replay.h, WaterLevel) | 35 | fixed by MAC-P2k b2bc35cd, not by me | - |
Plan with per-site recipes and consumers: `.notes/P4_PLAN.md` (slices P4-S1..S6 + findings).

## Census (tools/portable/snap_census.sh, PortableMac native)
| | p4_before (HEAD 0c792ad9) | p4_final (HEAD 2be44cad) |
|---|---|---|
| total rows | 320 | 45 |
| F/C inline asm (stream F, not mine) | 36 | 36 |
| P pointer/int width | 101 | 5 |
| other conversion/overload | 88 | 1 (General.cpp RAND_MAX) |
| P integer width / mixed overload | 16 | 0 |
| P size_t operator new/delete | 20 | 0 |
| C syntax + nested DMI + imgui + misc | 25 | 0 |
| W/R (UIRenderer DX9, SetFocus) | 0 visible (hidden behind the missing imgui header) | 2 |
| P layout static_assert (AnimManager) | 2 | 1 |
TSVs: .notes/w0/p4_before.tsv, p4_final.tsv.

## Win32 no-op proof (wine cl.exe x86, no PCH, flags of StandaloneRelease; A = HEAD 0c792ad9 tree, B = A + my files; NOTSA_SIG_NORM_STRINGS=1)
* 71 TUs (every .cpp I touched + users of changed headers: Group09_12.hpp users, operator-new callers CarGenerator/Garage/GangWars/Pools/Remote/SetPiece/Train/Bike/Boat/Population/PedModelInfo, Dummy.cpp): **68 byte-identical (code + relocations + data), 3 identical except one dead COMDAT**: Entity.cpp, Vehicle.cpp, ObjectCommands.cpp lose the unreferenced `std::bit_cast<int/uint/enum, ptr>` out-of-line body (`mov eax,[esp+4]; mov eax,[eax]; ret`) because the call is now `NOTSA_PTR_ID32` / `static_cast<enum>(uintptr_t)`. All referenced code identical. Method of MAC-P2b (scratch /private/tmp/claude-501/p4_ab, drv.py/cmp.py/dd.sh).
* The A/B caught a real difference in my first version: `notsa::PtrId32` as a function template (even `static __forceinline`) is emitted by cl as an unreferenced COMDAT in every user TU (11 TUs differed). Replaced by the macro `NOTSA_PTR_ID32`; re-run identical.
* Edits keep line counts (git numstat insertions == deletions) except Base.h (+5 comment/macro lines; no __LINE__-in-use macro depends on absolute position) and portable-only files.
* Hot files (git log fork/reverse/interior_c --since=2.days): Ped.cpp, Ped.h, Vehicle.cpp, Automobile.cpp, AEPedSpeechAudioEntity.cpp were edited by me on single tokens far from the recent hunks; `git merge-tree --write-tree HEAD fork/reverse/interior_c` after the push: no conflicts. (Entity/Ped/Ped.h gets `enum ePedPieceTypes : int`, Vehicle.h `enum eCarPiece : int` etc.)
* qc.py (clang native syntax check of the working tree, scratch) used for iteration; the census above is the gate.

## Consumer audit
* `NOTSA_PTR_ID32` keys: CRopes::FindRope/RegisterRope, CCoronas::RegisterCorona, CMotionBlurStreaks::RegisterStreak, CWanted::RegisterCrime[_Immediately], C3dMarkers::PlaceMarkerCone, CShadows::StoreCarLightShadow, CWaterCannons::UpdateOne, CTheScripts::HighlightImportantArea, CPickups corona ids, fire strongest id: all take the id by value and compare it; no reader converts it back to a pointer (grep'd the callee side of each). Collision only if two live objects have the same low 32 bits (pools are contiguous and far < 4 GiB).
* `field_18/field_40` (CAttractorScanner): only AttractorScanner.cpp reads/writes them. `CPlane::field_9E0`: Plane.cpp:199 (ctor), Vehicle.cpp (lock-on code, 5 sites).
* `modelSizeSectors` (Streaming.cpp): local, used with CStreamingInfo::GetCdPosnAndSize(size_t&) / GetCdSize.
* enum forward declarations: all other users of eCarPiece/ePedPieceTypes see the same enumerators; MSVC default underlying type is int, so no ABI change.

## Findings handed to the orchestrator (not changed: Win32 behaviour)
1. TaskComplexStareAtPed.cpp:97,112 passes `(int32)&gDefaultTaskTime` (the ADDRESS) as the IK look-at duration; `gDefaultTaskTime == 9'999'999 == 0x98967F` is the exe's immediate -> value was meant.
2. ObjectCommands.cpp:53 `RemoveEntityFromList(bit_cast<int32>(object), ...)` passes the deleted object pointer as a script handle (never matches).
3. Hidden, bigger: RAND_MAX / `rand()` (P4-S1): `GetRandomNumber()` is `uint16`, `RAND_MAX_FLOAT_RECIPROCAL = 1/RAND_MAX` and `>= RAND_MAX/2` tests are wrong on macOS in ~70 places; only one is visible to the census.

## Open items / CONTINUE
* Implement P4-S1 first (portable `rand` shim in portable/crt.h; no game-source edits), then S2/S4 (small), S3 after R-9, S5 with VALIDATE_LAYOUT, S6 with W-4.
* Not run: soak/gameplay gate (no behavioural change for Win32; portable changes are compile-time only).
* Not touched by agreement: PathFind.*, path-node headers, FileRecords/*Path*, tests/portable/*p2h* (MAC-P2h).
