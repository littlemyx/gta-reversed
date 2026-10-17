# User playtest 1 — 2026-10-17 ~22:37–23:01, C:\gta-play snapshot = StandaloneRelease @ 66d98061 (exe-free), fullscreen, sound on

Evidence: C:/gta-scratch/playtest1/ (crash-20261017-230109.log, game log.log ~1 MB, the gta_sa.set the game wrote, 3 photos of the screen 2/3/4.webp).

| # | observation (user's words, translated) | group |
|---|---|---|
| U1 | First start (no gta_sa.set): odd resolution, not fullscreen (photo 2: game in a part of the 4K screen, desktop visible). Changing it in the menu works, even 4K. | render/input |
| U2 | Square artefacts: semi-transparent dark/blue squares scattered over road, sky, walls (photos 3, 4). | render |
| U3 | Mouse not captured: rotating the camera in one direction stops at some point (cursor hits the screen edge?). | input |
| U4 | A dead pedestrian's corpse is unnaturally rotated (photo 4: body twisted, legs along the wall). | anim/physics |
| U5 | Police do nothing while the player has wanted stars. | combat/AI |
| U6 | Dead cops drop no weapons. | combat |
| U7 | Shots at the player do no damage and play no hit animation. | combat |
| U8 | Wanted level is not cleared after the first mission's cutscene. | mission/script |
| U9 | After a cutscene the bike pedalling animation broke. | anim |
| U10 | Busted while riding bikes with Ryder (mission): after leaving jail the mission state was not reset. | mission/script |
| U11 | Hard to get back on the bike (re-entering). | anim/vehicle |
| U12 | With sirens + street noise + voices + gunshots at once the mixer seems to break: audio dropouts. | audio |
| U13 | At some point all sounds except the radio died. | audio |
| U14 | Crash after a cutscene: `UNREACHABLE hit: ReadBeforeComboData (TaskSimpleFight.cpp:381)` (D-B18's GetComboFlagsLo pre-array mapping, never exercised at run time). | crash |
Also visible in photo 4: peds with double-image ghosting (motion blur?) — check vs original.
Many items may be REGRESSIONS of 2026-10-16/17 changes (stream D B/C phases, FIX-AUDIT bugfix switches off, CAM-INIT, D-C56 zero-init of audio objects) — bisect against the original exe and older snapshots (branch standalone-snapshot-2026-10-16, C:/gta-scratch/bv_play_old_gta_reversed.exe, C:/gta-scratch/play_prev_gta_reversed.exe).
