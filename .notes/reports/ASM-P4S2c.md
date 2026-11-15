# ASM-P4S2c: CEventHandler + CTheScripts (continuation of P4S2 / P4S2b)

10 rows compared against the 1.0 compact exe: 8 MATCH, 2 FIXED, 0 UNSURE. Table: .notes/asm_sweep/P4S2c.result.tsv.

Fixes
- 243f7eb7d CEventHandler::ComputeSeenPanickedPedResponse: reads the source ped's (e->m_ped) current event, not this handler's; smart flee scream = true.
- e3462fbb9 CTheScripts::ProcessAllSearchLights: bulb pitch = atan2(z, unrounded sqrt(x^2+y^2)).

TS: ComputeSeenPanickedPedResponse: the current event / flee source comes from the event's source ped's event history (not the responding ped's); CTaskComplexSmartFleeEntity gets scream = true.
TS: ProcessAllSearchLights: none (x87 rounding only).

gameplay_regress not run (GPU held by the orchestrator gate): left to the gate.
