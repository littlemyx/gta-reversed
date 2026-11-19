# ASM-HC3e: executed-but-unverified functions, gameplay files in source/game_sa (continuation of HC3d)

Zone rows (ryder1.tsv, NONE, no MATCH/FIXED verdict, non-render): ~112 after HC3d; render/audio-output skipped (CStreaming::RenderEntity, CCamera::RenderMotionBlur, CAudioZones/CAE*AudioEntity); TimeCycle ambient getters done in HC3d.
Compared 11: 5 MATCH-or-already-right (FindAngleToWeave prelude+loop, CalculateGroundHeight, FindGarageForObject, PickPedMIToStream, ComputeMoveDirToAvoidEntity after revert), 6 FIXED.
**9da7b7915 (HC3d) was WRONG**: ComputeMoveDirToAvoidEntity - the fld at 0x5F37B2 runs before `pop esi` (0x5F37B6) so [esp+0x40] is planes[3].y; the original port was right. Reverted (a0d19e4d3). Whole function re-read: all other plane offsets and the 3-way compare are right.
FIXED: SteerAICarWithPhysicsFollowPath/FindMaxSteerAngle (unrounded x87 max steer in the clamp), CCamera::CameraColDetAndReact (x87 chain), CStreaming::Update (call order with a remote vehicle), ComputeClearTarget (x87), CTheZones::FindZoneByLabel (8 byte compare).
NOT_REACHED (budget): ComputeRouteRoundEntityBoundingBox (1900 insns), PossiblyStreamCarOutAfterCreation, AreTexturesUsedByRequestedModels, CPathFind switch-off rows. Note: CarAI.cpp:1336 also clamps with the float FindMaxSteerAngle (exe 0x42025C may use the unrounded value) - not checked.
Builds: StandaloneRelease links; tests not run (no gate). Details: .notes/asm_sweep/HC3e.result.tsv.
