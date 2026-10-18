# PT-COMBAT — playtest-1 U4..U8 (police idle, no weapon drops, no damage/hit reaction, wanted level, corpse orientation)

## Summary
Root cause class of U5/U6/U7 found, reproduced with a scripted probe, fixed (run build only effect; DLL build unchanged):

**Pool storage was filled with the MSVC debug "no man's land" byte 0xFD instead of zero.** The exe hands out zeroed pool storage (fresh heap pages) and its constructors leave
members uninitialised on purpose. `CTaskSimpleCarDrive` (0x63C340) only rewrites bits 2/3 of its flag byte at +0x5C and CLEARS bits 4/5 (`and dl, 0xCB`) - the port never
initialised `m_bClosingDoor` / `m_bPassengerAnimBlended`, so on the 0xFD fill `m_bClosingDoor` read true and `CTaskSimpleCarDrive::MakeAbortable` (any priority != IMMEDIATE) returned
false forever: **a ped in a car could never be aborted by any event.** Observed: police drivers with hp 0 kept driving (`evq=2` damage events stayed queued, `cur=-1`, no response task,
no death, no pickups), cop cars circled / sat at 30-40 m, cops never got out; the same for gang drive-by drivers (no damage reaction, no hit anim for the player while shot from cars).
Fix commits: 81885320 (ctor bits like the exe), 414cac8a (CPool initial fill 0 in the run build; fixes the whole class, e.g. the 4 task Clone()s of NOTSA-AUDIT).

## Probe (tools: .notes/reports/PT-COMBAT.md section "probe")
`NOTSA_STANDALONE_PROBE=1` (run build only) logs `[pc]` lines every second: player vitals / wanted / back-off flags / safe flags, peds within 60 m (type, hp, state, task chain,
event queue, current event, response tasks), vehicles (mission, autopilot, pedals, physics flags), path-node info, population state; injector events `cmd:wanted:N|tp:x:y:z|give:W:ammo|spawnped:M:T|shoot:TYPE:R|shootnear:R|cheats|cheat:N`.
Routes in C:/gta-scratch/ptc_probe/ (route_skip.txt = new game, ESC-skip intro, 2 stars; route_kill.txt = + lethal damage events on cops; route_foot.txt = foot peds).
Original exe reference: inputdrv + orig_probe.py (ReadProcessMemory poller: player, peds, law vehicles).

## Findings
| # | status |
|---|---|
| U5 police idle | ROOT CAUSE above. Before: cops in cars stuck/circling at 25-40 m, hp-0 drivers alive. After: events reach the drivers, cops get out (task 1106>1103 POLICE_PURSUIT>1101 ARREST seen), killed cops die. NOTE: the ORIGINAL exe on this PC also keeps cop cars at 25-50 m for 40 s in the alley scenario (orig_skip run), and busted CJ within 25 s in another run: the chase has randomness, so "time to busted" is not a hard criterion. |
| U6 dead cops drop no weapons | (a) in-car cops never died (above). (b) real port deviation fixed: `CPed::CreateDeadPedWeaponPickups` ammo = min(total, onStreet >> 1) (exe 0x45925B `shr eax,1`), the port used `onStreet * 2` (1cd7ebfc). Pickups for on-foot kills work (542 -> 547 in the foot probe). Cops killed inside a car correctly drop nothing (exe `bInVehicle` gate). |
| U7 no damage / hit anim | Damage to the player by cop pistols works (armour 100 -> 91 -> 83 in route_cop3, hit anim task 208 seen). The no-reaction case = shooters / victims in cars (above). |
| U8 wanted not cleared after the cutscene | Not reproduced: in the natural-intro run the script cleared wanted 4 -> 0 at the mission end (fade), flags reset. CWanted::Update's "no decay above 1 star in the city" matches the exe (0x562D66). Most likely consequence of U5 (cops never evaded / never arrested -> level stays). Unverified. |
| U4 corpse rotation | Not reproduced; `CTaskSimpleDead::ProcessPed` tilt (acos of ground normal . fwd/right, slope pitch +0x518 / roll +0x520) matches 0x630600. Photo shows a stretched leg (skinning/IK), possibly uninitialised-member class as well (fixed by the zero pool fill), unverified. |

## Other things seen
- Typed cheat `OSRBLHH` "does nothing": injector artefact (`key:h;key:h` has no frame between up and down, the doubled letter is lost: cheat string `HLBRSO`). TURNUPTHEHEAT / HESOYAM work; the table is intact.
- Natural intro (no ESC): ours rides CJ automatically (task 709) from (2408,-1253) and CJ ends DEAD at (2270,-1275) -> hospital respawn with a frozen camera (letterbox, no ambient peds); the original natural-run could not be captured (inputdrv aborted when the window lost the focus). Open.
- Soak (3 min): see PHASE2_STATUS; one run died in the known flaky BuildShadowVolume (FLAKES.md), not related.
- CRC-32 etc untouched; NOTSA-AUDIT's reversed pool loops crashed new game in RemoveDistantCars: fixed concurrently by 6fd95a8e (my duplicate fix reverted, test rows kept: ace26031).
