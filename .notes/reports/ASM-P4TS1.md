# ASM-P4TS1: the 32 "C++ still differs" rows of the TS audit

Oracle: gta_sa_compact.exe 1.0. Build: StandaloneRelease links (ninja -j4). No game run, no tests run.
GetNearestTag (de5ec0895, 0x49D160): CONFIRMED (loop ecx=count..1, fcom + `test ah,5; jp` = strict <, higher index wins a tie).

Fixed (14 functions, commits ASM-P4TS1 on branch p4ts1): GangWarGoingOn, CTheScripts::Init, EventHandler gang-drive-by (passenger test inverted), GET_CLOSEST_CHAR_NODE (flag), IsCarSprayable (artict1), DettachPedFromEntity (TS calls it Attach), Pickups outZ + effect axes, FindTotalPedsKilledByPlayer (400), SwatRope health (<), UseScriptedBrain priority, CarAI stuck condition (negated), SHUT_CHAR_UP (enable path), ComputeResponseNewGangMember (inverted), CLEAR_CHAR_LAST_WEAPON_DAMAGE (null handle).

## For TS: TS is wrong here
- CObject anim time step 0x535F75: the C++ tip already multiplies (Timer::GetTimeStepInSeconds = ms_fTimeStep * 0.02f).
- ScriptGenerateOneEmergencyServicesCar 0x42FC22: C++ calls the 1-arg CAutoPilot::SetCarMission, which only stores the mission (no timer restart); already as the exe.
- CConversations yes key 0x43A8D2: the C++ upper-cases it only under IsFixBugs; the run build matches the exe.
- AttackWaveOvercome 0x445B30, CEventGunShotWhizzedBy::AffectsPed 0x4B5120, CTaskSimpleSitIdle 0x637A60: already fixed in C++ after 0372fa9f4 (comments cite the exe).
- Naming: 0x5E7F99 is CPed::DettachPedFromEntity, not AttachPedToEntity.

## NOT_REACHED (budget): attractor placer/pizza/shelter, CCam::Process, GenericLoad, HandShaker, AttractorManager order, MissionOps61_90 / FollowPatrolRoute / GoPickUpEntity x87 chains, SimpleVariables language flags, UseAttractor ATM, SwatRope check order.
