# ASM-S04c (CTaskComplexGangFollower x3 + CTaskComplexGangLeader x8) - 2026-10-28
Result: 11 of 11 compared in full (1 FIXED in 1 commit, 10 MATCH, 0 UNSURE, 0 NOT_REACHED). Table: S04c.result.tsv.
Fix (exe address):
- 6943b280 CTaskComplexGangFollower::ControlSubTask (0x662C9E-0x662D0A): NaN edges only. The "too far from target" test is `!(sq <= (r+1)^2)` (fcompp + jp is taken for NaN), and the slow-down clamp via 0x420800 returns its 2nd operand on NaN (std::max returned the first).
MATCH (details in the tsv): Follower CreateNext/CreateFirst; Leader dtor, GetRandomGangAmbientAnim (DoCoinFlip == rand()>=0x3FFF is the exe-faithful helper; default branch 0x117 + rand(0,8)), Abuse/Attack speech, CreateNext/First, ControlSubTask, ScanForStuff.
Notes: ScanForStuff keeps the exe quirks (wander task looked up on the leader ped, vehicle loop without break unless FIX_BUGS); closest-member search starts at FLT_MAX vs 1e9 in the exe, unobservable behind the `<= 100.0` test.
Consumer audit: no signature or layout change.
