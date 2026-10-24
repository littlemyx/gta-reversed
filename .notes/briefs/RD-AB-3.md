# RD-AB-3 — next orig-vs-port divergences (frame 3) + the two self-identity residues

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-3 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-NOISE-2.md (tables, rcmp.py, pins up to 1023, NOTSA_LOCKSTEP_DUMPSCR / RD_DUMPSCR). After the CRadar blip-counter fix the
script hash is equal through frame 2695 and the orig-port prefix is 3. At 0-based frame 3 (rand calls 133 orig vs 120 port):
(a) orig has one FxSystem_c::Play rand (return 0x4AA337) between object constructors that the port lacks -> an effect started only in the
    orig: find which object/effect (2dEffect / object.dat fx / CObject ctor path) and why the port does not start it;
(b) orig's first GenerateCarCreationCoors2 walks ~22 rand calls, the port returns after 1-3 (CarCtrl.cpp:2476/2480, start node invalid)
    -> path nodes near the player loaded/found only in the orig: compare ThePaths area loading at that frame (LoadSceneForPathNodes,
    MarkRegionsForCoors / ToBeStreamed — the 2026-10-21 consumer-audit area, IsAreaNodesAvailable, FindNodeClosestToCoors).
Residues: port-port flake (a run where 1-2 frames skip the render-pass rand calls: log the CTimer::Update caller / Idle path in that frame);
orig-orig regressed 1520/2177 -> 1186/2734 (check whether pin 512 on the orig side causes it).
## Task
Fix port deviations as the exe does (asm address, consumer audit, oracle row where feasible); push the orig-port equal prefix as far as it
goes within the budget, one divergence at a time; report each as frame / layer / cause / fix.
Gate (v5): Release + touched targets + soak. HARD budget 250k / 100 min — at 200k stop and land.
## Care
Work ONLY in your worktree — never checkout/switch branches in C:/Users/littl/github/gta-reversed (the orchestrator's checkout; RD-NOISE-2
detached it). Original exe needs the foreground; never fight USER-PLAY. Never kill by image name.
