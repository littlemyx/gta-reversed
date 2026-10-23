# ORACLE-C-VEH state (after continuation 2)

Done: helper (`vtCls`), SafeSEH root cause of the 11 process-killing rows (fixed, see ORACLE-C-VEH.md "Continuation 2"), 3 more port fixes, Vehicle covered 239 -> 244.
CONTINUE (optional, by value): (1) link game_sa/World.cpp (+ the RwMatrixUpdate shim TU for CloseBoot) into wave3_vehicle_oracle_test and give the Teleport rows a CWorld sector fixture (4 gameplay rows);
(2) UpdateWinch: a rope fixture (sane m_fSegmentLength 0.5/0.9, a pool for the hook CObject); (3) lights (CMatrix& argument, CCoronas/CShadows/CPointLights snapshots); (4) RW-bound rows need an RW 3.7 clump on the exe side.
Remove a skip line from tests/standalone/tools/w3_skip.tsv, regenerate with tests/standalone/tools/gen_w3_veh.py (command in ORACLE-C-VEH.md), run `wave3_vehicle_oracle_test.exe -n 60 <row>`; debug with cdb (`sxd av;g`) and the linker map.
