# NOTSA-AUDIT: upstream NOTSA changes in the run build, C3dMarkers::Init (Windows host, 2026-10-17)

Worktree C:/gta-scratch/wtns (base 66d98061). Mechanism: `NOTSA_EXE_OR(exeExpr, upstreamExpr)` (Base.h, expands to the upstream expression in the DLL build) and, for statements, `#ifdef NOTSA_STANDALONE_RUN ... #else #line N <upstream> #endif #line N+L`
(`#line` uses the EFFECTIVE `__LINE__`: several files already carry a `#line` directive, a physical number would shift every later `assert` / `ZoneScoped` line in the Debug DLL object code). Grep key: `NOTSA-AUDIT`.

## 1. Method
* 1021 source lines with `NOTSA` (excluding macro uses), ~690 written by upstream authors (git blame), 307 by the port team (their comments describe exe facts and were taken as such unless the code says otherwise). Debug modules / ImGui / `NOTSA_DEBUG` /
  `USE_ADDITIONAL_CHEATS` / `StuntJumpTestCode` / `DebugCode()` hotkeys: all inert in the run build (UIRenderer::DrawLoop returns early, D8; `NOTSA_DEBUG` undefined). `NDEBUG` is forced, so every `assert` is gone.
* Beyond the NOTSA grep I scanned for the same kind of deviation where it hides without the marker: copy constructors that read `this` instead of `o` (script over all 131 task copy ctors), tasks cloned by the implicit memberwise copy
  (`new X{*this}` without a user copy ctor: 9 classes), pool loops (`GetAllValid()` is always forward, the exe loops backwards: heuristic `sub`/`add` of the pool element size in the exe function over 80 sites), "simplified" comments.
* Each (b) was decided against the exe asm (capstone, `ns_dis.py`); the two non-trivial functions also got exe-oracle rows (logic_oracle_notsa.inc).

## 2. Inventory (class a = no effect on normal play / equal to the exe; b = differed from the exe, now follows it in the run build; c = needed for the port)

### (b) changed in the run build (exe address, what the exe does)
| site | upstream / port behaviour | exe | observable |
|---|---|---|---|
| `Render2dStuff` app_game.cpp:225 | calls `RenderDebugShit()` (script debug lines + `DefinedState()` every frame) | 0x53E230 has no such call (0x53E160 is `ret`) | render state at the start of the 2D pass |
| `CShadows::StoreShadowForPedObject` / `StoreRealTimeShadow` / `StoreCarLightShadow` | `drawOnBuildings \|\|= FxQuality >= VERY_HIGH` | 0x707B40 passes `isPlayerPed`, 0x707CA0 passes 0, 0x70C705 passes `veh == FindPlayerVehicle()` | FX quality "very high" |
| `StoreCarLightShadow` speed test | `>= 0.4` | 0x70C685 `fcomp; test ah,0x41; je` = strictly `>` | boundary |
| `C_PcSave::PopulateSlotInfo` | sets `SLOT_CORRUPTED` for a corrupted save | 0x619268..0x61928C does not | load menu |
| `CTaskComplexAvoidOtherPedWhileWandering`, `CTaskComplexKillPedOnFootStealth`, `CTaskComplexUseScriptedBrain`, `CTaskSimpleStealthKill` copy ctor (`Clone`) | the delegating ctor reads the uninitialised members of the NEW object | 0x66D050 / 0x623400 / 0x6371F0 / 0x623830 pass THIS task's fields | cloned tasks (sequences, scripts) |
| `Clone()` of `CTaskComplexCopInCar`, `...UseClosestFreeScriptedAttractorRun`, `CTaskSimpleCarFallOut`, `...CarSetPedSlowDraggedOut`, `...CarSlowBeDraggedOut`, `...ClearLookAt`, `...CreateCarAndGetIn`, `...DuckToggle`, `...WaitUntilPedIsInCar` | implicit memberwise copy (copies state, sub task, anim, parent; the Run attractor lost its RUN move state: the base copy ctor resets it to WALK) | 0x68CEC0 0x4B87B0 0x64A280 0x64A0B0 0x64A040 0x636F20 0x64A410 0x635C90 0x64A520: fresh task from the ctor arguments | cloned tasks |
| `CFire::ProcessFire` vehicle / object loops | forward | 0x53A8D2 / 0x53AA37 backwards | fire slot / RNG order |
| 41 more pool loops (`GetPedPool/VehiclePool/ObjectPool()->GetAllValid()` in CarAI, CarCtrl, Cheat, Cover, Object, Boat, Plane, Train, Vehicle, Formation, GangWars, Glass, PathFind, Pickup, Population, Replay, TheScripts, WaterCannon, Weapon, World) and 5 `CEntryExitManager` loops | forward | the enclosing exe function loops backwards (`sub reg, sizeof(elem)`; EntryExit 0x440B90 0x440C40 0x43F180 0x43F0A0 0x43EFD0) | tie / first-match / RNG order |
| `CReplay::Display` | wall-clock blink (1 s) | 0x45C21E frame counter 0xA43240, shown while bit 5 set | replay text |
| `CWeapon::TakePhotograph` | squared distance; the object loop skipped objects NEARER than 125 m | 0x73C2EC / 0x73C599: `!(sqrt((z*z+y*y)+x*x) < 125)` skips, for peds AND objects | snapshots / photo pickups |
| `CPed::OurPedCanSeeThisEntity` | `sqmag >= 1600`, `dot < 0` | 0x5E1660 `!(sqrt(dy*dy+dx*dx) < 40)`, `!(dot >= 0)` | ULP / NaN |
| `CCheat::MayhemCheat` | swaps the ped-type acquaintance table with a zeroed one; OFF swaps back and removes the events | 0x439B20: only adds the HATE bits, OFF does nothing | cheat |
| `CGangWars::UpdateTerritoryUnderControlPercentage` | zone classification `grove > ballas+vagos`, ties dropped; ranking starts from (ballas, grove, vagos); `GangRatings[gang] = rank` | 0x443E10..: `ballas+vagos <= grove` -> grove, `ballas > vagos` -> ballas, else vagos; bubble sort from (grove, ballas, vagos); `GangRatings[rank] = gang` (the stats screen prints `ST_GNG<GangRatings[row]>`) | territory %, gang war data, stats page |
| `CPickups::DoPickUpEffects` camera pickup corona | id = object + 1 (one corona per object) | 0x45584B `lea eax,[esp+0x85]`: the same id for all pickups (one shared corona) | night camera pickups |
| `CCoronas::RenderReflections` ground height refresh | `((i&0xFF) + (frame&0xFF)%16) == 0` (only coronas 0, 256.. every 16th frame) | 0x6FB763 `((frame + i) & 0xF) == 0` | wet-road corona reflections |
| `CMenuManager` front-end `DrawBuildInfo()` ("TEST / RW x.x" text, `USE_BUILD_INFORMATION` is defined in config.h) | drawn on every front-end frame | not in the exe | menu text |
| `CRunningScript::ResetIP` | clears the popped IP stack entries | leaves them (saved with the script) | save bytes |

### (c) kept deviations (port cannot do without them / exe UB)
vehicles.ide `wheelSizeFront/Rear = 0.7f` default (already a row). The `C3dMarker::DeleteMarkerObject` `Detach()` deviation is REMOVED (section 3). Exe crash-only guards were left in place on purpose (policy: no UB where the exe is defined; none of them is on a normal-play path):
`Hud.cpp:2389 !txd`, `RoadBlocks.cpp:271/351/412` null `new`, `Collision.cpp:2936 !ecd`, `Weapon.cpp:1546/1587/1619`, `Wanted.cpp:540` clamp, `TaskComplexWaitForBus/SmartFleePoint` null / type checks, `InteriorManager_c.cpp:187` capacity stop,
`MemoryHeap.cpp:350`, `TimeCycle.cpp:43`, `NodeRoute/PatrolRoute/PedGeometryAnalyser` `delete nullptr`, zero-initialised exe-uninitialised locals (Cam, CarCtrl, Interior_c, Group*.cpp labels, TaskComplexFallAndStayDown ...), `AEStreamThread.cpp:217` (zero-length stream = macOS stub data only).

### (a) verified equal / no effect
`AEPedSpeechAudioEntity` `IsGlobalContext*` (flag table equals the exe switch sets for all 358 contexts: streaming 33, interrupting 5, widescreen 3, pain 19; script ns/speech.py), `TaskSimpleUseGun::RemoveStanceAnims` abs (0x61E9EC tests +-50 on both axes),
`CTaskComplexWander::ValidNodes` (0x669F30 also compares area + node), `GetMissionAudioLoadingStatus` (0x4EBF60), `CAnimBlendSequence::SetNumFrames` external-memory flag (the only caller sets it right after), `ObjectData.cpp:60` '#' skip (the exe sees "#" -> no model -> `continue`),
`StuckCarCheck::AddCarToCheck` (`handle < 0` vs `== -1`, slots are -1 when free), `FindObjectsKindaColliding` early return (scan codes are reset per query), `CBike::DoBurstAndSoftGroundRatios` (one rand, the other computations are side-effect free), `ChooseCollisionSoundID` retry cap,
`Fx_c::Reset` `InitEntitySystems` (0x49EA60 is a NOP), `PopCycle::FindNewPedType` outPedMI (oracle compares the true path), `DrawBuildInfo` is compiled in (`USE_BUILD_INFORMATION`, config.h) - see open items, `Clock.cpp:46 gbFreezeTime` (only the ImGui module sets it), `IplStore.cpp:767` inverted conditional, `MenuManager_Draw.cpp:543` save name cut (names are <= 24 chars), remaining helper / structure notes (copy ctors with `o.` fields, lambdas, enumerate loops).

## 3. C3dMarkers::Init
Exe 0x724E40: `Init` does not assign `C3dMarker{}`; it stores the fields of the 32 markers one by one (marker+0x48 atomic, 0x50 type 0x101, 0x52/0x53 flags, 0x54 id, 0x58 colour, 0x5C/0x5E pulse period / rotate rate, 0x60 start time, 0x64 pulse fraction, 0x68/0x6C/0x70 sizes/brightness,
0x74 dist, 0x78..0x80 normal = (0, 0, 1), 0x84 LastMapReadX = 30000, 0x8C roof height, 0x90 last position) and never touches the matrix (+0x00..0x47), `m_LastMapReadY`, `m_LastMapReadResultZ`, `m_OnScreenTestTime`. The port's `rng::fill(.., C3dMarker{})` is a `CMatrix::operator=`
per marker (write through the stale `m_pAttachMatrix` into the freed RwFrame) and set `m_Normal = 0` and `LastMapReadY = 30000`. Now `notsa::audit::ResetMarkers3dLikeExe` (3dMarkers.cpp, run build) does the exe's stores (offsets static_asserted) and `C3dMarker::DeleteMarkerObject` no longer detaches (the exe has no Detach).
Oracle: `logic_oracle_notsa.inc` runs the exe's own loop (bytes copied from 0x724E40..0x724EE5 + epilogue) on a poisoned array and compares 5120 bytes with the port helper: 200 cases, 0 mismatches. Deviation row removed (tsv, STANDALONE_FIDELITY.md).

## 4. Not verified / open
* ~110 task copy constructors use `o.` fields: field NAMES are right, the exe Clone's argument OFFSETS were not compared one by one (only the 4 broken ones and the 9 implicit copies).
* Large rewrites kept as they are (no exe diff done): `CBoat::RenderWakePoints`, `CVisibilityPlugins::ShouldCullVehicleAtomic` / `GetVehicleDotProduct` helpers, `CPopCycle::FindNewPedType` (oracle covered), `CTaskSimpleJetPack` fx key time, `TaskSimplePickUpBike` 0.47 test, `FollowPointRoute` squared compare (`sq(dot)`, ULP only), `Population.cpp:740` division.
* Pool loops of the small pools (IplStore, ColStore, dummy, stunt jumps, txd) and the script commands `SetEntryExitFlagByName` were not checked for direction.

## 5. Gates (wrap-up was requested by the coordinator before the long runs)
| gate | result |
|---|---|
| StandaloneRelease / StandaloneDetached / StandaloneRW `gta_reversed.exe`, Debug DLL `gta_reversed.asi` | build, 0 errors (wtns, ninja -j4) |
| DLL object identity (`obj_code_sig.py` NORM_STRINGS, 1137 objects, baseline = HEAD 66d98061 built in the same worktree) | **0 differ** (after the `#line` fix: the first attempt used physical line numbers and moved `__LINE__` in 4 objects) |
| exe oracle `logic_oracle_test` (Detached) `C3dMarkers::Init marker reset` / `CGangWars::UpdateTerritoryUnderControlPercentage` | 200 / 3000 cases, PC24 and PC53: 0 mismatches |
| run_all_tests.sh 42/42, soak 3 min | **NOT RUN** (stopped early on request, GPU/build slots needed for the playtest bugs): please run both on the commit before relying on it, in particular the removal of `RenderDebugShit`, the backward pool loops and the `Clone()`s |

## 6. Findings that touch combat / weapons / peds / wanted / audio (possible link to the playtest bugs)
* `CWeapon::TakePhotograph`: objects were photographed only when FARTHER than 125 m (inverted range test); ped range used the squared distance.
* `CPed::OurPedCanSeeThisEntity` (cops / peds seeing the player or a target): distance test squared, NaN handling.
* `CTaskSimpleStealthKill`, `CTaskComplexKillPedOnFootStealth`, `CTaskComplexAvoidOtherPedWhileWandering`, `CTaskComplexUseScriptedBrain` clones (script sequences) read garbage; `CTaskComplexCopInCar` clone (police cars) copied the sub task and timers; 8 more tasks cloned by memberwise copy.
* 41 pool loops over peds / vehicles / objects iterated forward where the exe goes backwards (CarAI, CarCtrl, Pickup, Population, World, GangWars, Weapon driveby, ...): order of RNG use and of first-match wins.
* `CGangWars::UpdateTerritoryUnderControlPercentage`: wrong ties, wrong rank mapping (stats screen), wrong percentage.
* Not audio: all audio NOTSA sites were equal to the exe (speech context flags verified for all contexts).
