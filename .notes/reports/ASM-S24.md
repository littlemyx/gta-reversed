# ASM-S24 (PedIntelligence.cpp, 31 functions) - 2026-10-26 21:19 .. see hand-back
Result: 31 compared (3 FIXED in 3 commits, 28 MATCH), 0 UNSURE, 0 NOT REACHED (table in S24.result.tsv). Slice complete.
Fixes (exe address, one line each):
- Constructor (0x607140): m_mentalState was never initialised - the exe clears the anger level and starts m_AngerTimer (start = now, interval 0, started, not stopped; 0x607244..0x607280).
  Without it CTaskTimer::IsOutOfTime() was false forever, so IncrementAngerAtPlayer (CarCtrl, CollisionEventScanner, EventHandler callers) never raised the anger and CEvent(Low|High)AngerAtPlayer never fired.
- GetUsingParachute (0x6011B0): only the first partial association was checked against the "parachute" anim block; the exe loops over the whole chain (0x6011F0..0x60121D).
- TestForStealthKill (0x601E00): the target's current event vetoes the kill when this ped is its source and the target hates/dislikes this ped's type, with no group needed (0x601F5B..0x601FC7);
  the port also required a target group + a group event. The group-event check stays a separate second check (no-op in the exe: CEventGroupEvent::GetSourceEntity at 0x43E1F0 returns null).
MATCH: everything else (constructor field init incl. gang ranges 40.0, destructor, task getters and their type ids, AddTask*, AddTaskPrimaryMaybeInGroup, ClearTasks, FlushImmediately, ProcessFirst,
ProcessAfterProcCol, ProcessAfterPreRender, LookAtInterestingEntities (rand order, FindObjectsInRange args), ProcessStaticCounter, Process, ...).
Notes: ProcessStaticCounter compares the counter unsigned in the exe (port int32) - differs only after 2^31 frames; TestForStealthKill calls GetBonePosition before the duck/health checks in the exe (no side effects
with updateSkinBones=false); the exe leaves m_vecLastPedPosDuringDamageEntity and the CMentalState health bytes uninitialised (pool memory), the port keeps its defaults. Consumer audit: no layout or signature change;
the ctor change enables the anger events exactly as in the exe.
Gate: Release build OK (1186/1186); no oracle target covers CPedIntelligence (the script oracles stub the class); soak result in the hand-back.
