# COMBAT-FLAKY — gameplay_regress combat fails one-off in different checks since 2026-10-25

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh combat-flaky on origin/reverse/interior_c.
## Evidence (.notes/regress_flakes.tsv, gate logs C:/gta-scratch/gate_soak/*.regress.log, re-runs C:/gta-scratch/gr_out/recheck*-combat-*)
- 10-25 23:08 combat.hit.damage_event + car.driver_dies (CJ at 1 hp after the chase — ROUTE-FLAKES fixed the route with wanted 0 + heal)
- 10-26 19:30 combat.chase.no_zombie_cops "1 cops shot; afterwards dead=0 dead-but-not-dying=0"
- 10-26 19:50 re-run: combat.foot.weapon_pickup "new pickups after the kill: 0" (exe CreateDeadPedWeaponPickups)
Since 10-24 many behaviour fixes landed (ped events/AffectsPed, groups, walk speed, physics collision mass, population, CarCtrl).
## Task
1. Measure: run `gameplay_regress.sh combat` 8x on HEAD (Release of your worktree) and 8x on the exe of the 10-25 04:40 green gate
   (rebuild fe1fa820 in a second build dir if needed); table per check: fail count.
2. If HEAD fails more: bisect the landings between (git bisect run with a 3x combat loop as the test), find the commit, then decide: a port
   bug the fix exposed / a scenario assumption the fix (correctly, exe-like) broke -> harden the scenario the CHEATS-ARMOUR way (never loosen
   a check), or a real deviation -> fix as the exe.
3. If both fail alike: root-cause each failing check from its logs ([pc-dmg], the kill step, pickup creation) and harden or fix.
Acceptance: 8/8 combat PASS on the final tree. HARD budget: stop at 150k by your estimate. Never kill by image name; wait on gpu.lock.
