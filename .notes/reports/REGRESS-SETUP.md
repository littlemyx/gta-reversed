# REGRESS-SETUP (Windows, rules v4, 17:22-19:0x) - test side only (tools/standalone/gameplay_regress)

Exe under test: the 2026-10-23 16:25 StandaloneRelease build (copied to C:/gta-scratch/regress-setup-exe/, nothing run in the main checkout).

## Changes
1. `routes/header_free.inc` (the ONLY place that mounts/dismounts the tutorial bike; included by combat, audio, cheats, interior): the dismount is gated like REGRESS-FLAKES did in missions.txt
   (`key:return; until:onfoot:4000; skipif:onfoot:..; key:return; until:onfoot:4000`), plus a guard so a retry press can never re-mount.
   Found on the way (suite 4, audio attempt 1): the *mount* can fail too (`[until] invehicle TIMEOUT after 10000 ms`, then the dismount press mounted the bike -> SETUP invehicle=1). The mount now has
   one retry press (`until:invehicle:6000; skipif:invehicle; key:return`). Normal mount takes 2.6 s, normal dismount 0.7 s (12 of 12 dismounts in suites 1-3 took 0.69-0.72 s, the retry was never needed).
   firststart/vmode/mouse/arrest0 do not touch the bike; missions had its own gating already.
2. `gr_check.py --summary`: a `<scn>.attempt1.result` whose `<scn>.result` exists (gameplay_regress.sh re-ran it) is printed as an `info` line (with its SETUP rows) and not counted; the verdict is the retry's.
   (A retry that itself fails still counts as FAIL/SETUP.) Checked on a fabricated out dir (SETUP attempt1 + clean retry => PASS; retry with a FAIL => FAIL).

## Evidence (GR_SKIP=render_firststart, full suites)
| suite | header_free version | verdict |
|---|---|---|
| 1 | dismount gate | PASS=70, no SETUP |
| 2 | dismount gate | FAIL=4: cheats.no_crash + ran_to_end, combat.foot.weapon_pickup, combat.chase.no_zombie_cops (NOT setup; see below) |
| 3 | dismount gate | PASS=70, no SETUP |
| 4 | dismount gate | audio attempt 1 SETUP (mount timeout, above) -> retry passed; killed at cap before the end (partial) |
| 6, 7 | + mount retry | the four header_free scenarios only (combat audio cheats interior_lspd), run twice: PASS=36, no SETUP, both times |

## New finding (not in scope, for the orchestrator)
Suite 2 cheats: access violation at 0xb75adf `CAnimManager::UncompressAnimation:399 <- CAnimBlendAssocGroup::CopyAnimation:45 <- CAnimManager::BlendAnimation:529 <- CTaskSimpleFall::ProcessPed:74`
(log C:/gta-scratch/gr_out/regress-setup-2/cheats/log.log:453; 1 in 3 suites). Same suite: combat zombie cops / no weapon pickup (also 1 in 3). Looks like real flaky port behaviour, not route timing.

## CONTINUE
3 consecutive FULL suites with the final header_free (mount + dismount retry) were not completed in the 100-min cap (end 19:01): only the 4 header_free scenarios ran twice with it (PASS=36 each); full suites 1 and 3 (dismount-only version) PASS=70, suite 2 FAIL=4 for the unrelated items above, suite 4 partial.
Next: `GR_SKIP=render_firststart tools/standalone/gameplay_regress.sh` x3. Investigate the CTaskSimpleFall/UncompressAnimation crash separately.
