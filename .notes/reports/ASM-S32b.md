# ASM-S32b (Radar.cpp, 27 functions in scope) - 2026-10-29
Result: 5 compared by disassembly (ShowRadarTraceWithHeight, DrawCoordBlip, SetupAirstripBlips, DrawBlips, DrawLegend partial), all DEVIATION and fixed; plus 2 related functions fixed (GetStaticColour, SetCoordBlipAppearance). 22 NOT_REACHED (DrawEntityBlip and the drawing/sprite functions). Table in S32b.result.tsv.
Fixes (one commit, Radar.cpp only):
- DrawCoordBlip: non-sprite alpha was inverted (exe: colour alpha unless flag 0x20; port faded by distance), z-height tests spelled as exe (z-2 > pz, !(z+4 < pz), NaN branches), DbgFlag ShowRadarMarker block added, blip size movsx int16.
- tRadarTrace::GetStaticColour / SetCoordBlipAppearance: the exe uses the 2 bits at flag byte +0x25 (m_nCoordBlipAppearance) and requires BLIP_COORD; the port used m_nAppearance (+0x26) and required BLIP_CAR, so SET_COORD_BLIP_APPEARANCE never changed a coord blip colour (run-visible).
- DrawBlips: sprite pass first then non-sprite (was reversed), waypoint overlay drawn once, map-ruler rect corner order, _ftol-truncated mouse bounds, player direction * (1/range), hidden player colour (50,50,80), FindPlayerHeading(i), heading-(orientation+pi).
- ShowRadarTraceWithHeight: box rects use {l, y+dy, r, y-dy} (bottom/top were swapped).
- SetupAirstripBlips: nearest strip by the exe's strict-< chain (ties/NaN).
- DrawLegend: positions use _ftol (port rounded); height-2 box uses Y stretch and exe corner order.
Class patterns confirmed: CRect literals in the exe are {left, bottom, right, top} with bottom = larger screen y; check every DrawRect in the remaining Radar functions (DrawRadarMask, DrawRadarGangOverlay, DrawAreaOnRadar, DrawRadarSection*).
Open (NOT_REACHED, in order): DrawEntityBlip 0x587000 (port has x87 annotations, not re-compared), DrawAreaOnRadar 0x5853D0, Draw3dMarkers 0x585BF0, DrawRadarSection/SectionMap/GangOverlay, DrawRadarMask, DrawRotatingRadarSprite, DrawYouAreHereSprite, DrawRadarSprite, DrawMap/DrawRadarMap, Initialise, LoadTextures, Shutdown, Request/RemoveMapSection, StreamRadarSections, Load, Save; DrawLegend LOW/HIGH polygons and the sprite path.
Gate: Release + RW built; wave2_oracle_test PC24 0 / PC53 0 (1142 functions); gameplay_regress combat PASS=14 (run directly, -b worktree). v6 pre-commit grep clean; no signature/x87d changes, PortableWin64 compile not run.
