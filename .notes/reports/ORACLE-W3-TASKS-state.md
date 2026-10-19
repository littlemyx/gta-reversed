# ORACLE-W3-TASKS state (hand-back at the tool-call cap)

Landed on branch oracle-w3-tasks (see ORACLE-W3-TASKS.md): wave3_tasks_oracle_test 40 rows (PC24 0 / PC53 0), 2 port fixes, coverage 3203 -> 3243. 117 of the 167 uncovered A/B TaskTypes functions are skipped with a reason in tests/standalone/tools/w3_tasks_skip.tsv.

## Next steps (in order of yield)
1. The ~97 fixture-limited rows (31 process kills + 66 identical faults, listed in w3_tasks_skip.tsv): take 3-4 representative functions, disassemble (`python tools/standalone/exedis.py ADDR --exe gta_sa_compact.exe --repo .`), find the pointer chain the exe dereferences (task member -> struct, ped -> m_pVehicle -> ..., arguments) and add a generic fixture to `gen_w3_tasks.py` / `W3TaskCase`: a kind 'g' for non-entity pointer members (point at a registered scratch block), a real vehicle for ped->m_pVehicle, a real CScriptedEffectPair / CPedAttractor / CPointRoute pool. Remove the rows from the skip tsv, regenerate, run `C:/gta-scratch/w3tasks/loop.sh` (runs the test exe repeatedly; a row that kills the process is appended to skip.txt and the run resumes; `an.py <log>` summarises clean / mismatching / not-oracle-able rows).
2. The two open candidates (CarGoToPointNearDoor Clone 0x646EC0, CarShuffle Clone 0x649C40) with a valid object.
3. Hand rows: template SeekEntity classes, private ctor overloads, CEntity** argument functions.
4. Artefact rows: pool-slot / `this` masks (padding, embedded vptr) instead of skipping.

Scratch: C:/gta-scratch/w3tasks (sites.json, loop.sh, an.py, loop_prefix.log = pre-fix evidence, run logs).
Regenerate the rows: `python -X utf8 -I tests/standalone/tools/gen_w3_tasks.py . C:/gta-scratch/w3tasks/sites.json tests/standalone --classes A,B --skip tests/standalone/tools/w3_tasks_skip.tsv` (sites.json = oracle_coverage.py --json from BEFORE the rows landed, so "covered" does not hide the generated ones; make a new one from a checkout without the gen file if the sites change).
Run one row: `RW_EXE_ORACLE=<gta_sa_compact.exe> W2_PROGRESS=1 ./wave3_tasks_oracle_test.exe <row substring>` in build/StandaloneRW/source.
