# ASM-S07 (Tasks/TaskTypes, 41 functions) - 2026-10-28 - PARTIAL 11/41
Result: 11 compared (PolicePursuit pulled forward): 10 MATCH, 1 FIXED, 0 UNSURE; 30 rows NOT_COMPARED (budget). Tables in S07.result.tsv.
Fix:
- 27920f56 CTaskComplexPolicePursuit::ControlSubTask (0x690920-0x690AF9): when the arrest subtask runs and the cop is no longer in pursuit, the exe returns STAND_STILL if the cop's car is >=5 m away (or distance is NaN); the port returned NONE (kept arresting). The seek-entity test is strict (<25); the port used <=.
MATCH: PassObject::ControlSubTask, PlayHandSignalAnim x4 (dtor, CreateSubTask, CreateFirstSubTask, ControlSubTask), PolicePursuit::SetWeapon, PresentIdToCop::CreateNextSubTask, ProstituteSolicit CreateSubTask/CreateNextSubTask/ControlSubTask (whole bodies incl. flag bits, message ids, rand call order, stats ids).
Open range: from 0x697B70 (CTaskComplexReactToGunAimedAt x4) to the end of S07.tsv (RoadRage, ScreamInCarThenLeave x4, SeekCoverUntilTargetDead, SeekEntityAiming x3, ShuffleSeats x5, SignalAtPed x3, SitDownThenIdleThenStandUp x2, SmartFleeEntity x3, SmartFleePoint x4).
Unverified detail: CCamera field offset 0xB0 (m_nWhoIsInControlOfTheCamera) used by ProstituteSolicit::ControlSubTask was not re-derived from the header (address 0xB6F0D8 vs TheCamera 0xB6F028).
