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
