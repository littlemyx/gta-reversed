# ASM-S01b (SeekEntity<RadiusAngleOffset> x8, SeekEntity<Standard> x8, ArrestPed x2) - 2026-10-28
Result: 18 compared (8 FIXED in 2 commits: both instantiations share one template), 10 MATCH, 0 UNSURE, 0 not reached. Table: S01b.result.tsv.
The two instantiations differ only by field offsets after the calculator (+0x44/+0x48 vs +0x4C/+0x50); TaskComplexSeekEntity.h covers Standard, RadiusAngleOffset and XYOffset.
Fixes:
- MakeAbortable (0x46ACF0 / 0x493810, XYOffset 0x661EB0): leisure abort stored -1 to m_scanInterval; the exe stores it to +0x10 = m_seekInterval. Scan timer on success = (now, 0).
- CreateFirstSubTask (0x46F6A0 / 0x496600): scan timer started unguarded, seek timer skipped only for interval == -1 (was a >=0 guarded Start), arrived only when dist^2 < max^2 (equal = too far).
- CreateNextSubTask (0x46EFF0 / 0x496080): null entity => FINISHED (port dereferenced it in GetSeekPos); STAND_STILL => CreateFirstSubTask unless ped->m_pAttachedTo (port always made a new StandStill).
- ControlSubTask (0x46F800 / 0x496760): boat attach used ped - boat (exe: boat - ped, mirrored position and heading) and dropped the FindZCoorForPed result; commonBoat stops at the first shared non-boat vehicle; scan restart unguarded; Say used IsFollower (exe IsMember); Say skipped on the two GoTo/FollowNode CreateSubTask returns.
SYSTEMIC_TODO CTaskTimer::Start sites in SeekEntity: scan timer (CreateFirstSubTask, ControlSubTask restart) unguarded in the exe, now StartUnguarded; the seek timer test is `!= -1`.
MATCH: ctor, dtor, Clone, CreateSubTask of both instantiations; ArrestPed CreateFirstSubTask 0x6907A0 and ControlSubTask 0x68D350 (constants 25.0 / 2.0 / 3.0 / 0.25 read from the exe).
Consumers: ArrestPed and AnyMeans XYOffset use the same template.
Dumps: C:/gta-scratch/asm-s01b/.
