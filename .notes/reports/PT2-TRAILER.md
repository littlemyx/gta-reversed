# PT2-TRAILER - playtest-2 crash in CTrailer::ProcessEntityCollision + interior ped area code

## Summary
| # | finding | verdict | fix |
|---|---|---|---|
| 1 | access violation at 09:46 (build ac70ea8a) in `CTrailer::ProcessEntityCollision` <- `CPhysical::ProcessCollisionSectorList<CBuilding*>` | port bug, asm-proven: the port read `ocd->m_nNumTriangles` of the OTHER entity unconditionally; the exe touches the other entity's col data only inside `if (m_pTractor == entity \|\| m_pTrailer == entity)` (0x6D008B..0x6D00CE). A building whose col model has no col data (null `m_pColData`) therefore crashed the port and is handled by the exe (ProcessColModels returns 0). | 3a3428d2 |
| 1b | same function, 4 more deviations from the asm (see below) | port bugs, no crash | 3a3428d2 |
| 1c | `CAutomobile::ProcessEntityCollision` (0x6ACE70) compared line by line: one deviation (reference handling) | fixed | 3a3428d2 |
| 2 | interior peds have area code 0 while `currArea` is 6; far cops sit in task 1307 | **original behaviour** (measured in the original exe, see below); no port change | - |
| 2b | the original-exe harness's ped-pool read returned 0 peds | tool bug: pool stride 0x7A4 instead of 0x7C4 (also wrong: ped type 0x594 -> 0x598, intelligence 0x440 -> 0x47C) | fixed in `pti_probe/` and `ptc_probe/` (+ copies in `PT2-TRAILER_tools/`) |

## 1. Crash
### Evidence and symbolisation
`C:/gta-scratch/playtest2/log.log`: exception 0xC0000005 at 0x009DC705, parameters `[0]=0 (read), [1]=4`, ECX = 0, image base 0x007A0000. Stack: `CTrailer::ProcessEntityCollision:311 <- CPhysical::ProcessCollisionSectorList::lambda<CPtrListSingleLink<CBuilding*>> <- CPhysical::ProcessCollisionSectorList <- CPhysical::CheckCollision <- CAutomobile::ProcessControlCollisionCheck <- CAutomobile::ProcessControl <- CTrailer::ProcessControl <- CWorld::Process`.
The faulting instruction in `C:/gta-play/gta_reversed.exe` (= ac70ea8a, disassembled at RVA 0x23C705): `movzx eax, word ptr [ecx+4]` with `mov ecx, [eax+0x2c]` of `entity->GetColModel()` one line above: `ocd` (the building's col data) was null and `ocd->m_nNumTriangles` (offset 4) is read. The source line 311 is `originalONumTri = ocd->m_nNumTriangles` (initialiser "saving my sanity ... unconditionally assigning").

### What the exe does (0x6CFFD0, line by line)
- 0x6CFFD9: `m_fTrailerTowedRatio == -1000` -> `CAutomobile::ProcessEntityCollision`.
- 0x6D0006: status != SIMPLE -> `bVehicleColProcessed`.
- 0x6D0022: only `ebp = GetColModel()->m_pColData` (the trailer's). **The other entity's col data is read only in 0x6D00A4..0x6D00C8, behind `cmp [esi+0x4c4], edi / cmp [esi+0x4c8], edi` (towing / towed vehicle)**.
- 0x6D0190 wheel loop (4 iterations), 0x6D02BF support loop (2 iterations, SEPARATE and after the wheel loop), 0x6D0336 collision records, 0x6D035F hit-wall.

### Deviations fixed (3a3428d2)
1. Unconditional read of the other entity's col data (the crash). Now only inside the towing/towed branch, saved/restored there exactly as 0x6D00A4..0x6D014B.
2. The 2 support lines were processed INSIDE the 4-iteration wheel loop (ran 4 times; no visible effect because the second pass finds `touchDist >= m_supportRatios`). Moved after the wheel loop (0x6D02BF).
3. The port copied the other vehicle's `m_anCollisionLighting[i]` (that is CAutomobile's code at 0x6AD1AA; the trailer asm has no such block) - removed. `m_nContactSurface` is stored for every entity type (0x6D0248), as before.
4. Wheel entity reference: the exe does `m_apWheelCollisionEntity[i] = entity; entity->RegisterReference(&slot)` (0x6D01FB..0x6D0200) without cleaning up the old reference; the port used `ChangeEntityReference`. -> `CEntity::SetEntityReference` (also in `CAutomobile::ProcessEntityCollision`, 0x6AD14B..0x6AD150).
5. `SetHasHitWall` (0x6D035F..0x6D037A): BOTH terms (building / object with `bDisableCollisionForce`) need `numColPts > 0`; the port's `a && b || c` let an object with the flag set it from `numProcessedLines` alone.

`CAutomobile::ProcessEntityCollision` (0x6ACE70) was compared with the whole asm (579 lines): flags / model checks, Rhino extra wheels, hide-triangles, ghost status, wheel loop (lighting copy for vehicles is there, correct), forklift col point removal 0x6AD378, hit-wall 0x6AD45A, extra wheel col points 0x6AD482: only the reference handling (4) differed. Its `FIX_BUGS || NOTSA_STANDALONE_RUN` null guard is a deliberate earlier deviation (unchanged).

### Why only trailers, and how common the condition is
`cmd:nullcol` (below) counted, around Grove Street: **58 of 89 building models in range have no col data at all** (the others 20 were nulled by the command to extend the condition to walls). So the condition is normal data, and the exe (which never reads it) is fine; only the trailer was affected because `CAutomobile::ProcessEntityCollision` has its own null guard in the run build.

### Scripted repro (before / after)
New run-build diagnostics in `NOTSA_STANDALONE_INPUT` (SDLWrapper.cpp, e572a548 / 6639f22d): `cmd:rig:DX:DY:HEADING` (Linerunner + Articulated trailer created with `CCarCtrl::CreateCarForScript`, attached with `SetTowLink` like ATTACH_TRAILER_TO_CAB), `cmd:rigpush:VX:VY`, `cmd:nullcol:RANGE:MINHEIGHT` (drops the col data of tall building models: the playtest-2 condition made denser), `cmd:rigstate`. Route: `PT2-TRAILER_tools/route_rig.txt` (new game, HESOYAM, rig 25 m ahead, nullcol, push in 5 directions); runner `run_probe.sh`.

| build | result |
|---|---|
| exe with the old Trailer.cpp (HEAD a1a6a14b + the cmd:s) | **access violation, read address 4, EAX=8 ECX=0 EDX=0x10000002 - the same register signature as the user's crash**, instruction `movzx eax, word [ecx+4]` in the same code |
| after the fix | route runs to the end (RIG_END), 0 exceptions, the rig falls through the world where the ground building models were nulled (expected for the test), nColl counters move |

### Exe-oracle row
`CTrailer::ProcessEntityCollision 0x6CFFD0` in `physics_oracle_test` (new `tests/standalone/physics_oracle_trailer.inc`; the test now also links Trailer.cpp and the small Col*/Sphere/BoundingBox/ColPoint TUs; `CEntity::GetColModel` / `RegisterReference` are forwarded to the exe's own and `CReferences::Init` runs per case; the exe's `ProcessColModels` function-local-static guard word 0x9689D0 is pre-set because its one-time init goes through the unusable exe CRT). Fake trailer + building / vehicle / ped / object, ground box + extras, `ocd` null in 1/3 of the cases, towing / towed relations, wheel and support line hits. The whole result blob is compared; CColPoint bytes 0x20..0x2B of line hits are stack garbage in the exe (and zero in the port's `std::array{}`) and are not behaviour; PC53 tolerates a few ulp in the col points written to the output array.
- final code: 3000 cases x PC24/PC53: **PC24 0 mismatches** (2991 cases with a non-trivial result); PC53 24 informational (ulp differences of `CCollision::ProcessColModels` in 53-bit mode: not bit-exact there, separate observation).
- old Trailer.cpp: access violation at address 4 (the user's crash) in the first null-`ocd` case.
- only the null-ocd fix (without 2-5): **35 PC24 mismatches** (hit-wall precedence, object with bDisableCollisionForce).

## 2. Area code of interior peds
Question: interior peds spawned in the LSPD interior have area code 0 while `currArea` is 6; two far cops are stuck in task 1307 (`TASK_WAIT_FOR_MATCHING_AREA_CODES`).
- Static: `CEntity::CEntity` (0x532A90) clears byte +0x2F; `CPopulation::PopulateInterior` (0x616470) -> `AddPed` (0x612710) contain no store to it; of ~80 byte stores to `[reg+0x2f]` in the exe the only ped related ones are `CEntryExit::TransitionFinished`/`WarpGangWithPlayer` (player and his gang) and the replay/save code.
- Measured in the ORIGINAL exe (`pti_probe/orig_int`, fixed poller): in the LSPD interior (player area 6) **all 15 peds in range have area=0** (cops type 6, models 280, others type 4/5). Their contact entities: near rooms model 14846 area 6, far rooms (d 47-54) model 14847 **area 10**. After the 2-star cheat the far cops get `p1:1307` (WAIT_FOR_MATCHING_AREA_CODES) in the exe too (`p1:1307>-1 p3:912>1103>1101>1000>1001>907...`, 3 cops at d 47-54, `orig_int/probe.txt`), exactly like the port (4 samples of `tasks=1307` at d=45-51, contactArea 10 peds present, run `out_int2`, 2 stars).
- Verdict: **original behaviour**. The cause is not the area code of the cop (always 0 in both) but `CTaskComplexKillPedOnFoot`'s epilogue / `CEventScanner` comparing the contact entities' area codes (target on a floor of area 6, cop on a floor of area 10): `CEventAreaCodes` -> `CTaskSimpleWaitUntilAreaCodesMatch`. Nothing to fix in the port.
- The probe now logs ped type and contact entity model/area (`pedpos` line, 6639f22d).

### Tool bug (ped pool read)
`orig_probe_pti.py` (PT2-INTERIOR) and `orig_probe.py` (PT-COMBAT) walked the ped pool with stride 0x7A4. The exe's pool element size is 0x7C4 (`CPools::Initialise`: `imul eax, edi, 0x7c4` in the pool constructor 0x5503F0). The bytes read were mis-aligned garbage, so every position check failed. Also wrong: ped type at +0x594 (`field_594`; `m_nPedType` is at 0x598), intelligence at +0x440 (it is 0x47C: `mov ecx, [edi+0x47c]` in PopulateInterior, 0x6165A2) - the task chains were always empty. Fixed in `C:/gta-scratch/pti_probe/orig_probe_pti.py`, `C:/gta-scratch/ptc_probe/orig_probe.py` and copied to `.notes/reports/PT2-TRAILER_tools/`; `orig_probe_pti.py` additionally prints each ped's contact entity (+0x584: model and area code) and the player's. Vehicle pool stride 0xA18 was already right (verified, 0x5504C8 `imul eax, eax, 0xa18`).

## Verification
- Release build (StandaloneRelease) ok; oracle tests (24 `*_oracle*`/`x87`/`wave`/`world`/`script`/`pool`/`logic` ... targets) ok incl. the new row; device `rw_*` tests and the 3-minute soak: see below.
- `run_all_tests.sh` (worktree, StandaloneRW Debug, exe oracle ON): 24 oracle-type tests (run without the lock) + 21 `rw_*` device tests (under the GPU lock) = 45/45 ok, 0 SKIP.
- Soak 3 min (StandaloneRelease, the final tree): RESULT OK, 0 errors, 10500 frames, 42 screenshots; the one gate timeout (`until invehicle` 8 s) is the known route step that proceeds on its retry.
- The user's `Documents/.../gta_sa.set` was left as the last running agent wrote it (another agent's game was running when the soak ended); the user's original copy is `C:/gta-scratch/playtest1/gta_sa.set`.

## Observations, not changed
- `CCollision::ProcessColModels` (port) is not bit-exact against the exe in the 53-bit FPU mode (a few ulp in the output col points; PC24 / game mode is exact in this row); it has no row of its own.
- The nullcol repro makes the trailer fall through the world, so the rig scene is a crash-condition test, not a driving test.
