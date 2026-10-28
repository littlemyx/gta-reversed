# ASM-S08b (UsePairedAttractor x5, UseSwatRope::ControlSubTask, UseWaterCannon x2) - 2026-10-28 - COMPLETE 8/8
Result: 8 rows: 6 MATCH, 2 FIXED in 2 commits. Table in S08b.result.tsv. Tests (same worktree as S09): wave2_oracle_test 1142 functions PC24 0 / PC53 0 mismatches; combat regress PASS=14.
Fixes (run build, no FIX_BUGS guard needed):
- UsePairedAttractor::ControlSubTask (43a60490): partner-wait with a partner that has no paired-attractor task clears the partnership AND aborts the sub-task (port only cleared); use-partner aborts unless the partner has the paired task AND (use-partner | wait) (port: the three types were or-ed in one Find).
- UseSwatRope::ControlSubTask (7931b42f): CRopes::FindCoorsAlongRope failing also tries the abort to TASK_NONE (port just returned the sub-task); a failed ground-abort returns the sub-task without moving the rope along (port still advanced it).
MATCH: UsePairedAttractor CreateSubTask / SeekPartnership / CreateNextSubTask / CreateFirstSubTask, UseWaterCannon CreateFirstSubTask / ControlSubTask.
Notes: SeekPartnership's early return after the first match is equivalent (every later iteration `continue`s once m_Partner is set). UseSwatRope z-2 compare is done in float in the port (exe: extended in PC24 = float).
