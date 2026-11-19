# ASM-P4S10f

Review of the two P4S10e fixes plus 20 more queue rows (26 rows in `asm_sweep/P4S10f.result.tsv`: 20 MATCH, 4 FIXED, 2 UNSURE; 6 of the MATCH rows are the earlier half-read P4S10e rows re-read in full).
Part A: 21e504b05 (CTaskSimpleClimb::GetCameraTargetPos) CONFIRMED; 62c55cdd2 (FollowNodeRoute::CreateFirstSubTask flag clear) CONFIRMED, but two further bugs in the same class were found.

## Changes (before -> after)
- CTaskComplexFollowNodeRoute::CreateFirstSubTask / CreateNextSubTask (exe 0x6718A9 / 0x671A8A), 2b8fe6ef1: CalcGoToTaskType was called when m_bUseBlending was clear -> called when it is set (`test flags, 8; je skip`).
- CTaskComplexFollowNodeRoute::CreateNextSubTask (exe 0x6719EE), 17cb1b369: TASK_SIMPLE_GO_TO_POINT_FINE (0x39e) returned nullptr -> advances to the next node like GO_TO_POINT (0x384).
- CTaskComplexTrackEntity::Clone (exe 0x65F4F9-0x65F515), afa905e98: ctor arg `b` always o.b -> remaining timer time (interval - now + start) when timer byte +0x38 is set (dead in practice).
- CTaskSimpleGetUp::MakeAbortable (exe 0x678049-0x67809A), 781909686: elapsed > ftol(timeStepMs * 3) -> elapsed > ftol(timeStepMs) * 3.

## Not finished
CBike::FixHandsToBars (head only), CTaskSimpleFight::IsTargetInRange (head only; port emulates exe out-of-bounds array reads). Clone rows verified for ctor-argument order only (member offsets of base classes not re-derived).
