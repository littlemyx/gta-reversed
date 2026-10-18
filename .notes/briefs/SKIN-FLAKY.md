# SKIN-FLAKY — intermittent null deref in rw::d3d9::instanceMesh after cheat model swap

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh skin-flaky.
## Evidence (.notes/reports/PROBE-SUITE.md)
1 crash in ~8 runs of `gameplay_regress.sh cheats`: null deref in rw::d3d9::instanceMesh while rendering a skinned ped right after the
SECOND Beach Party cheat press (stack SkinPipeRender <- RpAtomicRender <- CRenderer::RenderOneNonRoad). Logs: C:/gta-scratch/gr_out/f5/cheats.
Beach Party swaps player/ped models (streaming removes/reloads clumps) — suspect: a ped whose model was released still in a render list,
or geometry instanced lazily after its mesh header was freed; or a port deviation in the cheat's model swap / CStreaming remove order.
## Task (one crash)
1. Make it reproducible: loop the cheats scenario (or a minimal route: Beach Party x2 + render N frames) with NOVSYNC, under gpu_locked.sh,
   until it crashes; capture a minidump / the injector state. Try a Debug or RelWithDebInfo run build for a better stack if needed.
2. Find the root cause by comparing the involved port code with the asm (cheat handler, CStreaming::RemoveModel / ped deletion order,
   render list cleanup). Fix as the exe does; do not add null guards that the exe doesn't have.
## Acceptance
Root cause stated with asm addresses; ≥30 consecutive repro-loop runs without the crash after the fix (and the crash rate before it);
Release builds; run_all_tests 45/45; gameplay_regress cheats PASS. Report ≤15 lines.
