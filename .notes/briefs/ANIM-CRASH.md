# ANIM-CRASH — access violation in CAnimManager::UncompressAnimation <- CAnimBlendAssocGroup::CopyAnimation <- CTaskSimpleFall::ProcessPed

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh anim-crash.
## Evidence (seen twice on 2026-10-23, independent runs)
1. ORACLE-C-VEH-2 soak run 1 (Release, NOVSYNC, 3 min new game): AV at the end of the rhino phase, same stack (see .notes/reports/ORACLE-C-VEH.md, continuation 2).
2. REGRESS-SETUP suite 2, scenario cheats: C:/gta-scratch/gr_out/regress-setup-2/cheats/log.log around line 453 — AV 0xc0000005
   at 0x00b75adf, read of address 0x0000000a (a near-null pointer + 0xA), EDI=0, EBX=-1. The same suite also failed
   combat.foot.weapon_pickup and combat.chase.no_zombie_cops (maybe related). Rate about 1 run in 3.
Suite 2 most likely ran on a tree WITHOUT the ORACLE-C-VEH-2 fixes, so do not assume they caused it — check.
## Hypotheses to test (cheap first)
- An anim block/hierarchy that is not loaded (or was removed by streaming) when the fall task asks for its anim: CopyAnimation on
  a group whose m_pAnimBlock / hierarchy is null; check AddAnimation/CopyAnimation/UncompressAnimation against the exe asm
  (block refcount, CAnimManager::RemoveAnimBlockRef, ms_animCache) and the fall task's group/anim ids.
- A recent oracle "fix" in Animation/ or streaming that changed refcounts or indices — git log -S / bisect on soak with a seed loop.
## Task
Reproduce (soak.sh loop or the cheats scenario of gameplay_regress.sh, silent, private USERDIR, GPU via gpu_locked.sh), root-cause,
fix as the exe does (asm address in the commit, consumer audit). If an oracle row is possible, add it and show it fails pre-fix.
Acceptance: the repro loop that crashed ~1/3 runs gives 0 crashes in ≥6 runs; soak OK; run_all_tests green. HARD budget 250k tokens / 100 min.
If not reproduced in 40 min, stop and report what you ruled out (like SKIN-FLAKY).
## Do not touch
Grass/2dEffect/platform and their oracle tests (ORACLE-W3-MISC works there).
