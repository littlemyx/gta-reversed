# ASM-S70b (Glass, FallingGlassPane, BreakManager_c, BreakObject_c, Birds, CEscalator::AddThisOne)

Compared 15 of the 31 rows against the exe; 16 rows NOT_REACHED (budget): BreakObject_c x7, BreakManager_c::Add, Birds x3, CEscalator::AddThisOne, CGlass::Render, RenderReflectionPolys, RenderShatteredPolys, RenderHiLightPolys.

Verdicts: 8 FIXED, 7 MATCH, 16 NOT_REACHED (table: S70b.result.tsv).

Fixes (rebased hashes)
- bff427479 CGlass::HasGlassBeenShatteredAtCoors 0x71CB70: returns bit 5 (bGlassBrokenAltogether), not bit 4.
- 39f4fea95 CGlass::FindWindowSectorList 0x71AFC0: distance sqrt((dz^2+dy^2)+dx^2) unrounded, compared unrounded against the float best.
- f2c2c615a CGlass::WindowRespondsToCollision 0x71BC40: furthest-vertex search starts at vertex 1 (sqrt distance, strictly greater); NaN damage takes the DELAYED arm.
- 4eaa01d5b CGlass::GeneratePanesForWindow 0x71B620: section count is ftol(max(1.0, size + 0.75)) (thin windows got 0 panes); pane matrix/position/offset arithmetic of the exe; the FIRST rand() goes to m_RandomNumbers.y and the second to .x; drag velocity unrounded; DELAYED time ftol(len*100 + time).
- d0de146a2 CGlass::CarWindscreenShatters 0x71C2B0: window = first TWO glass triangles of the model; right axis = normal x up (the port had up x normal: mirrored panes); dots summed x,z,y; centre arithmetic; the AE_GLASS_BREAK_FAST sound was missing.
- 32c06ec92 CGlass::BreakGlassPhysically 0x71CF50: sphere centre is the offset taken INTO object space (dot products), not CMatrix::TransformVector; no early return after cracking a pane; shatter sound 0x7E; sets bGlassBrokenAltogether.
- eb44cccbd CFallingGlassPane::Update 0x71AA10: ground sound/glass particles at (x, y, groundZ); the m_f6F step is (ts*vel)*0.35.
- fb11e8046 CFallingGlassPane::Render 0x71B100: signed 0..500 delta (delayed panes were drawn at full alpha), buffer flush checks, shattered polys use base+idx indices and advance both cursors (the port never advanced them, so cracked-glass polys were never drawn), 0.1 slope, camera distance in z,y,x order.

MATCH: BreakManager_c::Init/Exit/ResetAll/Update/Render, CGlass::WasGlassHitByBullet, CGlass::Update.

Tests: wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53 (the target does not contain Glass/FallingGlassPane, so it only confirms nothing else moved); combat regress PASS=14 (run before two spelling-only edits for the pre-commit grep, StandaloneRelease rebuilt after them); v6 pre-commit grep clean; Glass.cpp and FallingGlassPane.cpp compile for PortableWin64 (x64 cl, flags from the main PortableWin64 compile_commands with the include root swapped to the worktree, PCH dropped).

Open: BreakObject_c (all 7), BreakManager_c::Add, Birds (3), CEscalator::AddThisOne, CGlass::Render and the three Render*Polys helpers (not compared).
