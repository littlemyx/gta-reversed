# ASM-S30d (Camera.cpp, 14 rows)

Result table: .notes/asm_sweep/S30d.result.tsv. 4 functions fixed (5 commits), 3 MATCH, 1 PARTIAL (Process), 6 NOT_REACHED.

- UpdateTargetEntity (1a6e0538): exe flag bl starts false (true only when who==2, then arrested-test for modes 7/8); port started true, so with who!=2 the early-out was skipped. ENTER_CAR/CARJACK/OPEN_DOOR use raw player->m_pVehicle (FindPlayerEntity/FindPlayerVehicle need bInVehicle, not yet set while entering).
- CalculateGroundHeight (ca274767): NaN-only (clamp via `if (z<0)` instead of std::max); rest MATCH.
- ImproveNearClip (b0ee40b7, 1b0fc4f8): plane/heli test read m_nVehicleType (+0x590), exe reads m_nVehicleSubType (+0x594); min(a,b) NaN polarity (exe 0x404330).
- CameraColDetAndReact (18265771): the 3 inlined lengths add z,y,x (CVector::Magnitude adds x,y,z: 1-ulp differences in radius, minDist and the moved-enough test); two NaN polarities.
- ProcessJiggle, DeleteCutSceneCamDataMemory, LoadPathSplines: MATCH (all 5 hand-shaker default sets decoded from the stores at 0x516586-0x516970).
- Process: 0x52BA3B-0x52C340 (jiggle call + transition interpolation) MATCH; open 0x52C340-end.
- Not reached: SetRwCamera, Constructor, CopyCameraMatrixToRWCam, DealWithMirrorBeforeConstructRenderList, TryToStartNewCamMode tails, CamControl open ranges.
- Note: PC24 rounds every x87 op to float, so a plain float expression with the right term order is faithful; x87d casts only matter where the exe keeps a 53-bit operand.
- Tests: StandaloneRelease + StandaloneRW build OK; wave2_oracle_test 1142 functions, 0 mismatches (PC24 and PC53); gameplay_regress combat PASS=14; v6 pre-commit grep clean.
