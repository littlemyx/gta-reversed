# ORACLE-W3-TASKS state (after continuation 2)

Landed on branch oracle-w3-tasks2 (rebased on interior_c): wave3_tasks_oracle_test 83 rows (PC24 0), 9 port fixes, coverage 3343 -> 3386. See ORACLE-W3-TASKS.md "continuation 2".

## Next steps (in order of yield)
1. The ~38 "every case faults identically" rows in tests/standalone/tools/w3_tasks_skip.tsv: each carries the exe fault eip + accessed offset. Run with `W2_FAULTLOG=1` (e.g. `C:/gta-scratch/w3tasks2/loop.sh`, `fl.py <log>`), disassemble the eip (`python tools/standalone/exedis.py ADDR --exe gta_sa_compact.exe --repo .`), find which argument / member is null (`ped->m_pVehicle`, `m_pTarget`, a CColModel / CModelInfo chain for GetColModel@+0x14), add a typed kind in `W3_PTR_KIND` / `W3_MEMBER_KIND` (gen_w3_tasks.py) and `PutEntTo` (wave3_oracle_tasks.inc). Remove the skip lines, regenerate (`C:/gta-scratch/w3tasks2/regen.sh`), `C:/gta-scratch/w3tasks2/cycle.sh <tag>` = build + loop + summary (cycle_<tag>.txt, fl_<tag>.out).
2. The 31 "process killed" rows (CreateFirstSubTask / Clone / Destructor with an SEH frame): same method; the fault kills the exe's process, so find the dereference by disassembly of the function.
3. Hand rows: MoveBackAndJump::CreateSubTask (one stack arg), template SeekEntity classes, private ctor overloads, CEntity** argument functions.
4. Padding / pool-slot masks instead of skips; the port `default: NOTSA_UNREACHABLE` vs exe `return null` in CreateSubTask switches.
Scratch: C:/gta-scratch/w3tasks2 (cycle.sh, loop.sh, fl.py, mkskip.py, undef.py = unresolved-symbol finder from the .obj files, loop_*.log, loop_prefix_*.log = pre-fix evidence).
