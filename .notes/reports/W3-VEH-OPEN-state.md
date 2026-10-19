# W3-VEH-OPEN state (for a continuation agent)

Landed: see `W3-VEH-OPEN.md` (3 open rows fixed, 2 new rows, coverage 2963 -> 2965). Open work, in priority order:

1. `CVehicle::DoHeadLightEffect` 0x6E0A50 / `DoTailLightEffect` 0x6E1780 (class B, hand rows; the generator reports NOARG for `CMatrix& vehicleMatrix`): signatures in Vehicle.h; fixture = a CMatrix in memory shared by both sides, CCoronas / CShadows / CPointLights arrays compared (the W3 harness compares the exe data image 0x8A4000..0xCB0000 and restores it). Pattern for hand rows: tests/standalone/wave3_oracle_veh_hand2.inc.
2. `CHeli::Fix` 0x6C4530 / `CPlane::Fix` 0x6CABB0: fake vtable whose `SetupDamageAfterLoad` slot is the port function (raw address needed; `&CHeli::SetupDamageAfterLoad` on a virtual is a vcall thunk).
3. `CTrain::GetDooorAngleOpenRatio` 0x6F59C0: every case faults in both sides, even as a hand row; find the fault (first SEH frame) - the callee is trivial.
4. Lessons: rows that pass on garbage need game-range + edge-value fixtures too; with `/arch:IA32 /fp:precise` a `double` local mirrors an x87 register and a `float` local an `fstp dword` spill (see CBike::DoSoftGroundResistance).

Worktree recipe unchanged (see ORACLE-W3-VEH-state.md); sites json for gen_w3_veh.py: `tools/standalone/oracle_coverage.py --json` (the generator recomputes "covered" itself from tests/standalone).
