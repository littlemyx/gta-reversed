# ANIM-CRASH - AV in CAnimManager::UncompressAnimation <- CopyAnimation <- BlendAnimation <- CTaskSimpleFall::ProcessPed

Rules v4. Start 19:02, end ~20:32 (90 min), 2026-10-23, worktree C:/gta-wt/anim-crash (removed).
**Status: reproduced once (1 of ~19 attempts), NOT root-caused, no fix.** Instrumented builds (patch ANIM-CRASH-diag.patch) ran 14 attempts clean, so the offending (group, anim id) was never captured.

## Crash anatomy (3 independent sightings, identical)
Logs: C:/gta-scratch/gr_out/regress-setup-2/cheats/log.log, C:/gta-scratch/ocv2_soak/log.log, C:/gta-scratch/animcrash/m-1/cheats.attempt1/log.log (my repro).
* Stack: UncompressAnimation +0xf (reads h+0xA with h == 0) <- CAnimBlendAssocGroup::CopyAnimation(AnimationId) +0x3e (GetAnimation(id)->m_BlendHier == nullptr)
  <- CAnimManager::BlendAnimation(clump, groupId, animId, delta) +0x1dc (AddAnimationAndSync inlined) <- CTaskSimpleFall::ProcessPed:74 (StartAnim inlined: the branch taken when the
  fall anim is not already running). Victims: a civilian ped (cheats), the player (soak). AV addresses 0x00b75adf / 0x00685aff / 0x00d85adf = UncompressAnimation+0xf in three
  different (ASLR-rebased) builds of the port.
* Registers are the SAME in all three builds: EBX=0xffffffff, EDX=0x3c0, EDI=0 (the null hierarchy), EAX-ECX=0x17b8, ESI-ECX=0x26b8. 0x3c0 = 40 * 24 = sizeof(CAnimBlendStaticAssociation[40]):
  the vehicle anim groups (tank, kart, BF_injection, bikev) are created with 40 association slots ("Created #40 associations for block (...)") while the block has only 4-6 anims, so
  most slots keep a null m_BlendHier. Hypothesis (unproven): BlendAnimation was asked for a slot of such a group (or one past the end, index 40), or EBX=-1 is groupId == ANIM_GROUP_NONE.
  The alternative reading (index 40 of the default group == ANIM_ID_HIT_BEHIND) was considered less likely.
* In every sighting a vehicle anim block had been streamed in 13-36 s earlier (tank / kart + BF_injection / tank). The crash falls in the cheats route during cheat_70 (PRIEBJ) or
  the 2nd cheat_40 press, or in the idle phase of the soak after the rhino cheat. NOT caused by the ORACLE-C-VEH-2 fixes (suite 2 ran before them; my repro exe is the 16:25 main Release build).
* Constructors of CTaskSimpleFall with a non-default group/id: EventHandler.cpp:1109/1122 (CEventDamage: e->GetAnimId()/GetAnimGroup(); ComputeDamageAnim copies the ATTACKER's
  fight combo group, EventDamage.cpp:449/735) and :1636 (knock-off-bike, ids from CEventKnockOffBike::CalcForcesAndAnims, group DEFAULT). Others (HitPedWithCar, ReactToAttack,
  StuckInAir, InAirAndLand, EnterCar) are all ANIM_GROUP_DEFAULT.

## Reproduction
* Loop: tools/standalone/gameplay_regress.sh -x <exe> cheats (GR_SKIP=render_firststart, private USERDIR, GPU lock), 2.5-6 min per run; scripts in C:/gta-scratch/animcrash/ (loop.sh).
* **FINDING FOR THE GATE:** gameplay_regress.sh RETRIES a scenario whose set-up failed (probes < 10, free_start) and reports only the retry. A crash in attempt 1 is hidden: only an
  "info" line plus `<scenario>.attempt1.result`. Of my ~19 attempts, 2 crashed in attempt 1 and were reported as PASS: m-1 (this AV, 19:20:43) and m-3 (a different AV, below).
  Both happened while my own ninja build was running (host load); the later attempts (no load, or only another agent's builds) did not crash. So it is timing/load sensitive,
  about 1 in 19 overall. Suggest: gr_check treats a crash in attempt 1 as a FAIL (or KNOWN-flaky), not as a retriable set-up failure.
* Table (attempts / anim AV / other AV): main 16:25 build 7 / 1 / 1;  diag1 (CopyAnimation log) 6 / 0 / 0;  diag2 (+ BlendAnimation range log) 8+ / 0 / 0. No DIAG line was printed
  in any run: normal play never requests a null/out-of-range slot, the bad request is rare.

## Second crash (separate, not investigated): CCollisionData::GetShadTrianglePoint:205 <- BuildShadowVolume:124 <- CStencilShadows::Process:478 <- CGame::Process:836
Read of address 3 (null collision data + 3), m-3 attempt 1 at 19:31:21 (cheat_56_OFVIAC, bikev block just loaded): an entity without collision data while its stencil shadow volume is built.
Candidate brief: SHADOW-COLDATA.

## Ruled out (by reading)
* CopyAnimation / UncompressAnimation / BlendAnimation follow the exe flow (the exe also dereferences the null hierarchy); the bad input is the static association.
* D-FIX1 block index -1 guard (Add/RemoveAnimBlockRef) is in place; EBX=-1 is a register value, not a write to ms_aAnimBlocks[-1].
* CTaskComplexFallAndGetUp(dir, time) leaving m_nFallAnimId uninitialised for dir > 3: all callers pass 0..3 (ComputePedHitSide / ComputeEntityHitSide).
* CTaskComplexHitPedWithCar default branch (anim id 0xD0): m_PedHitSide is 0..3.

## Next steps for a continuation
1. Reapply .notes/reports/ANIM-CRASH-diag.patch (logs group/id/idOffset/numAnims/block when BlendAnimation or CopyAnimation sees a null or out-of-range slot) and run the cheats scenario
   under host load (a legitimate concurrent build), or run it under cdb (C:/gta-scratch/dbg, `sxe av`) and read the CTaskSimpleFall fields (m_nAnimId/m_nAnimGroup) at the AV.
2. With (group, id) known: who passed it (EventHandler 1109/1122 via CEventDamage::m_nAnimGroup, EventDamage.cpp:293 From()), and does the exe guard it (BlendAnimation 0x4D4610 asm).
3. Oracle row for the fix once the caller is known (BlendAnimation itself AVs on both sides, so the row must test the caller's group/id choice).
