# BUG2: soak intro NaN / wanted level 5 / CObject pool failures

Verdict: NOT a game-logic port bug. Cause = the soak's in-process sampler (NOTSA_STANDALONE_SAMPLER, on by default in tools/standalone/soak.sh).
It SuspendThread/GetThreadContext/ResumeThread-s the game thread every ~10 ms starting 35 s after launch (hence "~60 s after loading starts"). Under Wine on
Apple silicon this corrupts the thread's x87 register stack (FPU TOP drifts). Later fcomp/fld/fstp read empty registers -> junk compares and NaN.

Evidence
- Instrumented Release build (probes removed again, not committed): NaN first appears at many unrelated places each run (player ped, BMX/bike/car/object/ped,
  stored-poly Z, ProcessSphereTriangle normal, ApplyMoveForce force), never the same function => a shared low-level state, not one port defect.
- x87 TOP probe (fnstsw) at CWorld::Process stages: TOP walks 0->1->2->...->7->0 over time; per-function guards blamed pure-C++ functions
  (CCollision::TestSphereBox, ProcessSphereBox, ProcessLineTriangle, CAnimBlendClumpData::ForAllFrames...).
- TestSphereBox called in isolation with 20736 input combos (NaN/inf/denormal included): 0 TOP changes; its asm is balanced on every path => the drift is external.
- Symptoms of an empty-register read: ProcessSphereTriangle let dist=2.1066 pass `dist < r` (r=0.4); CVector::Normalise of a finite vector (0.68,-0.006,0.057) returned NaN.
- Hardware FP traps are not delivered under Wine/Rosetta (self-test: inf*0 raised nothing), so an invalid-op trap approach is blind here.
- NaN position -> CCullZones::Update: NaN passes every IsPointWithin test (exe has the same NaN-passes idiom, verified in asm at 0x415158 for the sibling code) -> military
  zone -> SetWantedLevelNoDrop(5). NaN cars/peds -> RemoveFallen* teleports, entities respawn -> CObject/CEntryExit pools fill; CPhysical::Add with a NaN bound rect -> bad sector index crash.
- S7 also saw a cheat-spawned taxi NaN right after creation: any x87 op after the corruption gives NaN, so spawn is covered by the same cause.

Fix: soak.sh no longer exports the sampler (opt in with SAMPLER=1, profiling only); warning comments in soak.sh and Fixups.cpp (commit a43d03b3). No port code changed.

Soak before/after (Release, own data dir /tmp/d3n, own prefix): before ~50% of 3-min runs hit NaN (wanted 5 in intro or NaN entities, later crash); after: 3 consecutive full routes
(walk, 2 cheat cars, drive, idle, quit) RESULT OK, 0 NaN lines, 0 "Allocation failed" (CObject pool 0, CEntryExit 0).

Not done / caveats: ApplyCollision ped-vs-ped was only a victim (NaN inputs arrived from corrupted state); no oracle case needed. Anything else that suspends the game thread
(debuggers, winedbg `bt`, stack.sh helpers) can corrupt x87 state the same way. The sampler could be fixed by sampling from within the thread (timer + signal-free EBP walk) if profiling is wanted.
