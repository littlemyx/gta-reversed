# ASM-S105 (phase 3 tier B: CRenderer render list / LOD selection, CStreamingInfo, CTxdStore) - 2026-10-30
Result: 39 rows, 38 compared in full (9 functions FIXED in 8 commits, 29 MATCH), 1 NOT_REACHED (CWorldScan::ScanWorld 0x72CAE0: only the duplicate removal and the lowest-y pick were re-compared; the gift wrapping and scan conversion were reviewed in f22f575ff). Tables in S105.result.tsv.

Everything that decides what is drawn / streamed was compared against the exe: the visibility returns (invisible / visible / culled / streamme), the LOD distance rules (300 / 150 / 50 / 20, fade 1/15 + 10, lowLodDistScale on big buildings), the jump table 0x554B00 of ScanSectorList, the streaming request logic (loaded / occluded / loading priority / numModelsRequested), the sector centres and frustum-to-sector conversions (0.02 + 60, 0.005 + 15), the tunnel flags (0x880 / 0x80 / 0x800), the in-the-sky test and LOD scale, the first-person vehicle block, the render states. All of it MATCHed; the deviations were in distance sums, NaN arms and float expressions:

Fixes (one commit each, all in source/game_sa):
- 17bb9a67f RenderEverythingBarRoads (0x553AA0): the alpha-list distance is an inline `fsub x3; fsqrt` summing the squares z,y,x (sorted list) / z,x,y (underwater list); the port used CVector::Magnitude (x,y,z).
- 8527a9848 SetupMapEntityVisibility (0x553F60): the two min()s (`fcomp; jp` / `fcom; jnp`) and the fade / streaming range tests take the exe's NaN arm (std::min and `>=` took the other one).
- ad61ab6b1 SetupEntityVisibility (0x554230): the camera distance (LOD fade input + render list distance) is the inline z,y,x sum, not Magnitude.
- 39288b602 SetupBigBuildingVisibility (0x554650): same z,y,x distance.
- 90cf4a506 ScanSectorList / ScanBigBuildingList: the entity distance is ONE stack slot per call; the RENDERER_VISIBLE paths that do not write it (vehicles, peds, clumps, don't-stream objects; big-building vehicles) hand the previous entity's value to AddEntityToRenderList (the port re-zeroed it per entity).
- 9eab55ee6 ConstructRenderList (0x5556E0): the LOD scale ratio clamp keeps a NaN (`fcom 1.0; jne`).
- 9950ff787 ScanWorld (0x554FE0): the far frustum points are (p * 300) * float(1 / farPlane) (fld1; fdiv; fmul) - the port divided (1 ulp for ~1/4 of the inputs, moves the scanned sector hull at its edges); the near copies are * 0.2f (not / 5, they are not used by the scan); the camera-still flag (unused global 0xB745C8) is `|d|^2 (z,x,y) < 16 && dot(forward, last forward) (z,x,y) > 0.98`, the port tested Dot(distance, forward).
- b2944c767 RequestObjectsInFrustum (0x555960): same ExeDivide far points; a NaN at.z takes the at.z > 0 arm (jp).
- df8d51a0d CStreamingInfo::Init (0x407460): the flags byte (+6) is not written (the port zeroed the record).

Notes (MATCH, no change): CStreamingInfo::InList is `next != -1` in the exe function, the port tests next >= 0 || prev >= 0 (the inline site 0x407F93): equal on consistent lists. CTxdStore functions guard null slots the exe would fault on (StartLoadTxd, Create, RemoveRef, SetupTxdParent): same behaviour on every valid slot. RequestObjectsInDirection ignores its flags argument in the exe (always 0x20); all three callers pass STREAMING_LOADING_SCENE.

Gate: StandaloneRelease + StandaloneRW built after the last edit; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=compact exe) 1142 functions, mismatches PC24 0 / PC53 0 (no CRenderer rows in it); PortableWin64 compile (x64 cl, Renderer.cpp + StreamingInfo.cpp, the main checkout's compile command with the worktree sources and a worktree PCH header) 0 errors; v6 pre-commit grep clean; combat 14 + missions 11 + render_vmode 10 = PASS=35 (run once at the end).
