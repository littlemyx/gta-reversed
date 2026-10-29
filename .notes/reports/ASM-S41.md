# ASM-S41 (phase 2: AttractorScanner, PedAttractor*, CarAI) - 2026-10-29
Result: 40 rows, 31 compared (23 MATCH, 8 FIXED in 8 commits), 9 NOT_REACHED (budget: PlacePedAtEffect + the 7 CarAI rows; the "?" RunningScript row has no address). Tables in S41.result.tsv.
Fixes (exe address, one line each):
- f27b123a CPedShelterAttractor::GetDisplacement (0x5EF420-0x5EF56B): the 1.0 min-distance test is (dz2+dy2)+dx2 on unrounded dx,dy (port: float dx2+dy2+dz2).
- 9aff650d CPedAttractor::ComputeAttractPos (0x5EA110): offset is (n*dir.c)*spacing per component (port: (spacing*n)*dir.c).
- 896e0149 CPedAttractor::ComputeAttractHeading (0x5EA1C0): the port overwrote m_UseDir with m_QueueDir; the exe only selects the direction.
- 27548fe2 CPedAttractor::DeRegisterPed (0x5EC5B0): searched the queue and erased from the attract list (wrong list); exe searches m_AttractPeds, otherwise BroadcastDeparture; pair erase guarded.
- dfb9f6d8 CPedAttractor::BroadcastDeparture (0x5EF160): the port worked on m_AttractPeds with slot i; the exe works on the queue (m_ArrivedPeds), slot i-1, erases the queue entry, then re-routes the attract list.
- 1b263ef6 CPedAttractor::RegisterPed (0x5EEE30): the ped goes into m_AttractPeds with slot = queue size (port pushed it into the queue).
- bde48135 CPedAttractor::BroadcastArrival (0x5EEF80): followers get literal PEDMOVE_WALK (4), not m_MoveState; attract-list erase guarded.
- b38ec8e5 CPedAttractor::SetTaskForPed (0x5EECA0): ms_tasks.push_back(task) was missing on both paths; old-task erase/delete guarded as in the exe.
- 09e3cdf6 SArray::emplace_back (NOTSA, no exe counterpart): growth policy reserve(size*1.5) never grew an empty or 1-element array (write through nullptr / past the end); now at least size+1.
MATCH: the whole CAttractorScanner (6 functions: sector range with the 0.02f/60.0f x87 chain, timer, scripted loop, rain/shelter polarity, z,y,x sums, NaN arms), ComputeDelta*, AbortPedTasks, Constructor, Destructor, the 9 GetType getters, GetHeadOfQueueWaitTime, Shelter BroadcastDeparture / ComputeAttractPos.
UNSURE (not touched): CPedAttractor ctor uses strcpy_s for the 8 byte script name; the exe's strcpy overflows for an unterminated 8-char name.
Consumer audit: m_UseDir has no other reader/writer; ms_tasks readers are GetTaskForPed (erase, now guarded by presence since SetTaskForPed fills it) and Shutdown; the attractor lists are only used inside CPedAttractor and the Shelter subclass; SArray users are the attractor classes only (growth change is safe for them).
Open: PedAttractorPedPlacer::PlacePedAtEffect (0x5EA390), CarAI 0x41BFA0 / 0x41C070 / 0x41C4A0 / 0x41C600 / 0x41C760 / 0x41C8A0 / 0x41C900.
Gate: Release + RW builds OK; wave2_oracle_test (1142 functions) PC24 0/0, PC53 0/0; combat regress PASS=14; PortableWin64 compile of PedAttractor / PedShelterAttractor / PedAttractorManager objects: 0 errors; v6 pre-commit grep clean. Trailer uses the brief's Opus 5.5 text.
