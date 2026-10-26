# ROUTE-FLAKES — soak `until:invehicle` timeouts and the combat-scenario flake

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh route-flakes on origin/reverse/interior_c.
## Evidence
1. soak.sh (3-min new-game route, tools/standalone/soak_route*.txt or the route soak.sh uses): "gate timeouts: 1..2, first: [until]
   invehicle TIMEOUT after 8000 ms" in gate soaks 2026-10-24 09:14, 22:10, 10-25 RD-AB-5/6 soaks (C:/gta-scratch/gate_soak/*/report.txt).
   RESULT stays OK, but the car-enter step needed a retry: find why (car spawned by the cheat not where the route expects, CJ blocked,
   the enter task aborted, since the 10-24/25 vehicle fixes cars spawn/settle differently) and make the route robust (gate on the real
   condition, never loosen a check) — or find the port bug if it is one.
2. gameplay_regress combat (2026-10-25 23:08 gate, C:/gta-scratch/gate_soak/1025-2244.regress.log + gr_out dir it names):
   combat.hit.damage_event "health+armour 1 -> 0/0" (CJ had 1 hp BEFORE the step) and combat.car.driver_dies "driver hp=-1 state=-1" (no
   driver). Re-runs passed twice. Hypothesis: gangs now have leaders (RD-AB-5/6) and attack CJ earlier; or the target car spawned empty.
   Root-cause from the logs ([pc-dmg] lines name the damagers) and harden the scenario the CHEATS-ARMOUR way.
## Acceptance
10 soaks with 0 gate timeouts; 6 combat runs PASS. HARD budget 200k tokens / 90 min. Do not touch tools/standalone/replay_diff/ or the
lockstep code (RD-AB-10 works there). Work ONLY in your worktree; never kill by image name; wait on gpu.lock.
