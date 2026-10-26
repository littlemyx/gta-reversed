# TP-HEAP - heap corruption (0xc0000374) after `cmd:tp` late in the soak route (rules v5, 2026-10-26 03:48-)

## Result
Root cause = a plain buffer overflow in `CVehicleModelInfo::GetWheelPosn` (exe 0x4C7D20), NOT the teleport itself. The tp only moves CJ under the LS airport approach, where
`CPlane::DoPlaneGenerationAndRemoval` spawns planes: `CPlane::CPlane` -> `CAutomobile::CAutomobile` -> `SetupSuspensionLines` -> `GetWheelPosn(wheel, vec, local=false)` for a
VEHICLE_TYPE_PLANE takes the temp-matrix branch (the other model types read the frame matrix directly).

## The corrupting writer
`auto matrix = RwMatrixCreate(); memcpy(matrix, RwFrameGetMatrix(frame), sizeof(CMatrix));` - `sizeof(CMatrix)` = 0x48 (CMatrix = RwMatrix 0x40 + m_pRwMatrix + owner flags), `RwMatrixCreate()`
allocates sizeof(rw::Matrix) = 0x40 from the CRT heap: 8 bytes written past the block (and read past the frame matrix) = the next heap entry's header -> 0xc0000374 at some later free/alloc
(10 s after the tp, at idle start or in the quit sequence: whichever touches that heap neighbourhood first). The exe copies exactly 0x10 dwords (`mov ecx, 0x10 / rep movsd`, 0x4C7D5A..0x4C7D61).
Fix: `memcpy(matrix, RwFrameGetMatrix(frame), sizeof(RwMatrix))`. Consumer audit: it is the only `memcpy(..., sizeof(CMatrix))` into/out of an RwMatrix in source/ (grep).

## How it was caught (first corruption, not the late detection)
Soak route + `cmd:tp:2100:-1790:13.5` (C:/gta-scratch/rf_route_tp.txt) under cdb x86 (a process started by the debugger gets the debug heap: tail/free checks, no registry/gflags change), via the new
`SOAK_WRAP=<cmd>` of soak.sh (Windows host: prefix of the game command). Both cdb runs (exe of da5c6902, 2 of 2) stopped at the free of the corrupted block:
`HEAP[gta_reversed.exe]: Heap block at 1BFB6B60 modified at 1BFB6BA8 past requested size of 40` (hex: 0x40 = 64 bytes asked, byte 0x40 modified) with the stack
`RtlFreeHeap <- free <- rw::Matrix::destroy <- RwMatrixDestroy <- CVehicleModelInfo::GetWheelPosn+0x76 <- CAutomobile::SetupSuspensionLines+0x89 <- CAutomobile::CAutomobile+0x898
<- CPlane::CPlane+0x46 <- CPlane::DoPlaneGenerationAndRemoval+0x476 <- CCarCtrl::GenerateRandomCars <- CGame::Process`. The block dump is a valid RwMatrix (right/up/at/pos) with the 8 bytes of the CMatrix tail behind it. Logs: C:/gta-scratch/tp-heap/logs/cdb-035039.log, cdb-035622.log (cdb-1 reported the same block message at car_exit, cdb-2 at the free in GenerateRandomCars).

## Verification (exe of this worktree, Release, NOVSYNC, windowed, private data dir C:/gta-tph, silent)
* Before: 3 of 3 plain runs APPCRASH c0000374 (ROUTE-FLAKES 3), 2 of 2 under cdb stopped at the tail check above.
* After: cdb x86 debug-heap run: RESULT OK, 0 `HEAP[` messages (quit_ask reached); 5 of 5 plain runs RESULT OK (outputs C:/gta-scratch/tp-heap/out/fix-1..5, same repro route
  C:/gta-scratch/tp-heap/route_tp.txt = rf_route_tp.txt). The repro route is the old soak route + tp: its `until:invehicle` hard gate timed out in 4 of the 6 runs (the ROUTE-FLAKES 1 artefact, fixed in the
  current soak_route.txt, unrelated). Soak = these runs. NOT run: run_all_tests (the change is one memcpy length; a GetWheelPosn oracle row cannot see the overflow), gameplay_regress.
* Tooling: soak.sh `SOAK_WRAP=<cmd>` (Windows host) = prefix of the game command, e.g. C:/gta-scratch/tp-heap/cdbwrap.sh (cdb, breakpoint script on the debug-heap `int 3` in RtlZeroHeap + AV/c0000374 handlers).
  Method for a late-detected heap corruption without gflags/registry: run under cdb (debug heap = tail/free fill checks are on for a debugger-started process), the break comes at the free of the damaged block.
