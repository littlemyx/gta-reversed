# ASM-S68 (Cheat.cpp cheat effect functions, 36 rows) - 2026-10-30
Result: 36 compared in full (3 FIXED in 1 commit, 33 MATCH, 0 UNSURE, 0 NOT REACHED). Table in S68.result.tsv.
Fixes:
- 8795876c CCheat::WeaponCheat1/2/3 (0x4385B0-0x438D50): player 0 is fetched with FindPlayerPed(-1) (port: 0, differs only with PlayerInFocus != 0); WeaponCheat1 makes only 9 of its 10 models deletable in the exe order BRASSKNUCKLE, MICRO_UZI, BAT, MOLOTOV, CHROMEGUN, AK47, CUNTGUN, ROCKETLA, SPRAYCAN (Colt45 stays GAME_REQUIRED; the port made all 10 deletable in request order).
Also verified (no change): m_aCheatFunctions (all 90 entries) and m_aCheatHashKeys (90 entries; slots 90/91 are 0 in the exe) in the NOTSA_GLOBAL_DEF initialisers equal the exe data at 0x8A5B58 / 0x8A5CC8; every Toggle index, ped-model list, clothes texture/model name and part, vehicle model id, weapon type/ammo and stat id checked against the disassembly.
Not in the slice: AddToCheatString (hash scan) is oracle-covered (logic_oracle_misc.inc), VehicleCheat is S43b MATCH.
Gate: StandaloneRelease build OK; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=compact exe) 1142 functions PC24 0/0, PC53 0/0; combat regress TOTAL PASS=14; v6 pre-commit grep clean; PortableWin64 compile of Cheat.cpp (x64 cl, PCH off) OK.
