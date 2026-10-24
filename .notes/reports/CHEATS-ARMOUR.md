# CHEATS-ARMOUR (Windows, rules v4, 2026-10-24 06:19-~07:55)

Both findings are SCENARIO ARTEFACTS (no port bug); fixed in the route / checker, not by loosening the cheat comparison.

## 1. cheats.state_matches_original armour 97/90/84 != 100 (c44/c70)
Reproduced with a stress route (HESOYAM-style `cmd:armour:100`, then the Ninja pair x12) with the game pinned to one core + 3 busy loops (stand-in for cdb): first two cycles armour 98 / 89.8 / 91.8.
`[pc-dmg]` (NOTSA_STANDALONE_PROBE=1) names the damager: Ballas gang peds (ped type 7, models 102/104, pistol weap 22 and fists weap 0, d=13 m) of the Idlewood start area (CJ stands in Ballas turf)
attack CJ - the game's own population/AI, not a cheat effect (armour drops come in 3.3 steps = single pistol/fist hits, hp untouched). In 36 unloaded/vsync cycles and 6+ plain cheats runs it never happened (the peds
arrive late when frames are slow); the original's one recorded baseline run (armour 100) was luck, not a guarantee. The existing guard only knew the Village People fight (c40+).
Fix (test side): gr_check.py reads the `[pc-dmg]` lines (cheats scenario now runs with NOTSA_STANDALONE_PROBE=1): once an ENTITY (lastDamager != 0) lowered CJ's armour in a probe window, armour is not compared from that probe on
(printed in the row); a death by an entity also drops weapons/money comparison (hospital fee; seen 2026-10-24: Village People rocket killed CJ -> c41 money 250250 / weapons empty, 1 of 6 runs). Cheat-caused armour changes have no damager and stay compared
(mutation: c5 armour 50 without damager -> FAIL; same with a pc-dmg line by an entity -> PASS with note).

## 2. cheats free_start SETUP mission=1 (5 of 11 cdb runs)
Measured: under load/slow frames (vsync mode, cdb) the first Return does not mount within 6 s, the retry mounts (`invehicle ok after 2.7 s`), and the opening mission flag clears only ~4.2 s after the retry press
(`[until] nomission ok after 4235 ms` in slow_1), but header_free.inc dismounted 1.5 s after bInVehicle -> the script had not yet seen CJ in the bike -> mission stayed 1. On a fast run the flag drops within the same second (mission=0 at the first inVeh=1 line).
Fix: new injector condition `until:nomission[:ms]` (SDLWrapper.cpp InjCond, run build only; docs/STANDALONE.md) and `until:nomission:20000` before the settle wait in routes/header_free.inc; a real failure still times out and stays SETUP.

## Runs (private data dir C:/gta-scratch/gr_ca, silent, windowed)
See the hand-back for the final 10-run acceptance numbers.
