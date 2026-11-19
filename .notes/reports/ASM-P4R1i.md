# ASM-P4R1i: review of P4R1h fixes, remaining non-story handlers, CRunningScript helpers

Part A (review vs exe): DRAW_WINDOW (0x477017), INCREMENT_INT_STAT_NO_MESSAGE (0x47B8CE), DISPLAY_TEXT_WITH_FLOAT (0x4730F4) all CONFIRMED.

Changes:
- SET_NO_RESPRAYS (exe 0x47B983-0x47B9A5): AllRespraysCloseOrOpen(enabled) -> AllRespraysCloseOrOpen(!enabled) (bOpen = !NoResprays). Commit 68189b08d.

Compared MATCH: ~38 handlers of todo.tsv and helpers CollectParameters, ReadArrayInformation, GetPointerToScriptVariable, UpdateCompareFlag, StartNewScript (both), CleanUpThisObject, IsPlayerOnAMission, PlayAnimScriptCommand, GivePedScriptedTask(int32). See P4R1i.result.tsv.
Open: GetLatestConsoleCommand (unused, stores 0 vs exe writes nothing); NOT_REACHED: GetRandomCharInZone, CSetPieces::AddOne, LoadAndLaunchMissionInternal, SetTextLabel<16>.
