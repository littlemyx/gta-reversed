# ASM-P4S10i

Review of the two P4S10h fixes plus the P4S10h NOT_REACHED rows (oracle: gta_sa_compact.exe 1.0).

| function | exe range | before -> after |
|---|---|---|
| CVehicle::ProcessOpenDoor | 0x6D56C0-0x6D5C25 | cur-ratio guard (open only while cur > new) on CLOSE_* cases -> on CLOSEDOOR_* cases (0x6D5826); CLOSE_* (0x6D5980) opens unconditionally |
| CPlayerPed::FindPedToAttack | 0x60C5F0-0x60C7BE | initial best distance FLT_MAX -> 99999.9f (0x47C34FF3) |

Confirmed (no change): CEventDamage::ComputeDeathAnim, CTaskComplexFollowNodeRoute::ControlSubTask.
MATCH: ScanForBlockedNode (both overloads), BlowUpCarsInPath, ControlGun, GoToPointAnyMeans::ControlSubTask, ApplySpringDampening, FindPositionOnTrackFromCoors.
Not reached: further rows of P4S10.queue.tsv.
