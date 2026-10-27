# ASM-S13b (the 12 TaskSimple* functions left from S13) - 2026-10-27
Result: 8 compared (3 FIXED in 3 commits, 5 MATCH, 0 UNSURE), 4 NOT REACHED (token budget; table in S13b.result.tsv).
Fixes (exe address, one line each):
- 9c6e5c5c CTaskSimpleGetUp::StartAnim (0x67C7DC-0x67C86A): the ignored-collision-vehicle test was inverted - the exe runs ProcessColModels only when (seed+frame-3)%8==0 and gets the ped up only if it reports no overlap; on the other 7 frames the ped stays stuck under the car (port: `|| frame-check ||` got up on those frames).
- 402b4f94 CTaskSimpleJetPack::DoJetPackEffect (0x67B814-0x67B8C7): creating a thruster fx resets m_FxKeyTime to 0, creation precedes the key-time update (port never reset it on re-creation).
- 19e0969d CTaskSimpleJetPack::RenderJetPack (0x67F72C-0x67F999): jetball1 rotX = ThrustAngle*57.29578 + (delta*57.29578)*10, jetball2 minus (port used the minus form for both); heading wrap compares strict.
MATCH: GoToPoint::ProcessPed, GoToPointFine::SetBlendedMoveAnim, HoldEntity::ProcessPed, HoldEntity::SetPedPosition, JetPack::DropJetPack (only NaN compare edge cases differ).
NOT REACHED: GunControl::ProcessPed, InAir::ProcessPed, JetPack::ProcessControlInput, JetPack::ProcessThrust (401-line disassemblies).
Note: RenderJetPack's stop-path divides the exact int time by 79.577 in the exe (no float rounding of the int first); differs from the port only past 2^24 ms (4.6 h), not fixed.
Consumer audit: no layout/signature change; all fixes are local to the task's own update/render.
Gate: Release build OK (1176/1176); no oracle row covers these functions; soak RESULT: OK (3 min NOVSYNC).
