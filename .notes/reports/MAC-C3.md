# MAC-C3: clang undefined-behaviour warnings (STREAM_C_PLAN slice C-3) + exit abort

Commit 144285db on portable/main (pushed). Warning runs: `tools/standalone/clang_census.py run --mode native --compdb build/PortableMac/compile_commands.json --extra "<flags>"` (1114 TUs, 36 s, syntax-only; the flow-sensitive warnings are all front-end ones) + a real build in build/PortableMacWarn (1119 steps, `-Werror=return-type` clean).

## 1. -Wreturn-type: 57 sites (50 in MAC-P5's list + 7 header sites the PCH hid) -> 0
Mechanism: `NOTSA_UNREACHABLE_RET(RET, ...)` (Base.h). Win32: expands to `NOTSA_UNREACHABLE(...)` token for token (exe: runs off the end, EAX / ST0 garbage; run build logs "UNREACHABLE hit"). Portable: logs the hit, `return RET`. All edits in place (no line shifts). Portable CMake target: `-Werror=return-type -fno-strict-return -fno-strict-aliasing` (clang only).
Verdict legend: U = default of a switch that covers every enumerator that can occur (unreachable by construction, no exe value to reproduce); N = NOTSA-only helper (no exe code); X = exe function that is unused/never called; R = reachable only on corrupt or modded data, exe value is garbage (EAX leftover is not reproducible), portable returns a safe neutral value.
| site(s) | what | verdict | portable RET |
|---|---|---|---|
| WinMain.cpp:257 | game-state lambda | U | GAME_STATE_TITLE |
| extensions/utility.hpp:88 find_value | needle not in mapping | N | value-init of mapped type |
| AnimManager.cpp:193 GetAnimationGroupIdByName | name not found | N, R | {} |
| AEPedSpeechAudioEntity.cpp:1271 GetVoiceSoundBank | ped type outside the 5 voice types | U | {} |
| CarAI.cpp:878, :1036 | escort mission remap lambdas | U | MISSION_ESCORT_LEFT(_FARAWAY) |
| Clothes.cpp:104 | unknown rule tag in clothes.dat | R | TAG_CUTS |
| ControllerConfigManager.cpp:1019, :1455, :1567 | GetIsKeyBlank (unused), GetControllerStateJoyStick, GetControllerState | U | false / pad.PCTempJoyState.ButtonCross / pad.PCTempKeyState |
| Core/Vector.h:301-303 | 3 deprecated Multiply* stubs | N | CVector{} |
| Coronas.cpp:302 | flare def lambda | U | &SunFlareDef[0] |
| Cover.cpp:262, :267, :536 | CalculateHorizontalSize, DoLineCheckWithinObject, FindVectorFromFirstToMissingVertex (all "Unused") | X | 0.f / false / {} |
| CoverPoint.cpp:177, :189 | ShouldBeRemoved / GetPos after full-enum switch | U | false / {} |
| Entity/Entity.cpp:2419 GetRpAtomicOrFirstAtomicOfClump | RwObject type neither atomic nor clump | N, R | nullptr |
| Entity/Ped/Ped.cpp:6100 GetPadNumber | ped type not PLAYER1/2 | N, U | 0 |
| Entity/Physical.cpp:94 PhysicalGetRepeatSectorList | type not vehicle/ped/object (source note: exe reuses previous iteration's list); never hit in 25+ routes (DBG probe in default printed nothing) | U | nullptr |
| Entity/Vehicle/Automobile.cpp:1738, :4017; Vehicle.cpp:4806 | rhino extra wheel remap, GetNthUnpicked, rotor dir | U | door.m_openAngle / n / {} |
| Enums/eCarWheel.cpp:19, Enums/ePedType.h:128 | enum -> enum maps | U | {} |
| Events/GroupEventHandler.cpp:132 | ComputeWalkAlongsideResponse ("Unused") | X | nullptr |
| Fx/FxInfoManager.cpp:168 | FX info type lookup, unknown name (file data) | N, R | 0 |
| PedAttractorManager.h:211 VisitAttractorsOfType | attractor type enum | U | value-init of the visitor result type |
| PedGroupPlacer.cpp:288 PlaceGroup | allocType not FOLLOW/STAND/CHAT/RANDOM | U | false |
| Plugins/RpAnimBlendPlugin/RpAnimBlend.cpp:210 | RpAnimBlendClumpAddAssociation ("Unused function") | X | nullptr |
| Population.cpp:890 | ped class by ped type | U | nullptr |
| Replay.cpp:1120 | vehicle subtype of a replay packet | R | nullptr |
| Scripts/CommandParser/ReadArg.hpp:284 (2 sites inside Read<T>), StoreArg.hpp:44 | SCM parameter type byte not valid for the argument kind (corrupt script) | R | T{} / GetGlobal<T>(0) |
| Scripts/TheScripts.cpp:334, TheScripts.h:594 | checkpoint colour, GetSCMChunk (no such chunk) | U, R | {} / nullptr |
| Sprite.cpp:263 | vertex colour index 0-3 | U | 0u |
| Tasks/.../TaskInteriorLieInBed.cpp:264 | anim seq idx | U | {} |
| Tasks/.../TaskComplexEnterCar.cpp:361, :741 | CreateNextSubTask / CreateSubTask on an unexpected subtask type | R (the NOTSA_UNREACHABLE already marks it) | nullptr |
| TaskSimpleCarGetIn:68, CarGetOut:96/:113, CarJumpOut:77, CarOpenDoorFromOutside:81/:168, CarSlowBeDraggedOut:37 | door enum -> anim / door lambdas | U | first case value |
| TaskSimpleClimb.cpp:112, TaskSimpleFightingControl.cpp:162, TaskSimpleGangDriveBy.cpp:209, TaskSimpleGunControl.cpp:219 | climb offset, event type, drive-by anim by sector, gun command | U | {} / CVector{} |
| WaterLevel.cpp:288 GetWaterLayerTexInfo | layer 0/1 only | N, U | {} |
| Weapon.cpp:282, :1325, :2702 | LaserScopeDot (UNUSED), GetProjectileType, Fire result lambda | X, N, U | false / WEAPON_ROCKET / {} |
Not touched by anyone else's uncommitted work at commit time (Cover.cpp, Ped.cpp were dirty from MAC-FLOOP2-REV while I edited; REV landed 2aa95584 before my commit, the files held only my hunks then).
Proof: wine cl.exe, `obj_code_sig.py` (NOTSA_SIG_NORM_STRINGS=1), HEAD worktree vs HEAD+patch: 45 touched / representative TUs identical (code + relocs, not even a `__LINE__` immediate), +10 TUs that include IKChain.h / EntityRef.hpp identical, +EventPotentialWalkIntoBuilding.cpp identical.
`-Werror=return-type` verified: the build.ninja build of build/PortableMacWarn links with it on.

## 2. Warning sweep (-Wunsequenced -Wuninitialized -Wsometimes-uninitialized -Wconditional-uninitialized -Wshift-count-overflow -Wint-in-bool-context -Wtautological-compare -Wdangling -Wreturn-stack-address -Wnull-dereference -Wundefined-reinterpret-cast -Wmismatched-new-delete)
Counts (distinct file:line): unsequenced 0, shift-count-overflow 0, int-in-bool 0, dangling 0, return-stack-address 0, null-dereference 0, mismatched-new-delete 0.
| class | n | sites | verdict | action |
|---|---|---|---|---|
| -Wuninitialized | 6 -> 2 | EventPotentialWalkIntoBuilding.cpp:5 (x4: base ctor gets its own uninit members, `SafeRegisterRef(m_building)` on heap garbage / 0xFD fill) | clang-only hazard (Win32 reads whatever the heap held) | fixed NOTSA_PORTABLE (nullptr / {} / 0), F9 in PORTABLE_SYNC |
| | | FireManager.cpp:482 `point` used in its own initialiser | real port bug, Win32 too | PORTABLE_SYNC F7, not fixed |
| | | SpecialFX.cpp:91 AddWeaponStreak lambda captures uninit LTM/end | function is unused (exe 0x7233F0 "unused function") | none |
| -Wsometimes-uninitialized | 11 | Collision.cpp:2448 (x6), :2701 (x2), CollisionPrimitives.cpp:498, Vehicle.cpp:847, :882 | `default: NOTSA_UNREACHABLE()` of a switch on a value that is constructed (GetPrincipleAxis 0/2/4, enum) | benign U, none |
| -Wconditional-uninitialized | 14 | Collision.cpp:2459 (x2), :2704, Vehicle.cpp:853 | same switch default | benign |
| | | Entity.cpp:671 savedAlphaRef, Automobile.cpp:6085 fColZ, Bmx.cpp:315 refAngle, GangDriveBy.cpp:649 windowFlag, TimeCycle.cpp:441 farBoxB_T | guarded by the flag that is also the "assigned" condition | false positives |
| | | AnimManager.cpp:86 `def`, FileLoader.cpp:2537/2539 `id`/`objModelIndex` (data file line order), CarGenerator.cpp:195 `vehicle` (unknown vehicle type) | exe has the same uninitialised read on malformed data | none (same on Win32) |
| | | MenuManager_Draw.cpp:686 displayText | only the NOTSA_UNREACHABLE "index out of bounds" branch leaves it unset | benign |
| -Wundefined-reinterpret-cast | 15 | FileLoader.cpp:632-686 (11, `reinterpret_cast<T*&>(bufferIt)++` col cursor), Cover.cpp:145,168 (`CBuilding*` as `CEntity*&`), GangWars.cpp:675, Group12.cpp:270, TheScripts.h:622 (int<->float) | strict-aliasing UB that MSVC (no TBAA) tolerates; today harmless (clang's pointer-TBAA is off, scalar cases are SROA'd) but one inlining change from silent miscompiles | clang-only hazard: `-fno-strict-aliasing` for the portable target (this is the lever; the 15 sites stay as they are, MSVC TUs untouched) |
| -Wtautological-constant-out-of-range-compare | 10 | MentalHealth.cpp:44-50, CarGenerator.cpp:344, AERadioTrackManager.cpp (overlap-compare, 1) | documented BUG comments, faithful to the exe | none |
| | | Text.cpp:422,432 (`char c >= 128`) | signed char: accent tables never used (MSVC too); exe signedness unverified | PORTABLE_SYNC F8 |

## 3. Exit abort
Root cause (stack from p5a.out, and reproduced here as exit 134, "fault address 0xfdfdfdfdfdfdfe05" in `IKChainManager_c::~` -> `CEntity::CleanUpOldReference`): `g_ikChainMan` is a real static in the portable build; `IKChain_c::m_Ped` (`CPed::Ref`) and `m_TargetEntity` (`EntityRef`) unregister from the entity in their destructors, and exit() runs them after the quit path shut the pools down (freed CPed filled with 0xFD). The exe never destroys the manager. Sporadic: only when a look-at chain is live at quit (baseline: 1 abort in 5 s1 runs, 0 in 2 s2 runs).
Fix: `EntityRef::ForgetAtExit()` + `~IKChain_c()` under NOTSA_PORTABLE forgets both pointers (Win32 keeps `= default`).
Verification (PortableMacWarn, load average <= 6): s1 8/8 exit 0 (+5 with a debug variant), s2 3/3 exit 0 (66-75 s), s3 / s4 / s7 / s8 / s9 / s5 / lod_drive exit 0. "exit 0 three times" is a statistical proof only (the abort rate was ~20 % of s1 runs); a deterministic repro would need a live chain at quit.

## Other
* Flaky crash seen while the machine was loaded by parallel builds: CWorld::Process -> CPhysical::RemoveAndAdd, fault address 0, in the new-game PROLOG1 cutscene; ~70 % of runs at load >= 11 (baseline exe too), 0 of ~30 at load <= 6; PortableMacRR does not show it. Same as W5 in PORTABLE_SYNC. Not caused by this slice; investigated only far enough to rule out the default branch of PhysicalGetRepeatSectorList (a probe there never fired).
* Not run: the 900-frame determinism route (needs a PortableMacRR rebuild while other agents have uncommitted edits in the tree); the new flags cannot change FP results.
* Consumer audit: no shared state layout/meaning changed (only RET values on paths the exe leaves undefined, one portable-only ctor argument, and a destructor that now skips unregistering a pointer at process exit).
* Cleanup: no wine / ninja / game processes left (worktrees /tmp/c3/wtA, wtB removed).
