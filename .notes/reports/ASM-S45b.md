# ASM-S45b (phase 2: CEntryExitManager + CEntity::PreRender tail + ProcessLightsForEntity) - 2026-10-30
Result: 11 rows = 4 MATCH + 6 DEVIATION (fixed) + 1 NOT_REACHED. Table: S45b.result.tsv.
Deviations fixed:
- LinkEntryExit (5c140c6d7): the back link test was inverted (set only when the target ALREADY had a link, i.e. never for a fresh pair), so the paired exit of every CREATE_LINKED_PAIR enex had no link back; AddOne now tests the flag (0x43FCB7) instead of LinkEntryExit.
- AddOne (eef4ad9ec): the quad tree box is built from two opposite corners (-/+ range*0.5) rotated with CMatrix::SetRotateZ taking the angle in DEGREES (not converted - exe quirk), moved to the centre and ordered with the fcom/jp arms; the port rotated the rect corners by radians around the rounded centre (different tree membership -> different "which enex is picked"). The unknown-burglary (0x400) time window uses rand() < 0x3FFF (port RandomBool(50) maps differently). Name copied with strncpy(8): an 8 char name aborted in strcpy_s.
- FindNearestDoor (c1788db97): best distance starts at the radius, sqrt distance, last-to-first walk (ties).
- ResetAreaCodeForVisibleObjects (f345ed0e3): counter left at -1 (the exe walks off the list on a double reset; the port keeps a safe guard).
- Init (4bdcc6004): pool name string.
- CEntity::PreRender tail (9930b211a): crane/hook shadow range 110, molotov NaN arm, beachball intensity int16.
MATCH: SetAreaCodeForVisibleObjects, GetEntryExitIndex, PostEntryExitsCreation, GetPositionRelativeToOutsideWorld; PreRender missile/flare/grenade/traffic-light constants (every pole x/y checked against the exe bits).
Open: CEntity::ProcessLightsForEntity 0x6FC7A0 (not compared, budget). Load/Save of the enex layout were not part of this slice's rows.
