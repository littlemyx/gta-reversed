# ASM-P4S10g

Review of the four P4S10f fixes, the two P4S10f UNSURE rows, base-class offset re-check of the P4S10f Clone rows, and 18 further Clone rows (34 rows in `asm_sweep/P4S10g.result.tsv`: 34 MATCH, 0 FIXED).

## Changes (before -> after)
None. All four P4S10f fixes were re-read line by line against the exe and are CONFIRMED:
- 2b8fe6ef1 CTaskComplexFollowNodeRoute::CreateFirstSubTask/CreateNextSubTask (0x6718A9/0x671A8A): CalcGoToTaskType gated on m_bUseBlending SET = exe.
- 17cb1b369 FollowNodeRoute::CreateNextSubTask (0x6719EE): GO_TO_POINT_FINE joins the GO_TO_POINT block = exe.
- afa905e98 CTaskComplexTrackEntity::Clone (0x65F4F9): remaining-time arg = exe.
- 781909686 CTaskSimpleGetUp::MakeAbortable (0x678049): ftol(timeStepMs) * 3, strict > = exe.

CBike::FixHandsToBars and CTaskSimpleFight::IsTargetInRange finished: MATCH (full body read, constants and enum ids checked).

## Not reached
Big queue rows (CPed::UpdatePosition, CEventDamage::ComputeDeathAnim, ControlSubTask rows, CBike/CAutomobile physics) were not started within the budget.
