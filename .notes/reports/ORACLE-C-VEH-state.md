# ORACLE-C-VEH state (helper landed, budget exhausted before the crash rows)

CONTINUE: ORACLE-C-VEH-state.md. Done: helper (`vtCls`, see ORACLE-C-VEH.md), 24 rows, 2 fixes. Open, by value:
1. The 11 process-killing rows (CAutomobile::Fix / ProcessAutoBusDoors / CloseBoot / Teleport, CBike::CalculateLeanMatrix / FixHandsToBars / Teleport, CBoat / CTrain::Teleport, CVehicle::UpdateWinch / UpdateTrailerLink): remove their lines from
   tests/standalone/tools/w3_skip.tsv, regenerate, run under cdb (`sxd av;g;k`) with `-n 8` to find the unhandled fault (suspects: stack overflow through a virtual re-entry, or a fault inside RTC-framed code after the first agreed fault).
   Driver for crashing rows: W2_PROGRESS=1 W2_SKIPFILE=<file> W2_START=<row> (see wave1_oracle_test.cpp).
2. GetComponentWorldPosition: constrain the component index (hand row).
3. Lights (DoVehicleLights, DoHeadLightBeam / Reflection*; CMatrix& argument): hand rows with CCoronas / CShadows / CPointLights snapshots.
4. The RW-bound rows (clump + frames, ~30) need an RW 3.7 clump built on the exe side.
