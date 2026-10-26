# MAC-MERGECHK2 (2026-10-25): portable build after the merge of reverse/interior_c (8ec264bc)
- Merge: 24 commits, 12 source files (Automobile, Trailer, EventHandler, GroupEventHandler, PathFind, PedGroup, PedGroupIntelligence,
  PedGroupMembership, TaskComplexSeekEntity.h, TaskSimpleCarDrive, standalone/Lockstep.cpp, rw/rwmath_exact.h).
- PortableMac ninja (nice 15): GREEN as landed, 1136/1136, link ok. No source edit was needed.
- ctest -L portable: 19/19 passed. ctest -L mutation: 12/12 passed (no failure list).
## Scans on the 9 merge-changed game_sa TUs
- pc24_scan (--cc build/PortableMac): 0 AUTO/CHECK sites on lines added by the merge. The only AUTO (Automobile.cpp:4462 `std::fabs(carUpDown)`,
  TowTruckControl) is untouched pre-merge code with no double chain -> not converted. The merge code already carries x87d (Trailer
  SetupSuspension*, PathFind distSlot sqrt, PedGroupMembership rand*1/32768*100) written by the branch author.
- ftol_scan: 0 class A sites. New class B only (PedGroupMembership:272, PathFind:306, GroupEventHandler:618, TaskSimpleCarDrive:189/191,
  EventHandler:1082 moved block): rand*(1/32768)*K casts already float-chain / x87d; class B is not converted by the codemods.
- 0 x87d/X87R/X87W/NOTSA_FTOL conversions -> no TU touched -> no wine cl.exe A/B needed (Win32 trivially unchanged).
## Portable audit of the merge
- Lockstep.cpp new raw fopen/fwrite of 28-byte path nodes (NOTSA_LOCKSTEP_DUMPPN) and the extended "D" line sit inside the existing
  `NOTSA_STANDALONE_RUN && !NOTSA_PORTABLE` guard: not compiled on Mac. No new Win32 API, REPLAY hooks or layout asserts outside it.
- rwmath_exact.h gains a local X87W fallback (#ifndef X87W, token-identical to Base.h): compiles clean in unit-test targets.
## Boot check
- SKIPPED: `pmset -g assertions` UserIsActive = 0 (display not active). No play1 run.
No background tasks, caffeinate, wine or game processes left.
