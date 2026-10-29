# ASM-S32c (Radar.cpp drawing/streaming, 23 rows) - 2026-10-29
Result: 13 functions compared by disassembly: 8 DEVIATION fixed (one commit each), 5 MATCH; 10 NOT_REACHED. Table in S32c.result.tsv. All changes in source/game_sa/Radar.cpp.
Fixes (commit, function, what):
- 41d2a008 DrawEntityBlip: airstrip range-update test takes the update branch on NaN distance (only difference found; everything else, incl. zoom/NaN compare forms, matches).
- 5e27328d DrawAreaOnRadar: early-out is the exe's 4 separation tests vs m_radarRect (IsRectInside never fired: wrong polarity and top/bottom pairing).
- 5ac16a65 DrawRadarGangOverlay: mode-2 pulse angle (t&0x3FF)*2pi/1024; the port had 1024/2pi (alpha of pulsing gang zones was effectively random).
- fd6433db Draw3dMarkers: contact-point enex/area gate, Nevada factor 0.6 (port 5/3), vehicle z association, missing pickup drawn at origin.
- 63edd765 DrawRadarMask: the full-radar-square pass at the far screen z was missing; corner order.
- 33a5199d DrawRadarSection: texture lookup first + early return (port drew stale vertices, treated slot 0 as none), corners *(1/range), tex coords *0.002f (was /500), no state change when 0 verts.
- fb336029 DrawYouAreHereSprite: 500 ms on / 200 ms off (port 700 both).
- a43b5688 RemoveRadarSections: x-major, skip -1 (port also hit RemoveModel(19999)).
- d50c0d23 StreamRadarSections(x,y): x-major order.
MATCH: RequestMapSection, RemoveMapSection, DrawRadarSprite, StreamRadarSections(vec), DrawRotatingRadarSprite, DrawRadarSectionMap.
IMPORTANT note on the S32b "CRect literal order" pattern: the fork's CRect declares its floats as left(+0), top(+4), right(+8), bottom(+0xC) but the ctor is CRect(left, bottom, right, top) and assigns by NAME, so a ctor literal {a,b,c,d} has physical layout (a,d,c,b) while the exe ctor gives (a,b,c,d). The exe's CSprite2d::SetVertices(CRect) reads slot +0xC for vertices 0/1 and +4 for vertices 2/3, the port's reads .bottom (+0xC) for 0/1, so the port literal must be the exe literal with arg2/arg4 swapped to be physically identical (DrawRadarSprite's existing {x-w, y-h, x+w, y+h} == exe {x-w, y+h, x+w, y-h}). S32b's rewrites to {l, y+dy, r, y-dy} (ShowRadarTraceWithHeight, DrawLegend box) are therefore physically mirrored vs the exe; harmless for uniform-colour fills (same pixels), wrong for textured/gradient rects. Worth a check by whoever owns that class.
Open (NOT_REACHED, in order): DrawLegend LOW/HIGH polygons + sprite path (0x5828A0), DrawRadarMap 0x586880 (plane-rect literal slot order, see note) and DrawMap 0x586B00, Initialise 0x587FB0, LoadTextures 0x5827D0, Shutdown 0x585940, Load 0x5D53C0 / Save 0x5D5860 (blip save layout).
Gate (worktree asm-s32c): StandaloneRelease + StandaloneRW built; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=gta_sa_compact.exe) PC24 0 / PC53 0 (1142 functions); gameplay_regress combat PASS=14 (run directly, -b worktree). v6 pre-commit grep clean (final); PortableWin64 not compiled (no signature/x87d changes; RwIm2DGetFarScreenZ is the shared fakerw accessor already used by Sprite.cpp).
