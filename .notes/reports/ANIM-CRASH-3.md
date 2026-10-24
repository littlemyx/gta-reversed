# ANIM-CRASH-3 - fall-task anim AV: root cause, fix, verification

Rules v4. Start 04:40, end ~06:20 (2026-10-24), worktree C:/gta-wt/anim-crash-3 (removed). Scratch: C:/gta-scratch/ac3/ (probe.txt, cdbrun_probe.sh, run1.sh, loop.sh, logs/, out/).

## Recovered arguments (overnight capture cdb-043708.log)
BlendAnimation frame (cdecl, return 0x0103067c = CTaskSimpleFall::StartAnim inlined in ProcessPed): clump 0x1f05a3c8, **group 0 (ANIM_GROUP_DEFAULT), anim id 0xC0 = 192 = ANIM_ID_DOOR_LHINGE_O**, blend 8.0f.
Faulting access: CAnimBlendNode::SetupKeyFrameCompressed+0x9 `cmp word [esi+6],1` with esi = node->m_Sequence = 0xEA280002 (garbage): id 192 is past the end of the default group's association array, so
CopyAnimation/AddAnimation built the association from whatever lay behind it (null hierarchy in the earlier sightings, a garbage hierarchy here).

## Root cause
0xC0 is a sentinel, not an animation: CEventDamage::ProcessDamage stores id = DOOR_LHINGE_O and m_bFallDown = false when the ped's active task refuses MakeAbortable (exe 0x4B473E `mov [esi+2Ch],0C0h`; port EventDamage.cpp:891).
The exe's CEventHandler::ComputeDamageResponse 0x4C0170 handles it at 0x4C042B/0x4C0488: `test [e+25h],2` (m_bFallDown) -> CTaskComplexFallAndGetUp; else `cmp id,0BFh / cmp id,0C0h -> jmp 0x4C062D` (no response task); else the partial-anim / CTaskSimpleBeHit branch.
The port had `if (!fallDown && !contains({0xBF,0xC0}, id)) {...} else { FallAndGetUp }`, i.e. it folded the sentinels into the fall branch: a CTaskComplexFallAndGetUp(192, DEFAULT) was created and its CTaskSimpleFall::StartAnim blended slot 192.
Live proof (pre-fix exe, cdb breakpoint on the CTaskComplexFallAndGetUp ctor, run p1, the first probe run): `FALLCTOR id=192 grp=0 t=1000` from ComputeDamageResponse <- ComputeEventResponseTask <- HandleEvents, then the same AV
(CAnimBlendNode::SetupKeyFrameCompressed+0x9 <- Start <- BlendAnimation <- CTaskSimpleFall::StartAnim, BlendAnimation args (clump, 0, 0xC0, 8.0f)). Log: C:/gta-scratch/ac3/pre-fix-hit-cdb.log.
The earlier sightings (null hierarchy at UncompressAnimation, EBX=-1, EDX=0x3c0) are the same slot-192 read with different heap contents behind the array; the "vehicle anim group with 40 slots" hypothesis of ANIM-CRASH was wrong.

## Fix (EventHandler.cpp, CEventHandler::ComputeDamageResponse)
* `if (m_bFallDown) FallAndGetUp; else if (!contains(0xBF,0xC0)) {partial/BeHit}` (nothing for the sentinels) - as the exe.
* Same block, two more deviations from the asm: ids 0x1C..0x1F use CAnimManager::AddAnimation 0x4D3AA0 (port: BlendAnimation) when the clump has no such association (0x4C04F8), and both partial paths set e->m_bAnimAdded (0x4C051C / 0x4C0551 `or [e+25h],4`; the port set it only for ids outside 0x1C..0x1F).
* Consumer audit: only the response task chosen for one event state changes; no shared layout/index/flag meaning. Readers of m_bAnimAdded: CTaskSimpleBeHit init (copies it) and later ComputeDamageResponse calls on the same event (the `!GetAnimAdded()` test) - now as the exe.
* Oracle row: not added. ComputeDamageResponse starts with e->ProcessDamage(ped) (world, FX, audio and task-manager state) and has no row today; the sentinel only arises from ProcessDamage's MakeAbortable failure, so a faithful row needs the whole fixture. The proof is the live capture + the asm.

## Verification
* Pre-fix: probe run p1 (HEAD 29c0823e Release, cheats under cdb): 1 of 1 runs hit (id=192 -> AV). Overnight loop: 1 hit in 3 runs; earlier sightings 1 in 3-20.
* Post-fix (this build, same probe, cheats scenario under cdb, GR_NORETRY): **11 complete runs (18 cheat probes each, route to the end), 0 AV, 0 CTaskComplexFallAndGetUp with id 192**; 35 fall tasks created in them, 20 of them by ComputeDamageResponse (ids 23-27, fall-down path), otherwise only valid ids (+ 191 from ComputeKnockOffBikeResponse, handled by StartAnim). The requested >=20 runs were not reached: the GPU lock is shared with REPLAY-DIFF's loops (a run took 3-6 min incl. waiting) and the 100-minute cap. The mechanism proof (live capture pre-fix, asm, no sentinel reaches the fall task post-fix) carries the claim more than the rate.
* Soak (fixed exe, NOVSYNC, route 393 s): RESULT OK, 0 errors, 21 markers (C:/gta-soak-out/20261024-051231).
* run_all_tests (worktree, StandaloneRW, exe oracle ON): NOT complete: stopped at the 100-minute cap after wave3_input_oracle_test (about 15 oracle tests - wave3_interior onward, wave2, world, x87 - not reached); the rw_* device tests crashed (exit 139, 6 attempts each) while a game ran concurrently on the GPU - unrelated (no rw code touched); the completed oracle tests have 0 PC24 mismatches excluding NaN-payload-only (wave3_events 99 functions 0/0, wave3_anim 0/0, logic, review, script g13-g26 0).

## Side notes
* cheats.state_matches_original FAIL (armour 97/90/84 != 100 after c44/c70) seen in post-fix run n-4 (and run 1 of the overnight loop): logged only, not investigated.
* cheats.free_start SETUP (mission=1) in 5 of 11 cdb runs: the tutorial-bike mount is load/timing dependent under cdb (as in CRASH-HUNT); the route still ran to the end in all of them.
* gameplay_regress.sh hides an attempt-1 crash (see ANIM-CRASH.md); GR_NORETRY=1 avoids it.
