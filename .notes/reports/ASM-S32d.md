# ASM-S32d (Radar.cpp: Task 0 CRect check + DrawLegend/DrawMap/DrawRadarMap/Load/Save/Initialise/LoadTextures/Shutdown) - 2026-10-29
Result: Task 0 found 8 mirrored rect literals in 4 functions (all fixed); Part 2: 8 functions compared, 2 DEVIATION fixed (DrawLegend, DrawMap), 6 MATCH. Table in S32d.result.tsv. All code changes in source/game_sa/Radar.cpp.

## Task 0: CRect corner order
Key finding: the S32c claim "the exe CRect ctor stores its 4 args at +0,+4,+8,+0xC in order" is WRONG. The exe CRect(float,float,float,float) ctor at 0x4041C0 stores arg1 -> +0, arg2 -> +0xC, arg3 -> +8, arg4 -> +4
(`mov edx,[esp+8]; ... mov [eax+0xC],edx; ... mov [eax+4],edx`), i.e. exactly what the fork's ctor does (CRect(left, bottom, right, top) assigning by name into left+0, top+4, right+8, bottom+0xC).
So the ctor is identical to the exe's, Rect.h is NOT changed, and a ctor literal {a,b,c,d} has memory (a,d,c,b) in both. The exe's SetVertices(CRect) (0x727A00) reads +0xC for vertices 0/1 and +4 for 2/3; the port reads .bottom (+0xC) / .top (+4): identical.
The Radar code in the exe does not call the ctor, it stores floats directly at +0/+4/+8/+0xC; the S32/S32b rewrites read those stores as ctor argument order and mirrored the literals. Correct rule: port literal {p0, p3, p2, p1} for exe memory (p0, p1, p2, p3).
Harmless for uniform fills (same pixels), but vertex order / winding differed from the exe.

| site | port physical (before) | exe physical | verdict / commit |
|---|---|---|---|
| ShowRadarTrace border (0x583F40) | (x-s-1, y-s-1, x+s+1, y+s+1) | (x-s-1, y+s+1, x+s+1, y-s-1) | MIRRORED, fixed aba5aa20 |
| ShowRadarTrace inner | (x-s, y-s, x+s, y+s) | (x-s, y+s, x+s, y-s) | MIRRORED, fixed aba5aa20 |
| ShowRadarTraceWithHeight NORMAL border + box (0x584070) | (x-dx, y-dy, x+dx, y+dy) | (x-dx, y+dy, x+dx, y-dy) | MIRRORED x2, fixed f3c90045 |
| DrawCoordBlip (0x586D60) | no rect (calls ShowRadarTraceWithHeight) | no DrawRect / ctor in the exe | n/a |
| DrawBlips map ruler vertical (0x588050) | (X-1, 0, X+1, H) | (X-1, H, X+1, 0) | MIRRORED, fixed dda9bf74 |
| DrawBlips map ruler horizontal | (0, Y-1, W, Y+1) | (0, Y+1, W, Y-1) | MIRRORED, fixed dda9bf74 |
| DrawLegend NORMAL outline + inner (0x582F0E) | (x-dx, y-dy, x+dx, y+dy) | (x-dx, y+dy, x+dx, y-dy) | MIRRORED x2, fixed ce4a2e49 |
| DrawLegend sprite rect (0x582E24) | (x, y+16sx, x+16sx, y) | (x, y+16sx, x+16sx, y) | MATCH |
| DrawAreaOnRadar early-out vs m_radarRect (0x5853D0 / SetupRadarRect 0x584A80) | left+0 top+4 right+8 bottom+0xC, 4 tests | same slots, same tests (S32c 5e27328d) | MATCH |
| DrawRadarGangOverlay zone rect (0x586650, GetRect = ctor(x1,y1,x2,y2)) | (x1, y2, x2, y1) | (x1, y2, x2, y1); reset value 1e6/-1e6 slots equal | MATCH |
| DrawRadarMap plane rect (0x586880) | (x-1000, y+2000, x+1000, y-2deg) | (x-1000, y+2000, x+1000, y-2deg) | MATCH |
| DrawRadarMask / DrawRadarSection / DrawYouAreHere / Draw3dMarkers / DrawEntityBlip (S32c) | vertex arrays / sprites, no CRect literal changed | - | n/a |

Note: DrawRadarSprite's existing literal ({x-w, y-h, x+w, y+h}) was already physically right (never touched), and so were the original pre-S32 ShowRadarTrace / DrawLegend / ruler literals apart from their other bugs.

## Part 2
- DrawLegend (d910833b, ce4a2e49): LOW/HIGH triangles: the base is at 6 * Y stretch from the centre (port 7), apex at 7/5 * Y stretch (port used the X stretch for the LOW apex), vertex order as the exe (HIGH apex first). Text and sprite path match (sprite rect checked physically).
- DrawMap (058d5303): plane radar range slope is 16.666668 (0x858B8C; the port's 1/60 gave ~340 instead of 350 at the top), co-op with focus on player 1 uses that player's centre (port averaged unless focus was player 2), NaN speed takes the max range, remote-mode test uses Players[PlayerInFocus], area/radar-mode gate evaluated after the origin (port dereferenced FindPlayerPed first).
- DrawRadarMap: MATCH (10 render states, tile indices, 9 sections in order, plane block incl. rect). Load/Save: MATCH (non-portable path; 175 x 0x28, enex pool index+1, tracking bit save/restore). Initialise / LoadTextures / Shutdown: MATCH (name table 0x8D0720 compared entry by entry; exe entries 0/1 hold a pointer to an empty string, the port nullptr, texture stays null).
- Not changed: the exe's Load reads the pool flag byte unchecked for a bad index, the port bounds-checks (kept). SCREEN_STRETCH_* float helper vs exe: no difference (the exe runs with 24-bit precision control).

Gate (worktree asm-s32d): StandaloneRelease + StandaloneRW built; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=gta_sa_compact.exe) PC24 0 / PC53 0 (1142 functions); gameplay_regress combat PASS=14 (run directly, -b worktree); v6 pre-commit grep clean on every commit. PortableWin64 not compiled (no signature/x87d/header changes).
