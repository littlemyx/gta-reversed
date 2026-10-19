// ORACLE-W3-WORLD rows A: World.cpp / World_Helper.cpp (see wave3_oracle_world.inc). ROW / IC / Tw are defined there.
static void TestW3WorldA() {
    using namespace w2; using namespace w3w;
    // ---- sector accessors / scan code / line test options
    ROW("CWorld::GetCurrentScanCode", 0x407250, 50) {
        Tw tw([](Rng& rr) { CWorld::ms_nCurrentScanCode = (uint16)rr.u32(); });
        IC(return (uint64_t)CWorld::GetCurrentScanCode();, return (uint64_t)oracle::Fn<uint16_t __cdecl()>(0x407250)(););
    } ENDROW;
    ROW("CWorld::GetSector", 0x407260, 800) {
        const int k = r.below(10);
        const int x = k == 0 ? (int)r.u32() : k == 1 ? -(int)r.below(300) : k == 2 ? 100 + (int)r.below(60) : (int)r.below(120);
        const int y = k == 3 ? (int)r.u32() : k == 4 ? -(int)r.below(300) : k == 5 ? 100 + (int)r.below(60) : (int)r.below(120);
        IC(return U(&CWorld::GetSector(x, y));, return (uint64_t)oracle::Fn<uint32_t __cdecl(int, int)>(0x407260)(x, y););
    } ENDROW;
    ROW("CWorld::GetRepeatSector", 0x4072A0, 800) {
        const int k = r.below(6);
        const int x = k == 0 ? (int)r.u32() : k == 1 ? -(int)r.below(300) : (int)r.below(140) - 10;
        const int y = k == 2 ? (int)r.u32() : k == 3 ? -(int)r.below(300) : (int)r.below(140) - 10;
        IC(return U(&CWorld::GetRepeatSector(x, y));, return (uint64_t)oracle::Fn<uint32_t __cdecl(int, int)>(0x4072A0)(x, y););
    } ENDROW;
    ROW("CWorld::GetLodPtrList", 0x4072C0, 400) {
        const int x = r.below(30), y = r.below(30);
        IC(return U(&CWorld::GetLodPtrList(x, y));, return (uint64_t)oracle::Fn<uint32_t __cdecl(int, int)>(0x4072C0)(x, y););
    } ENDROW;
    ROW("CWorld::ResetLineTestOptions", 0x5631C0, 60) {
        IC(CWorld::ResetLineTestOptions(); return 0;, oracle::Fn<void __cdecl()>(0x5631C0)(); return 0;);
    } ENDROW;
    // ---- player lookups
    ROW("CWorld::FindPlayerSlotWithPedPointer", 0x563FA0, 300) {
        const int k = r.below(6);
        void* arg = k < 2 ? (void*)CWorld::Players[k].m_pPed : k < 4 ? (void*)PedP(r.below(4)) : nullptr;
        IC(return (uint64_t)(uint32_t)CWorld::FindPlayerSlotWithPedPointer(arg);, return (uint64_t)oracle::Fn<uint32_t __cdecl(void*)>(0x563FA0)(arg););
    } ENDROW;
    ROW("CWorld::FindPlayerSlotWithRemoteVehiclePointer", 0x563FD0, 300) {
        const int k = r.below(6);
        void* arg = k < 2 ? (void*)CWorld::Players[k].m_pRemoteVehicle : k < 4 ? (void*)VehP(r.below(4)) : nullptr;
        IC(return (uint64_t)(uint32_t)CWorld::FindPlayerSlotWithRemoteVehiclePointer(arg);, return (uint64_t)oracle::Fn<uint32_t __cdecl(void*)>(0x563FD0)(arg););
    } ENDROW;
    ROW("CWorld::FindPlayerSlotWithVehiclePointer", 0x564000, 300) {
        const int k = r.below(6);
        void* arg = k < 2 ? (void*)(CWorld::Players[k].m_pPed ? CWorld::Players[k].m_pPed->m_pVehicle : nullptr) : k < 4 ? (void*)VehP(r.below(4)) : nullptr;
        IC(return (uint64_t)(uint32_t)CWorld::FindPlayerSlotWithVehiclePointer((CEntity*)arg);, return (uint64_t)oracle::Fn<uint32_t __cdecl(void*)>(0x564000)(arg););
    } ENDROW;
    // ---- FindPlayer* (World_Helper.cpp)
    const auto Pid = [](Rng& rr) { return rr.below(4) == 0 ? -1 : (int)rr.below(2); };
    const auto FixPeds = [](Rng&) { for (int k = 0; k < 2; ++k) if (!CWorld::Players[k].m_pPed) CWorld::Players[k].m_pPed = reinterpret_cast<CPlayerPed*>(PedP(k)); };   // (no null check in the exe either)
    ROW("FindPlayerCoors", 0x56E010, 600) {
        const int id = Pid(r);
        IC(*reinterpret_cast<CVector*>(S(oOut)) = FindPlayerCoors(id); return 0;, oracle::Fn<void __cdecl(CVector*, int)>(0x56E010)(reinterpret_cast<CVector*>(S(oOut)), id); return 0;);
    } ENDROW;
    ROW("FindPlayerSpeed", 0x56E090, 600) {
        const int id = Pid(r);
        Tw tw(FixPeds);
        IC(return U(&FindPlayerSpeed(id));, return (uint64_t)oracle::Fn<uint32_t __cdecl(int)>(0x56E090)(id););
    } ENDROW;
    ROW("FindPlayerEntity", 0x56E120, 600) {
        const int id = Pid(r);
        Tw tw(FixPeds);
        IC(return U(FindPlayerEntity(id));, return (uint64_t)oracle::Fn<uint32_t __cdecl(int)>(0x56E120)(id););
    } ENDROW;
    ROW("FindPlayerTrain", 0x56E160, 600) {
        const int id = Pid(r);
        IC(return U(FindPlayerTrain(id));, return (uint64_t)oracle::Fn<uint32_t __cdecl(int)>(0x56E160)(id););
    } ENDROW;
    ROW("FindPlayerCentreOfWorld", 0x56E250, 800) {
        const int id = Pid(r);
        Tw tw(FixPeds);
        IC(return PtrId(&FindPlayerCentreOfWorld(id));, return PtrId(oracle::Fn<const void* __cdecl(int)>(0x56E250)(id)););
    } ENDROW;
    ROW("FindPlayerCentreOfWorld_NoSniperShift", 0x56E320, 800) {
        const int id = r.below(2);   // (the exe indexes Players[] with the raw id in places: every caller passes 0)
        IC(return PtrId(&FindPlayerCentreOfWorld_NoSniperShift(id));, return PtrId(oracle::Fn<const void* __cdecl(int)>(0x56E320)(id)););
    } ENDROW;
    ROW("FindPlayerHeading", 0x56E450, 800) {
        const int id = r.below(2);   // (raw Players[id] indexing in the exe: every caller passes 0)
        Tw tw(FixPeds);
        IC(return FB(FindPlayerHeading(id));, return FB(oracle::Fn<float __cdecl(int)>(0x56E450)(id)););
    } ENDROW;
}
