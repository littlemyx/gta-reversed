# ASM-P4S10h: large unreached rows of the P4S10 queue

Order taken (queue order = descending size/score): CPed::UpdatePosition, CEventDamage::ComputeDeathAnim, CTaskComplexWalkRoundObject::ControlSubTask, CBike::DoSoftGroundResistance, CTaskSimpleThrowControl::ProcessPed, CPlayerPed::ProcessGroupBehaviour, CTrailer::ProcessEntityCollision, CTaskComplexFollowNodeRoute::ControlSubTask, CVehicle::ProcessOpenDoor (partial). Not reached: ScanForBlockedNode, FindPedToAttack, BlowUpCarsInPath, ControlGun, GoToPointAnyMeans::ControlSubTask, ApplySpringDampening, FindPositionOnTrackFromCoors.

Fixes (one line each):
- CEventDamage::ComputeDeathAnim (exe 0x4B3A60-0x4B3F04): RAMMEDBYCAR/RUNOVERBYCAR, direction 0: piece map right arm -> KD_RIGHT, any other non-left-arm piece -> KO_SKID_BACK (port: piece 0 -> KD_RIGHT, right arm -> KO_SKID_BACK). Knock-back force direction: CVector2D::Normalise of (dx, dy), z fixed to 5.0 (port normalised the 3D difference). [017964190]
- CTaskComplexFollowNodeRoute::ControlSubTask (exe 0x671AB0-0x671CB0): the GO_TO_POINT_FINE move-ratio update needs m_bSlowingDown; speeding-up is a separate branch (m_bSpeedingUp + FINE, CalcBlendRatio(ped,false); br>=0 keeps going, br<0 clears the flag, SetMoveRatio(BaseRatio) and returns GO_TO_POINT). Port merged both under CalcBlendRatio(ped,true). [c399e2a03]
