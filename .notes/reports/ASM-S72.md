# ASM-S72 report

Base 1524905b0 (origin/reverse/interior_c). Result table: `.notes/asm_sweep/S72.result.tsv` (35 rows: 7 FIXED in 3 fix commits, 28 MATCH).

| row | verdict | commit |
|---|---|---|
| CCheckpoints::PlaceMarker 0x722C40 | FIXED (IsUsed in lookup, strict sqrt<2) | fe1b307a1 |
| CCheckpoints::Update 0x7229C0 | FIXED (TORUSTHROUGH step 0.01 not 0.1, UPDOWN/DOWN tests) | b79a04c81 |
| CCheckpoint::Render 0x725C00 | FIXED (ENDTUBE zwrite/valpha, torus fade test, ring offsets) | b79a04c81 |
| C3dMarkers::PlaceMarker 0x725120 | FIXED (4 deviations) | 4ddfc9ba6 |
| C3dMarkers::PlaceMarkerCone 0x726D40 | FIXED (sqrt < 1.6, NaN) | 4ddfc9ba6 |
| C3dMarkers::DirectionArrowsDraw 0x7211F0 | FIXED (normalised orientation matrix) | 4ddfc9ba6 |
| C3dMarkers::Update 0x7227B0 | FIXED (single rounding) | 4ddfc9ba6 |
| everything else (Conversations, Credits, remaining 3dMarkers, Checkpoints Init/Shutdown/Render/DeleteCP/UpdatePos) | MATCH | - |

## Notes

- Biggest visible findings: TORUSTHROUGH checkpoints faded 10x too fast; reused TORUS / TUBE-without-zCheck markers never took a new Z (checkpoint TORUS_UPDOWN bob was invisible); ENDTUBE finish flag drew with Z-write on and left vertex alpha on.
- `CMatrix::WithUp` (Core/Matrix.h) does not normalise right/forward. The exe's arrow code does. `C3dMarker::Render` (0x7223D0, slice S71) also calls `WithUp`; I did not touch the shared helper (local `ExeArrowUpMatrix` in 3dMarkers.cpp). S71 should check whether its exe path normalises too.
- `DoneSettingUpConversation` with all 14 slots in use: exe dereferences NULL, the non-FIX_BUGS port writes after the array. Left as is (both UB, unreachable in the scripts).
- Credits: all 707 print/space events of RenderCredits compared by script (key, scale, highlight, space size): identical.
- Precision: the run build is PC24, so the `x87d` spellings added here are per-op float in effect; portable compile (PortableWin64, x64) of Checkpoint.cpp / Checkpoints.cpp / 3dMarkers.cpp: 0 errors.
- Tests: wave2_oracle_test PC24 0 / PC53 0 mismatches (1142 functions); combat regress PASS=14.
