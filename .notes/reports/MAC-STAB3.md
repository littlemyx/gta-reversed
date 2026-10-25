# MAC-STAB3: matrix-list ownerless link ROOT CAUSE (2026-10-25, portable/main)

Build dirs build/PortableMacStab2 (release) and build/PortableMacAsan (ASan, rebuilt from HEAD). Harness stab_run.sh (own NOTSA_LOCK_FILE per run), NOTSA_MATRIX_CHECK=1, caffeinate -dimsu held (UserIsActive 1), one GL process at a time. Scratch /private/tmp/claude-501/stab3/ (out/*.out).

## Root cause (proven)
`CTheScripts::ClearSpaceForMissionEntity` (0x486B00) ported with a lazy ped-in-vehicle test. Ghidra decomp of the exe (headless, 0x486B00):
1. FindObjectsKindaColliding into `int[16]`;
2. **pre-pass**: every entry != ourEntity with `type == ped && (pedFlags @0x46C & 0x100) /* bInVehicle */` is set to 0;
3. main loop over the remaining entries (null / ourEntity skipped); vehicle branch and ped branch are `if ... else if` (after `delete vehicle` nothing else touches it).
The port skipped `ped && IsInVehicle()` (bInVehicle && m_pVehicle) lazily inside the loop. When a colliding vehicle came first, its driver and passengers were deleted (`CPopulation::RemovePed`), but the same peds were still in `colEntities`; by the time the loop reached them bInVehicle / m_pVehicle were cleared, so the check passed and `entity->GetMatrix()` + `GetColModel()` ran on a FREED ped pool slot: `GetMatrix()` allocated a list-1 link owned by the dead slot. Nobody ever releases it (the slot's next tenant's ~CPlaceable frees only its own link) -> stale list-1 link, later the oldest-link eviction deref (E3) / checker failures. After `delete vehicle` the old code also fell through to the ped test on the freed vehicle.
Evidence: temporary env switch NOTSA_CSFM_LEGACY (old logic) + log of "ped not valid in the ped pool" on a 3 min carstress route (stab3_car.txt: spawnmany + 3-5 `roadveh` per city spot): legacy run L1 = `[csfm] DEAD ped visited` x2 (slots ...eef0 and ...f8a8; eef0 is the slot from STAB2's s2) and `[matrix] CHECK FAILED` from frame 6240 with owner = exactly those two slots (list1 link #345, `ped 1`), then guard FIRED x2 at frame 7639. Same route with the exe's pre-pass: 0 dead visits, 0 failures, 0 guard (F1). The temporary diagnostics were removed before the commit.

## Fix (all `#ifdef NOTSA_PORTABLE`; w0a_guardlines TEXT OK; Win32 view byte-identical, no A/B needed)
* TheScripts.cpp: exe pre-pass, lazy test dropped, `continue` after `delete vehicle`. **Win32 fidelity bug** -> PORTABLE_SYNC.md entry; Windows host should apply it unguarded.
* MatrixLinkList.cpp `Init`: m_pOwner of all links = nullptr (ctor leaves it uninitialised; ASan heap showed garbage owners in free links = checker false positive at frame 1, 12 "failures" in the first ASan run).
* Formation.cpp `FindCoverPointsBehindBox`: `(i - 2) % 4` -> `(i - 2) & 3` (exe 0x699FF0 uses `& 3`; `% 4` of a negative signed index read corners[-2 / -1], ASan stack-buffer-overflow in CCoverPoint -> CTaskComplexKillPedOnFootArmed). Also a Win32 bug (stack read; bogus cover points).
Consumer audit: ClearSpaceForMissionEntity's other readers = none (static, results only via world side effects); callers (CreateCarForScript, CarCtrl x3, EntryExit, GameLogic x3, Cheat) pass pos + entity unchanged. The pre-pass changes behaviour only for peds in vehicles (they were skipped before too when still alive). No state layout change.

## Acceptance
| run | binary | route | result |
|---|---|---|---|
| L1 | release, OLD logic (NOTSA_CSFM_LEGACY, temp) | stab3_car (3 min) | 2 dead peds visited, checker failed from frame 6240, 2 guard firings |
| F1 | release, fix | stab3_car | 0 dead visits, 0 checker failures, 0 guard |
| t1 | release, fix | stab3_soak1 (10.2 min script, 31 roadveh, 15 spawnmany combat rounds, 42 far tp) | exit 0 (796.7 s wall incl. start), 0 failures, 0 guard, 0 [pool] FATAL, 0 crashes |
| t2 | same | stab3_soak2 (40 roadveh, 12 combat, 56 tp) | exit 0 (657.7 s), 0 / 0 / 0 / 0 |
| t3 | same | stab3_soak3 (34 roadveh, 12 combat, 37 tp) | exit 0 (740.1 s), 0 / 0 / 0 / 0 |
| t4 | release, FINAL tree (adds MatrixLinkList Init + Formation fix) | stab3_soak2 again | exit 0 (644.4 s), 0 / 0 / 0 / 0 |
| a1 | ASan, fix (before Init zeroing) | stab3_car | 12 checker "failures" = uninitialised m_pOwner of free links (ASan heap) -> Init fix |
| a4 / a5 | ASan, FINAL tree | stab3_car / stab_combat (4 min) | exit 0, 0 checker failures, 0 guard; ASan reports below |
All runs: "Loading the Game" present (139 lines), clean exit via cmd:quit. Soaks t1-t3 ran on the tree before the two small portable-only commits MatrixLinkList Init + Formation (t4 reran one soak on the final tree). Script time is 10.2 min per soak (routes stab3_soak{1,2,3}.txt from stab3_gen.py: STAB1 mix + `carstress` blocks = spawnmany peds + 3-5 `roadveh`, which is what reaches ClearSpaceForMissionEntity).

## ASan on the same routes (final tree)
No heap-use-after-free on the GetMatrix path any more, no matrix finding. Remaining ASan reports are NOT in the matrix/ClearSpace path:
| report | where | status |
|---|---|---|
| global-buffer-overflow ModelInfo.h:92 (x3) | CStreaming::ProcessLoadingChannel:1904 at LoadLevel (known E2 of STAB1, the exe has the same harmless read) | left |
| heap-use-after-free rwmath_exact.h MultKernel (x13, carstress route only) | CAEFireAudioEntity::UpdateParameters (AEFireAudioEntity.cpp:126) -> FxSystem::GetCompositeMatrix: the fire fx parent matrix is the RwFrame of a building freed by `CStreaming::LoadScene -> DeleteAllRwObjects` (the roadveh command's own LoadScene / a far teleport); a stale fire sound keeps reading it | NOT fixed (fx/audio lifetime, exe likely reads the freed frame too); open item |
| stack-buffer-overflow Vector.h:227 (x3 in a2/a3) | CFormation::FindCoverPointsBehindBox `% 4` | FIXED (gone in a4/a5) |
So "ASan clean" is met for everything this task touches; the fire-audio UAF is a separate open bug.

## Not changed / notes
* STAB2's CMatrixLink::operator= fix stays (still a real latent bug, not the cause here).
* Eviction guard (STAB1) stays as defence; it never fired in any run on the final tree.
* Wall clock about 80 min of the 100 min cap.
