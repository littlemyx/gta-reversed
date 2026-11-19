# ASM-P4R2ad
Zone: runtime of garages/car generators/zones/ped groups/enex/etc.; 153 candidate functions with an address not yet in a result tsv, ~60 non-trivial (>=14 instrs).
Review of P4R2ac: ProcessAutoBusDoors, ScanForCrimes, CWeapon::Update reload audio - all CONFIRMED against the exe.
Compared 6 (see P4R2ad.result.tsv), fixed 2:
- DoInternalProcessing: regression from SH3 debug-log commit 80d427780 (6 unbraced `if (...) g_cgGenWhy=..; return;`) - return was unconditional, car generators never spawned a car. Braces restored.
- CPedGroupMembership::Process: distance removal requires same area code as the leader.
Build: StandaloneRelease links. No tests/regress run (brief: no game runs).
