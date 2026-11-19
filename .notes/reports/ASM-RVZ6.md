# ASM-RVZ6: independent review of RVZ4 leftovers
Oracle gta_sa_compact.exe 1.0. See RVZ6.result.tsv. No source changes (no build needed).
- 0ea5811fc CameraColDetAndReact radius z^2: CONFIRMED (fst keeps unrounded st0, fmul by float copy; x/y float).
- SteerAICarWithPhysicsFollowPath 0x434900-0x435452: MATCH, no deviations (stack slots re-derived across the `push edi` shift; fwd2D/heading slots not clobbered).
- FindAngleToWeaveThroughTraffic: skipped, covered by HC3e.
