# CRASH-HUNT — continuation of ANIM-CRASH: capture group/id of the null-slot crash, plus the stencil-shadow null coldata crash

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh crash-hunt.
## Evidence
Read .notes/reports/ANIM-CRASH.md first (whole file, incl. "Next steps") and .notes/reports/ANIM-CRASH-diag.patch.
1. AV in CAnimManager::UncompressAnimation +0xf (h==0, reads h+0xA) <- CopyAnimation(AnimationId) <- BlendAnimation(group,id)
   <- CTaskSimpleFall::ProcessPed:74. EBX=-1, EDX=0x3c0 (= 40 * 24). Vehicle anim groups (tank/kart/BF_injection, 40 slots for 4-6 anims)
   streamed 13-36 s before each crash. Rate ~1 in 10-19 `gameplay_regress.sh cheats` runs, more under CPU load (own build running).
2. AV in CCollisionData::GetShadTrianglePoint:205 <- BuildShadowVolume:124 <- CStencilShadows::Process:478, read at address 3 (null
   collision data); once, 2026-10-23 19:31:21, cheats scenario. Logs under C:/gta-scratch/animcrash/.
## Task
- Capture the real group/id/idOffset/numAnims (diag patch) or run the cheats scenario under cdb/WinDbg (`sxe av`, `kv`, `dv` in the
  fall-task frame) — check first whether cdb exists (Windows SDK Debugging Tools); do NOT install anything system-wide; do NOT suspend the
  game thread with a sampler. Load the machine (a parallel low-priority ninja of your own worktree) to raise the rate.
- Trace how the group reached the fall task (EventHandler 1109/1122 via CEventDamage::m_nAnimGroup) and compare with the exe
  (BlendAnimation 0x4D4610, the event-damage anim group setter); fix as the exe does (asm address, consumer audit, oracle row if possible).
- Shadow crash: why a shadow-casting entity has null col data (model/col streamed out while in the shadow list?) vs the exe.
Acceptance: each fixed crash 0 in >= 15 loaded cheats runs (vs the pre-fix rate); soak OK; run_all_tests green. HARD budget 250k / 100 min.
If neither is captured in 50 min, stop and report what was captured and ruled out.
## Do not touch
game_sa/Tasks/TaskTypes oracle tests (ORACLE-C-TASKS works there; a fix in TaskSimpleFall.cpp itself is fine, tell it in the report).
