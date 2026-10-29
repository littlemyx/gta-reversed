# ASM-S42d (phase 2: CCarEnterExit remainder) - 2026-10-29
Result: 9 rows = 4 DEVIATION (fixed), 4 MATCH, 1 NOT_REACHED (GetNearestCarDoor, budget). Table: S42d.result.tsv.
Fixes (one commit per function, subject lines carry the exe range):
- AddInCarAnim: boat driver anim DRIVE_BOAT (81) is used when the handling does NOT have sit-in-boat; with the flag it is CAR_SIT (port was inverted).
- QuitEnteringCar: undoes the entering: ClearGettingInFlags (port Set), bIsBeingCarJacked cleared (port set), NumGettingIn only decremented when > 0, bike bGettingPickedUp (+0x614 bit 3; port cleared bIsBig), ped collision set to true (port false).
- SetAnimOffsetForEnterOrExitVehicle: int_office is requested/ref-ed before int_house.
- SetPedInCarDirect: passenger acquaintance loop only for index > 0.
MATCH: GetPositionToOpenCarDoor, IsPlayerToQuitCarEnter (task sets decoded from the exe jump tables), IsRoomForPedToLeaveCar, GetNearestCarPassengerDoor.
Notes: x87 extended intermediates (sqrt/sum not stored) differ from the float port in IsRoomForPedToLeaveCar / IsPlayerToQuitCarEnter, never a stored float except doorAngle (heading + pi/2 rounded once in the exe, twice in the port: 1 ulp, UNSURE, left). The exe leaves dyRR uninitialised in GetNearestCarPassengerDoor when door 9 is not found; the port uses 999.
