# ZONEINFO — callers of CTheZones::GetZoneInfo(const CZone*) and zone-info index handling

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh zoneinfo.
## Evidence (.notes/reports/TRAP-AUDIT.md, "Callees")
CGangWars::CanPlayerStartAGangWarHere 0x443F80: the exe compares against &ZoneInfoArray[idx] with no index-0 nullptr and no bounds check;
the port's GetZoneInfo helper returns nullptr for index 0 and bounds-checks. Other users may share the deviation: GangWars.cpp:106/693/710/
1187, Radar.cpp:1705, ZoneCommands.cpp:19 (script opcodes), plus anything else calling GetZoneInfo. Gang territories and zone names hang on it.
## Task (one helper family)
For every port call site of GetZoneInfo (grep), read the exe code at the corresponding address: does the exe special-case index 0 / bounds?
Fix each site to the exe (one commit per file, asm addresses in the subject). Add rows where the function is oracle-testable (wave3_gameplay
has the zone fixture: TotalNumberOfNavigationZones, ZoneInfoArray). HARD budget under 200k tokens.
## Acceptance
Table site → exe behaviour → changed/unchanged; rows PC24 0; report ≤15 lines with start/end time.
