# RD-AB-17 - the frame-145 rand divergence is a symptom: a driven car is shifted 0.0157 m in the port at fc 1048612 (frame 36); no fix yet

Agent RD-AB-17, rules v5, 2026-10-26 16:30 - 18:06 WEST. Worktree C:/gta-wt/rd-ab-17 (origin b2d82a54, removed). Data C:/gta-scratch/rd-ab-17 (r2 = rlog, e1..e7 = probe runs, e3 = P lines, e7 = P+Q lines). Own proxy RD_PX=C:/gta-scratch/rd-ab-17/px. Documents signature ee0a22fe... identical before/after every orig run.

## Equal prefix (orig-port, pin 1023, intro; orig = fresh runs and RD-AB-15 o1..o3)
No port fix landed, so the number is unchanged: 145 in every port run of this session (HEAD exe from RD-ENV with the 349-range map: r2; my own sccache worktree build, audio_rva ranges: 829 (the cache build splits more ranges; same prefix 145): b1 vs RD-AB-15 o1, e1..e7 with probes). A 3-rep final was not run (no change to measure).

## Divergences
| frame (fc) | layer | cause | fix / exe address |
|---|---|---|---|
| 145 (fc 1048721) | rand count: port 4 extra CDecisionSimple::MakeDecision rolls (DecisionSimple.cpp:33) | SYMPTOM. Port ped 4 (model 19) gets a new EVENT_POTENTIAL_WALK_INTO_PED (13) in CEventScanner::ScanForPedPotentialCollisionEvents (0x606580): CEventGroup::Add -> 3x ComputeResponseTaskOfType + ComputeResponseTaskType = 4 MakeDecision rolls (exe: bestPed==0 for all 3 scanned peds at fc 1048719..1048722, no event, no roll). Reason: the scanned peds' positions/targets already differ from the exe (ped model 19 pos 451c5a92 vs 451c5aa2, go-to targets differ, ped model 21 451c1715 vs 451c1f97) | none - upstream state differs |
| 36 (fc 1048612) | physics of a DRIVEN CAR (ped slot 5 model 106 is the driver; its position is set from the car by the task SetPedPosition in CPedIntelligence::ProcessAfterProcCol 0x6018F0, found with a write watchpoint on the ped matrix: eip 0x59bbd5 CMatrix::operator=, stack 0x601927 <- 0x568a2b CWorld::Process+58b) | car slot 7 (model 492 Greenwood) is shifted in the port only: pos x 4524a75a vs 4524a79a (-0.0157 m, y -0.0006, z +0.0001), move speed 2 ulp, forward y differs 1e-3. One-off (the offset persists, speed afterwards equal). The ped P line shows it as dx -0.2612 vs -0.2456 in that frame only. Not in CPhysical::ProcessShift/ApplySpeed of the ped (call order of 0x54DFB0/0x547B80/0x54D920/0x54DB10/0x54DEC0 and CPed 0x5E8CD0/0x5E1B10/0x5E1FA0 equal). NEXT: trace the call order of car slot 7 in fc 1048612 (RD_BPS 54DFB0/54DB10/547B80/54D920 with the vehicle in ecx; RD_PSLITE logs "ped" slot - extend to the vehicle pool) and the vehicle collision branch (CAutomobile::ProcessControl / ProcessEntityCollision 0x6ACE90) | - |
| 4..15 (fc 1048580..1048596) | tiny physics drift of parked/idle cars | vehicle slots 1,4,5,8,10 (models 492/600/481): move/turn speed 1-2 ulp off from fc 1048580 (slot 1), 1048582 (slots 4,5), 1048595/6 (slots 8,10); positions equal. Invisible to the F line, not analysed (suspension / ground contact x87 candidates, CAutomobile::ProcessControl) | - |
| 43 (fc 1048612+) | ped slots 1,2,3,4,8 (models 79,21,22,19,106) | 1 ulp speed drift (fc 1048665..679) and slot 2 aim rotation 403dd6ba vs 3ff80fea at fc 1048648 | not analysed |

## Tooling added (inert without env / pin)
- pin bit 32768 (with 1024): "Q fc slot model pos(3) movespeed(3) turnspeed(3) fwd(3) status" for every vehicle-pool slot, rd_orig.c (pool 0xB74494, stride 0xA18) and Lockstep.cpp (same format). Compare per slot like ucmp/pdiff (my one-off script: group by slot, first differing fc).
- rd_orig.c `RD_WPED=1` with `RD_WATCH=1 RD_WDYN=<ped slot>:30 RD_WLEN=4 RD_WFC=<fc> RD_WN=0`: hardware write watchpoint on a ped's matrix x (ped+0x14 deref + offset): "V fc newvalue eip <call-site candidates>" names the writer of a ped position. Lesson: a ped that moves 0.25 m/frame with moveSpeed 0 is a car driver.
- Lesson: `rd_diff` hashes only the player, pools counts and rng; run with pin 2047/34815 and pdiff.py FIRST - the first P/Q line difference (fc 1048580 here, 65 frames before the F-line divergence) is the real starting point; every rand-count divergence so far was a downstream effect.
- Lesson: R-log rcmp.py frame number = F-line index + 1; pass pin 1023 only (1023 already contains 32; 1055 turns it into 1024+31 and breaks the run).

## Gate
Release build ok (cache build, probes); soak, run_all_tests, gameplay_regress NOT run (no port behaviour change; only the env-gated Q-line block in Lockstep.cpp and rd_orig.c changed).
