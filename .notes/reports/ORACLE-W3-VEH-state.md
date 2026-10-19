# ORACLE-W3-VEH state (for a continuation agent)

Landed: see `.notes/reports/ORACLE-W3-VEH.md` (81 rows, 6 fix commits, Entity/Vehicle covered 124 -> 209, A/B uncovered 44/58 -> 5/12).
Worktree recipe: `tools/standalone/windows/new_worktree.sh <id> --preset StandaloneRW` then `cmake -S . -B build/StandaloneRW -DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON` (the sccache + /Zi combination fails on the librw pdb).
Build/run: `ninja -j4 -C build/StandaloneRW wave3_vehicle_oracle_test` (via build_locked.sh, low priority, background), `RW_EXE_ORACLE=<gta_sa_compact.exe> wave3_vehicle_oracle_test.exe [row substrings] [-v] [W3_OPEN=1] [W2_ASSERT_STACK=1]`.
Regenerate rows: `python tests/standalone/tools/gen_w3_veh.py . <sites.json> tests/standalone --classes A,B --skip tests/standalone/tools/w3_skip.tsv` (sites.json from `tools/standalone/oracle_coverage.py --json`; Windows python needs C:/ paths).

## Remaining (in priority order)

1. OPEN rows, run with `W3_OPEN=1`: `CBike::DoSoftGroundResistance` 0x6B6D40 (2/200: exe NaN vs finite - sand/rail branch, compare `-0x6B6D40` asm with Bike.cpp:2334, suspect the wheel-ratio / surface test order or the forward-component removal), `CVehicle::CanVehicleBeDamaged` 0x6D1280 (1/120: result and the
   `bDamagedDueToFireOrExplosionOrBullet` flag differ - probably `CPhysical::CanPhysicalBeDamaged`, port vs exe, under garbage physicalFlags), `CBmx::LaunchBunnyHopCB` 0x6C0390 (skipped in w3_skip.tsv; 1/60, m_vecTurnSpeed.z sign of zero, compare ApplyTurnForce),
   `CAutomobile::DoSoftGroundResistance` 0x6A4AF0 (1/200 denormal speed: artefact, probably just needs the speed fixture bounded).
2. Remaining uncovered A/B (5 + 12): `CTrain::GetDooorAngleOpenRatio`, `CHeli::Fix`, `CPlane::Fix` (need a valid vptr: build a fake vtable whose SetupDamageAfterLoad slot points at the port function), `ProcessSuspension` (CAutomobile / CQuadBike),
   `SetRemoveAnimFlags`, `UpdateClumpAlpha`, `Setup/RemoveLighting`, `DoHead/TailLightEffect`, the RW callbacks (`SetCompAlphaCB`, `FindUpgradeCB`, `FindReplacementUpgradeCB`, `SetVehicleAtomicVisibilityCB`, `GetBoatAtomicObjectCB`, need a clump/atomic fixture in the exe's RW 3.7 layout).
3. Classes C / S / D of Entity/Vehicle (86 / 57 / 119) were out of scope.
4. A mutation round on unrelated functions (the fixes themselves were the mutants).
