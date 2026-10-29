# ASM-S48c - 2026-10-29
18 rows: 14 MATCH (CGarage::Update now complete), 4 FIXED (CGameLogic::Update tail, ValidateVersion, CGame::Init2, CGame::InitialiseEssentialsAfterRW), 0 NOT_REACHED. Table: S48c.result.tsv.
Fixes:
- b3bf0f4c2 CGameLogic::Update (0x4437B6-0x4437E9): 2-player distance limit = fsqrt of the unrounded x87 (p1-p2) squares (y first); the direction vector p2-p1 is built afterwards (port measured the float vector).
- 0fb15b3e3 ValidateVersion (0x5BA0E3): strncpy(version_name, &buf[15], 64) with zero padding (port strncpy_s aborts on a full buffer).
- fd400c7c4 CGame::Init2: LoadPlayerSkin only for player 1 (0x5BA1F5; port loaded both), CDraw::ms_fLODDistance = 500 (0x5BA22C; port 0).
- 8daf01fc7 CGame::InitialiseEssentialsAfterRW: no PushMemId/PopMemId in the exe (port leaked MEM_30 on failure).
CGarage::Update: all remaining handlers (bomb shop, only-target, closes-on-touch, open-for-target-freeze, script ones, hideout, stay-open, impound, tuning, burglary) compared with the jump tables (0x44C864/874/88C/89C/8AC) and every float constant (64, 289, 900, 12.25, 100, 225, 16, 4900, 25, 3600, 4225, 2.0): equal.
CGameLogic::Update mission copy and tail: equal apart from the fix above (coop restart point, fade args, heading from the matrix, pad word cleared, direction negated for player 2).
CGame: Process, Shutdown, ShutDownForRestart, InitialiseWhenRestarting, ReInitGameObjectVariables, Initialise and the small helpers compared call by call with arguments and strings; only Init2 / EssentialsAfterRW / ValidateVersion differed. ValidateVersion failure path: the exe shows a loading screen and spins, the port aborts (kept).
