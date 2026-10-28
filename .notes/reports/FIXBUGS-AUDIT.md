# FIXBUGS-AUDIT (2026-10-28)
Table: .notes/asm_sweep/FIXBUGS.tsv (paths relative to source/game_sa unless source/app). 80 rows: 57 OK-NONFIX, 3 WRONG-SIDE, 1 NEITHER, 0 UNSURE, 19 NOT_REACHED (budget; all unguarded comment-only "FIX_BUGS" fixes in UI/streaming/loaders/replay and todo/#if 0 mentions).
Method: every `#ifdef/#ifndef/#if FIX_BUGS` block read; the gameplay ones (Entity/Vehicle, Tasks, Events, Pop, World, EntryExit) compared with the exe by exedis (19 asm-read); render/UI/platform ones judged from source only (marked "not asm-read").
Extra finding: the real WRONG-SIDE cases were UNGUARDED fixes whose comment says "FIX_BUGS" (they run in the run build too), not the #ifdef blocks. The same pattern is probably in the 19 NOT_REACHED rows.
Fixes (one commit each, FIX_BUGS side unchanged):
- 7b8879af CPopCycle::Initialise 0x5BC090 (NEITHER): exe always rescales the 18 percs by 100/sum (ftol) and adds 100-newSum to the last maximal element; port rescaled only when sum<100, no remainder.
- bc7a4efd CGameLogic::RestorePedsWeapons 0x441D30: exe tests modelId1 twice (0x441D7D); port tested both models.
- 503429e9 CRadar::SetupAirstripBlips 0x587D20: exe gates on (CTimer::m_FrameCounter & 4) (0x587D72); port used a 200 ms timer.
- dfaca2ee CPed::PreRenderAfterTest 0x5E65A0: exe shadow test is pedType == PLAYER1 only (0x5E6777); port used IsPlayer() (+PLAYER2).
Not changed: Cheat.cpp:224 (exe does ClearAdrenaline, asm read), Bike/Trailer ProcessEntityCollision null guards (exe has none, ocd unused elsewhere).
Gate: Release build OK (0 errors, full build + incremental after the Ped edit). No oracle target covers the 4 functions (grep of tests/); all four edits sit on the non-FIX_BUGS side or keep the old code under FIX_BUGS, so the oracle targets (which define FIX_BUGS) compile unchanged code; no oracle baseline/rerun done. Portable x64 build not run (no portable-affecting API; new code uses NOTSA_FTOL, float only).
Soak (NOVSYNC, 3 min): RESULT: OK (NOVSYNC, route 406 s, attempt 1).

## Round 2 (FIXBUGS-AUDIT-2)
Table now 118 rows (the 19 NOT_REACHED rows reached except 4: Sprite2d, Streaming ExtraObjectsDir, MenuManager x3, aggregate "remaining BUG: comments"). New verdicts this round: 3 UNGUARDED-FIX, 30 OK-FAITHFUL, 19 DOCUMENTED-DEVIATION, 2 UNSURE (TheScripts debug box, MenuManager_Process SetDir), 4 NOT_REACHED. Earlier: 57 OK-NONFIX, 3 WRONG-SIDE, 1 NEITHER.
Method: grep of bugfix / fixed / FIX_BUGS / BUG: / NOTSA: fix comments in source/game_sa; candidates judged by the wording and the guard next to them (IsFixBugs / bugfixes:: / NOTSA_STANDALONE_RUN), then asm-read where the port deviated unguarded. The rest of the BUG: hits (Camera, Cam, CarCtrl, Tasks, Audio, Renderer, Rope, Train, Weapon) say "original does X, kept" or are guarded; not each asm-read.
Fixes (one commit each, port code kept under FIX_BUGS):
- f89524cd CWaterCannon::PushPeds 0x7295E0: exe bbox starts (+10000, -10000); port FLT_MAX / FLT_MIN clipped the ped box at 0 on negative coords.
- adda5b4b CTaskSimpleSwim::ProcessEffects 0x68AA70: bubbles at matrix-array index 3 (BONE_SPINE1, 0x68AC8F); port used BONE_JAW.
- bb399c11 CReplay::RestoreStuffFromMem 0x45ECD0: the exe never restores ms_nNumGang; port did.
Crash guards left (DOCUMENTED-DEVIATION): pool-full null checks, TagManager::GetNearestTag empty list, 0-frame anim sequence, FileLoader null mi. FileLoader::LoadLine(buffer) skipping CR differs from 0x536FE0 but has no exe consumer (only the port-written CutsceneMgr).
Oracle: all three sites keep the old code under FIX_BUGS, so oracle targets (which define FIX_BUGS) compile unchanged code.
