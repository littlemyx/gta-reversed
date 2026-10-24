# MAC-P5: latent LP64 bugs the compiler cannot see (native macOS build)

Commit f81634d3 on portable/main (pushed to fork). Tool: `tools/portable/lp64_scan.py layouts|offsets` (new).

## Headline: the Fx name bug (MAC-BISECT1 "ects" / "t2" / "arette_smoke", MAC-PLAY1 F-4 was a wrong diagnosis)
Not object.dat and not a stack buffer. `Fx_c::CreateEntityFx(entity, effect->particle.m_szName, ...)` (CEntity::CreateEffects, 2dfx particle effects of streamed models). The 2dfx loader writes the name through
`C2dEffect::Cast<C2dEffectParticle>(&fx)`, `struct C2dEffectParticle : C2dEffectBase, tEffectParticle`. `C2dEffectBase` {CVector, uint8 type} is sizeof 16 but non-POD, so the Itanium ABI (clang arm64) reuses its
tail padding: dsize 13, `tEffectParticle::m_szName` landed at +13 (MSVC/exe: +16, and `C2dEffect::particle` reads +16). Evidence: name pointer at `...01 69 6e 73 65 63 74 73 00` = type byte 0x01 followed by "insects"
starting 3 bytes before the read address. All 2dfx particle effects (insects, cigarette smoke, fires from models, ...) were therefore missing on the Mac build (FindFxSystemBP failed -> no effect) and the type's upper 3 bytes were overwritten.
Fix: explicit 3 byte pad in C2dEffectBase (sizeof unchanged; x86 unchanged) + NOTSA_PORTABLE static_asserts that every C2dEffectXxx view puts its payload at +0x10. After: 0 "Couldn't find Fx system bp" in s1/lod_tp/lod_drive/s2/s7.

Generic scan (81.6k records from 1000 TUs, `lp64_scan.py layouts`): this was the ONLY game class with tail-padding reuse (the one other hit is libc++ internal).

## Hit table
| file:line | pattern | verdict | fixed |
|---|---|---|---|
| Plugins/TwoDEffectPlugin/2dEffect.h C2dEffectBase | Itanium tail-padding reuse (C2dEffectParticle name at +13) | LP64/ABI bug | yes (pad + static_asserts) |
| 2dEffect.cpp:193 | `Malloc(n*sizeof(C2dEffect) + sizeof(entry::m_nObjCount))`: header is 8 bytes on LP64 (C2dEffect 8-aligned), heap overrun by 4 | LP64 bug | yes (`sizeof(t2dEffectPluginEntry)`) |
| Cam.cpp:183,1233,1234 | `NOTSA_GLOBAL_ALIAS` = `(uint8*)&gIdleCam + 148/+40`, `&g_InterestingEvents + 300` (CIdleCam::m_Target / m_Events hold pointers) read the wrong members | LP64 bug (idle camera logic) | yes (member refs) |
| Cam.cpp:1235 | `&gCrossHair + 40` (CWeaponEffects, no pointers) | x86 == LP64 | n/a |
| TaskSimpleFight.cpp:1325 | `(CVector&)(&TheCamera + 772)` = m_aCams[0].m_vecFront (mouse 3rd person aim) | LP64 bug | yes (NOTSA_PORTABLE only) |
| Group21b.cpp:215 | `&CPedGroups::ms_groups[g] + 0x2CC` (SET_GROUP_SEQUENCE_TASK) | LP64 bug | yes (NOTSA_PORTABLE: `GetIntelligence().SetTaskSeqId`) |
| Group23.cpp:647 | `(uint8*)car + 0x717` = m_swingingChassis.m_doorState | LP64 bug | yes (member) |
| TaskComplexPartner.h:53 | `this + 0x70` go-to-point counter (derived `field_70`); on LP64 +0x70 is INSIDE the base: the counter overwrote base fields every frame (partner chat/deal/greet/shove tasks) | LP64 bug | yes (NOTSA_PORTABLE own member) |
| CutsceneObject.cpp:28, CutsceneMgr.cpp:47 | union {RwFrame* m_pAttachToFrame; uint32 m_nAttachBone}: ctor / AttachObjectToBone write 4 bytes, upper half of the pointer stale, read as `if (m_pAttachToFrame)` | LP64 bug | yes (write the whole union) |
| CustomCarEnvMapPipeline.cpp:56,69,75 | RpMaterial/AtomicRegisterPlugin(size 4) for a pointer slot | benign (librw rounds each slot to sizeof(void*)) but fragile | yes (`sizeof(void*)`) |
| MemoryMgr.cpp:136,347 | MallocAlign stores `void*` at result-1: needs align >= 8 | latent; all callers use 64/2048/sector size | no (documented) |
| Entity.h:120 m_nLodIndex/m_pLod; ClumpModelInfo/VehicleModelInfo anim index/name; Cam.h two-player unions; Radar/Breakable unions | pointer overlaid with a 32-bit scalar | reviewed: all readers compare the low 32 bits or the pointer side is reset first; two-player cam unused | no |
| EntryExitManager.cpp:371 | `(CEntity**)&PC_Scratch[256]` list | capacity halves on LP64 (2016 entries) but GetObjectsInFrustum range 6 m; same pattern as R9-1 | no (listed) |
| Streaming.h:203-204, Glass.h:25-26, common.h:315, Population.cpp:1636, MissionCleanup.cpp:34-35, TaskSimpleUseGun.cpp:589-590, Bike.cpp:81, Group09.cpp:39 | alias of one global to another at a byte offset | arrays of int32/float/char/POD without pointers: x86 == LP64 | n/a |
| TheZones.cpp:239,243; Messages.cpp:478; FileLoader.cpp:1160; C_PcSave.cpp:62; MenuManager_Input.cpp:169; VehicleRecording.cpp:304 | literal memset/memcpy sizes, `(WORD*)pad)[11]`, int8 view of a frame | file structs / CPad (no pointers): x86 == LP64 | n/a |
| Font.cpp:421,749 | `(uintptr)dst & 3` alignment of CFontChar records | 4 is enough on arm64 (unaligned loads OK) | no |
| Shadows.cpp:214,1915, Crime.cpp:58, Pickups/Corona IDs | pointer truncated to a uint32 ID | NOTSA_PTR_ID32 convention; collisions only for equal low 32 bits | no |
| Animation AnimBlend*.cpp MallocAlign | sizeof based since earlier slices | ok | n/a |
| standalone/rw/camera_sync.h, pipeline_skin*.h | lifted x87 code on synthetic x86 layouts (`frame+0x50`, truncated pointer in a fake camera) | by design (emulation buffers), not audited further | no |

Negative results of the sweeps (nothing left): `(char*)p + N` / `reinterpret_cast<uint8*>(x) + N` outside the rows above, `memcpy/memset` with literal sizes of structs with pointers, `sizeof/4` arrays, `RegisterPlugin` literal sizes (only the env-map ones), bit-packed pointers (`uintptr & mask`: only the aligned allocators and Font), flexible-array headers (only t2dEffectPluginEntry), `long`/DWORD members in game structs (shim types are 32 bit).

## A/B (wine cl.exe, 12 TUs, HEAD vs HEAD + the patch)
code+relocs identical: Entity, FileLoader, InteriorManager_c, Cam, Group23, CutsceneObject, CustomCarEnvMapPipeline, TaskComplexPartner. `__LINE__`-only immediates: 2dEffect.cpp, CutsceneMgr.cpp.
The first version changed Group21b.cpp (+2 dead COMDATs GetIntelligence/SetTaskSeqId) and TaskSimpleFight.cpp (dead std::array<CCam,3>::operator[] COMDAT and one x87 instruction reorder in ProcessPed): both sites are now `#ifdef NOTSA_PORTABLE`
with the original expression kept for MSVC (only `__LINE__` shifts there); not re-run through wine after the guard (the guarded text is the pre-patch text byte for byte).

## Runs (native, PortableMac, NOTSA_LOCK_FILE=/tmp/p5/p5.lock)
s1, lod_tp, lod_drive, s7: exit 0; s2 (110 s budget): exit 0; "Couldn't find Fx system bp": 0 (before: 17+ per s1 run). Only error line: W-B5 WMA user tracks (known). No hang / crash.

## Consumer audit
* C2dEffectBase pad: readers of C2dEffect (Cast<> users: FileLoader, InteriorManager_c, Entity::CreateEffects, TwoDEffectsDebugModule, ReadArg, Scripted2dEffects) all go through typed members; layout of the exe unchanged.
* New `CTaskComplexPartner::m_goToPointFrameCounter` (portable only): only `GetGoToPointFrameCounter()` touches it (TaskComplexPartner.cpp:198,324); derived `field_70` is no longer used on portable.
* `SetTaskSeqId`: the only writer of CPedGroupIntelligence::m_TaskSeqId outside its own class was this opcode.

## MAC-FLOOP2 follow-up (pc24_scan.py --only, not converted)
Group23.cpp: AUTO=11 LOCAL-AUTO=5, CutsceneMgr.cpp: AUTO=1 LOCAL-AUTO=1, Group21b.cpp: AUTO=2 double-precision float chains FLOOP2 would convert; 2dEffect.cpp, CutsceneObject.cpp, CustomCarEnvMapPipeline.cpp: none.
Landed together with FLOOP2's staged x87d hunks of Cam.cpp and TaskSimpleFight.cpp (as approved by the orchestrator).

## Compiler-warning sweep (-Wformat -Wpointer-to-int-cast -Wint-to-pointer-cast -Wsizeof-pointer-memaccess -Wclass-memaccess ...)
See the section appended below (WARN).

## (WARN) compiler-warning sweep, 998 distinct warnings over 1000 TUs (/tmp/p5/warns.txt; flags -Wformat -Wformat-security -Wpointer-to-int-cast -Wint-to-pointer-cast -Wsizeof-pointer-* -Wclass-memaccess -Wdynamic-class-memaccess -Wvarargs -Wcast-align)
* -Wpointer-to-int-cast / -Wint-to-pointer-cast / -Wsizeof-pointer-memaccess / -Wvarargs: 0 (every pointer-as-ID site already goes through uintptr / NOTSA_PTR_ID32).
* -Wformat (13): DecisionMakerTypesFileLoader.cpp:105 `%n` into a size_t (benign: initialised 0, little endian, value < 2^31; not changed to keep the MSVC TU unchanged); toolsmenu debug modules (`%d`/`%u` with size_t / ptrdiff_t / long, `%s` with std::optional<const char*> in DoorDebugModule:56,57,76 and PedDebugModule:172): debug UI only, cosmetic, listed not fixed. -Wformat-security (9): non-literal format strings, no pointer-width issue.
* -Wdynamic-class-memaccess: Replay.cpp:702,760 memcpy over a CCamera (vtable pointer rewritten with the identical saved value): same on x86.
* -Wcast-align (94): arm64 tolerates unaligned loads; no hit on a pointer-holding record.
* Not LP64-specific but found by the same run: 50 x -Wreturn-type (functions / lambdas that fall off the end; on arm64 the return register is garbage, on MSVC usually the last eax): e.g. AnimManager.cpp:193, CarAI.cpp:878,1036, Clothes.cpp:104, Coronas.cpp:302, Cover.cpp:262,267, ControllerConfigManager.cpp:1019,1455,1567, AEPedSpeechAudioEntity.cpp:1271; full list: `grep Wreturn-type /tmp/p5/warns.txt`. Worth a separate fidelity slice.
