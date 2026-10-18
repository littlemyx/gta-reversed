# VT-OVERRIDES report: missing virtual overrides, dtor / reference bookkeeping, cheats (Windows host, 2026-10-18)

Start: `reverse/interior_c` @ 87b997a3. Worktree `C:/gta-scratch/ptv` (branch `pt-overrides`), runs in `C:/gta-scratch/ptv_out/`.

## 1. The audit (tools)

| tool | what it does |
|---|---|
| `tools/standalone/vtable_override_audit.py` (rewritten) | reads the **built port exe's real vtables** (`??_7Cls@@6B@` of the linker map) and the original exe's vtables (all 586 found by scanning `.text` for vptr stores; 535 named, 525 compared with a port vtable), slot by slot, against the base class: MISSING (exe slot has own code, port inherits), WRONGFN (registry name differs), EXTRA (port override where the exe inherits), COUNT, DTOR (destructor bodies: calls with registry names and virtual calls vs the port destructor), UNREG (both override but the exe address is never mentioned: tiny leaf bodies compared automatically, stubs flagged, the rest listed). Triaged rows live in `vtable_override_audit_explained.tsv`; the tool prints `UNEXPLAINED: {}` at the end. (The first version only grepped the sources for the address; that found 33 rows of which most were false positives and missed the classes below.) |
| `tools/standalone/callee_audit.py` | for every registry function: calls to CEntity::RegisterReference / CleanUpOldReference, CAnimBlendAssociation::SetFinishCallback / SetDeleteCallback, Add/RemoveAnimBlockRef in the exe vs the port (depth-2 through small helpers). 45 candidates, ~15 real, rest `notsa::EntityRef` / lambdas. |
| `tools/standalone/stub_audit.py` | functions defined as stub / empty / NOTSA_UNREACHABLE although the exe has code at the address in the comment: 29, 15 with an exe reference; all documented no-ops or false positives (TODO comments); `CCarAI::TellCarToRam/BlockOtherCar` have no caller in the exe. |
| `tools/standalone/exedis.py` | disassembler of a function / vtable (`--vt`) / references (`--callers`) of the exe with registry names. |

Unnamed exe vtables (51): 43 are CRT / STL runtime classes (> 0x770000), the rest are abstract bases (all slots `_purecall`) and 4 small classes without any caller; named but without a port vtable: `CDummyPed`, `CEventSpecial`, `CTaskSimpleWaitForPizza` (the exe constructs them only inside its own `Clone`, never reachable).

## 2. Fixed (asm evidence in the commit messages)

| # | what | exe | port was |
|---|---|---|---|
| 1 | `CTrain` vtable slots 5, 14, 17, 27, 29, 30, 45: SetModelIndex 0x6F5530, Teleport 0x6F7190, PreRender 0x6F5570 (train head / tail lights), OpenDoor 0x6F6370 (door frame matrix, atomic flags, sound events), GetDooorAngleOpenRatio(U32) 0x6F5930 / 0x6F59C0, IsRoomForPedToLeaveCar 0x6F5EE0; `~CTrain` 0x6F54D0 (audio Terminate before CVehicle's body) | own code | inherited CVehicle's empty versions |
| 2 | `CAutomobile::SetModelIndex` 0x6A6590 (clear + FillFrameArray of the 25 car nodes) | own | inherited |
| 3 | `CTaskAllocatorKillThreatsDriveby`: shadowing `m_Threat` / `m_Timer` members, so the inherited constructor's threat was never seen by `AllocateTasks`: **the drive-by response of gang groups never allocated a task** | | |
| 4 | `CTaskComplexWaitForBus::MakeAbortable` returned `true` without asking the sub task (exe slot 6 = CTaskComplex::MakeAbortable) | | |
| 5 | `CTaskSimpleAffectSecondaryBehaviour::ProcessPed` 0x691320 remove-mode returned the **opposite** (`true` after the first request, `false` forever when nothing is left); no destructor (exe `delete m_task`) | | |
| 6 | destructors / constructors that left reference entries and anim callbacks dangling: `CPlayerPed` 0x6093B0 (2 anim block refs + player group), `CTaskSimpleChoking` 0x620370, `CTaskSimpleStealthKill` 0x622680 (+ `MakeAbortable` 0x6226F0: delete callback, blend delta -1000, IMMEDIATE / URGENT semantics), `CTaskSimpleCarGetIn` 0x646710, `CTaskSimpleCarCloseDoorFromInside/Outside` 0x646380 / 0x646570 (the port reset the DELETE callback, the exe resets the FINISH callback), `CTaskSimpleCarFallOut` ctor+dtor 0x648D60 / 0x648DE0, `CTaskComplexEvasiveDiveAndGetUp` ctor+dtor 0x6536B0 / 0x653740, `CTaskComplexSignalAtPed` dtor 0x660AB0 (gangs anim block ref), `CTaskSimpleCarJumpOut` ctor 0x64BE40 (**cleaned up** the reference instead of registering it), `CCopPed::RemoveCriminalToKill` 0x5DE040 (**registered** instead of cleaning up) | | |
| 7 | `CTaskComplexKillPedOnFoot::Clone` 0x623370 dropped the two wait-for-player-safe flag bits | | |

## 3. Cheats (item 3)
Method: `tools/standalone/cheats_route.txt` types every cheat (CKeyGen hash of the reversed string, 200 ms between keys: the injector dropped repeated letters in OSRBLHH / BUFFMEUP without waits) with a `probe:` line after each; the same script drives the original exe through `inputdrv.exe` while `windows/cheat_probe_exe.py` samples its memory (wanted level, armour, money, cheat flags, weapons, vehicles, weather, time scale). `compare_cheat_probes.py` compares the two logs by scene position.

Causes found (all in `Cheat.cpp`, ported from the asm, oracle rows in `logic_oracle_cheatfns.inc`):
* VillagePeople, Funhouse, Ninja: ON and OFF branches swapped (the first press ran the OFF clean-up, the second the setup).
* Dusk set the clock when switched OFF; Predator spawned a vehicle where the exe's function is a bare `ret`; NotWanted stored an extra `bWantedByPolice`; WeaponCheat1 gave the set twice; VehicleCheat read the streaming flags after requesting the model.
* BeachParty / Funhouse crashed: two leftover `FIX_BUGS` calls `SetTextureAndModel(nullptr, nullptr, ...)` and a non-null-safe `CPedClothesDesc::SetTextureAndModel` (exe 0x5A8080 maps null names to key 0): both fixed.

Result: weapon sets, HESOYAM, wanted up / down / clear, spawn cheats (Rhino 432, Rancher 505, HotringB 502, Vortex 503, Trashmaster 408) and the toggles match the original in state / models. Remaining differences are explained: traffic randomness, and the original CJ died around cheat 28 (resets its flags and weapons, so later rows are not comparable). Open observation: after the sunny-weather cheat `CMissionCleanup::Process` releases the forced weather in the same frame in the port; not verified that the exe does the same at that moment (script/mission state differs between the runs).

## 5. Open items
* Clone / CloneEditable overrides (45 UNREG slots): sampled by hand only (one real bug: KillPedOnFoot); an automatic offset-set comparison was too noisy.
* Other `// FIX_BUGS` deviations not audited: Replay.cpp:802, TheScripts.cpp:1664, Radar.cpp:2302 / 2452.
* `CCarAI::TellCarToRam / BlockOtherCar` have no caller in the exe.

## 4. Oracle rows added (`logic_oracle_test`)
* `logic_oracle_taskrefs.inc`: reference / callback bookkeeping of 8 task destructors, 7 constructors and `CCopPed::RemoveCriminalToKill` in the BUG-REFS fixture; mutants (JumpOut ctor clean-up, CloseDoor delete callback, Choking dtor, RemoveCriminalToKill register) -> 1125 mismatches.
* `logic_oracle_cheatfns.inc`: 36 cheat functions (dusk / midnight / time / wanted / health / weather / stats / OFF branches of funhouse, ninja, village people, adrenaline ...); the original sources mismatch (Dusk 400/400, Predator crashes the harness through VehicleCheat).

## 6. Verification (final Release exe built from HEAD of `pt-overrides`)
| gate | result |
|---|---|
| Release build (`build/StandaloneRelease`, ninja -j4) | OK |
| `run_all_tests.sh` | 44 / 44 ok (22 oracle incl. new `CCheat`/`CTask` rows + 21 rw_ + x87), 0 mismatches |
| soak 3 min (`soak.sh`, GPU lock) | RESULT: OK, 0 errors, 39 screenshots (`ptv_out/soak4`) |
| `vtable_override_audit.py` | 586 exe vtables, 525 compared with a port vtable, `UNEXPLAINED: {}` (140 explained rows in the TSV) |
| cheat scene ours vs original | see section 3 |
