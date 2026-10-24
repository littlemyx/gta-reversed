# ANIM-CRASH-3 — root-cause the fall-task anim AV from a full cdb capture

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh anim-crash-3.
## Evidence (captured 2026-10-24 04:37, run 3 of the overnight loop, exe = HEAD 29c0823e Release copy C:/gta-scratch/night/gta_29c0823e.exe)
Full cdb log with .exr, registers, kv 40, disassembly, stack dwords: C:/gta-scratch/night/keep/cdb-043708.log (scenario output r3/ next to it).
AV in CAnimBlendNode::SetupKeyFrameCompressed+0x9 <- CAnimBlendAssociation::SetCurrentTime+0xc5 <- Start+0x12 <- (inline)
CAnimManager::AddAnimationAndSync <- CAnimManager::BlendAnimation+0x204 <- (inline) CTaskSimpleFall::StartAnim <- CTaskSimpleFall::ProcessPed
<- CTaskManager::ManageTasks <- CPedIntelligence::Process <- CPed::ProcessControl <- CCivilianPed::ProcessControl (a civilian ped).
Same task/path as the earlier sightings (UncompressAnimation null hierarchy, .notes/reports/ANIM-CRASH.md, CRASH-HUNT.md): the fall task
blends an anim whose hierarchy/sequence data is missing (unloaded block, unfilled slot, or an anim never uncompressed).
Reproduction loop (no agent needed while it runs): .notes/reports/CRASH-HUNT.md section 1 (GR_WRAP=/c/gta-scratch/crash-hunt/cdbrun.sh);
rate ~1 hit in 3-20 cheats runs.
## Task
1. From the stack dwords / registers recover the BlendAnimation args (clump, group, anim id, blend delta) and the association/hierarchy
   pointers; if needed re-run the loop with a cdb breakpoint script that logs BlendAnimation args from CTaskSimpleFall (bu with .printf; no sampler).
2. Compare that path with the exe (CTaskSimpleFall::StartAnim / ProcessPed, CAnimManager::BlendAnimation 0x4D4610, AddAnimationAndSync,
   CAnimBlendAssociation::Start / SetCurrentTime, anim-block refcounting: AddAnimBlockRef / RemoveAnimBlockRef / ms_aAnimBlocks,
   CStreaming removal of anim blocks while a ped still uses them) and fix as the exe does (asm address, consumer audit, oracle row if possible).
Acceptance: ≥ 20 loop runs with 0 hits after the fix (pre-fix rate stated); soak OK; run_all_tests green. HARD budget 250k / 100 min.
## Notes
Also seen in run 1 of the same loop (not this brief): cheats.state_matches_original armour 97 != 100 after c44/c70 — log it only.
Data dirs under C:/gta-scratch/<id>/; never rm -rf a dir with junctions.
