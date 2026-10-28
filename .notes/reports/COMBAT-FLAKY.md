# COMBAT-FLAKY report (2026-10-28, rules v6; no game code changed, so no soak)
Measured: HEAD ba7ead2e Release, `gameplay_regress.sh combat` 8x -> 8/8 PASS, but no_zombie_cops SKIPPED in 7/8 (CJ wasted in the chase -> hospital respawn -> no cops at the kill step). Runs on the 10-25 build not needed (root cause is in the scenario).
| check | fails before (gates 10-26/10-28 + 8x HEAD) | after (8x, final tree) |
| chase.no_zombie_cops | 3 FAIL (1 cop shot, dead=0) + 7/8 SKIP | 0 FAIL, 0 SKIP (8/8 real) |
| foot.weapon_pickup | 3 FAIL (10-26 rerun, 10-28 0830 gate, 2 more in my A/B runs) | 0/8 |
| car.driver_dies | 0 (appeared 5/8 after the 24 s wait below: corpse already removed at car_shot_b) | 0/8 |
Root cause 1 (no_zombie_cops): test artefact. CJ stands still under pistol fire from 3 cops and dies in 10-33 s (7/18 logged runs); the respawn removes the cops; `shot` collected ALL "shot ped" lines, incl. the later shoot:6:200 clean-up, so one cop shot afterwards = "shot, none dead". Cop death path was fine ([gr] copsDead/state=DEAD in every log).
Root cause 2 (weapon_pickup, "cop not dying" was wrong: cop hp 0, state 54/55): exe behaviour. CreateDeadPedWeaponPickups -> CPickups::TryToMerge_WeaponType adds the ammo to a same-type PICKUP_ONCE_TIMEOUT pickup within 5 m, so a cop killed next to the clean-up corpses drops "no new pickup". Those drops live 20 s (Pickups.cpp GenerateNewOne).
Fix (tools only): routes/combat.txt heals CJ every 2.5 s in the chase and waits 24 s before step A (drops expired); gr_check.py: `shot` = only the kill-step shots (Run.cmds_after); car.driver_dies looks at car_shot_a AND car_shot_b (corpse may be gone, like the kill-step precedent). Checks otherwise unchanged. Tried and dropped: a `clearpickups` GR cmd (did not help, made it worse).
Final 8x on the final tree: 8/8 PASS, 14 PASS each, 0 SKIP (exe built before the C++ experiment was reverted; source tree == origin, only an unused cmd differs).
