# ASM-S31i - Cam.cpp per-mode processors, seventh slice (6 rows: 5 FIXED, 1 NOT_REACHED)

Result table: .notes/asm_sweep/S31i.result.tsv. Branch asm-s31i from origin/reverse/interior_c a3e65d14. One commit per function.

- Process_WheelCam (exe 0x512110-0x512B0F, full), 2bbb56a5: FIXED. The train facing test adds the dot product z,y,x (port x,y,z). Everything else MATCH: ped/vehicle/heli/plane/boat/bike geometry, TransformVector vs TransformPoint, NaN arms (`< 0` skip, water `level-0.3 > z`), inlined blur, the up tilt sums, LOS args.
- Process_Rocket head (0x511B50-0x511EC2), 963d929d: FIXED. Head position used GetTransformedBonePosition (0x5E01C0) instead of GetBonePosition (0x5E4280). Reset block, pad/mouse deltas (fov factors, NaN sign arms), clamps, front, heading MATCH. Tail was done in S31f.
- Process_DW_HeliChaseCam rest (0x51A740-0x51B0F0), 6e309bb9: FIXED. t via fild/fidiv; fov blend-in 1.0/blend stays unrounded; 3D distance for the fov zoom adds z,y,x; lerp ratio from exact int differences. Init loop, Randomize (0x50E090), collision blend, counters MATCH.
- Process_DW_CamManCam (0x51B120-0x51B83F), 5c6c67f1: FIXED. t via fild/fidiv; lamp distance adds x,z,y; |vel| adds x,z,y; shake select follows the exe NaN arms (0x420800 is `a > b ? a : b`, NaN takes 0.2).
- Process_DW_BirdyCam (0x51B850-0x51C246), 54a4dad5: FIXED. t via fild/fidiv; lamp point height is `(h-1)*(r*k)+1` (port `(h-1)*r*k`). Lamp pair search, found exits and LOS args MATCH.
- NOT_REACHED (budget): Process_Cam_TwoPlayer (0x525E50). The same fidiv `t` pattern may exist in other DW cam functions (checked here only in the three above).
- S30e (Camera.cpp, 7 rows) NOT_REACHED, see ASM-S30e.md.

Tests: StandaloneRelease built clean (/WX); v6 pre-commit grep clean; wave2_oracle_test, combat regress and PortableWin64 Cam.cpp.obj compile see ASM-S30e.md / final report lines below.
