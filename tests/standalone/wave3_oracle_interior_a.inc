// ORACLE-W3-INTERIOR rows A: FurnitureGroup_c / FurnitureSubGroup_c / FurnitureManager_c (see wave3_oracle_interior.inc). ROW / IC / Tw are defined there.
static const char* const kGroupNames[] = { "IT_SHOP", "IT_OFFICE", "IT_LOUNGE", "IT_BEDROOM", "IT_KITCHEN", "IT_MISC" };
static const char* const kSubNames[] = {
    "SHOP_UNIT1_L", "SHOP_UNIT1_R", "SHOP_UNIT1_M", "SHOP_UNIT2_L", "SHOP_UNIT2_R", "SHOP_UNIT2_M", "SHOP_UNIT3_L", "SHOP_UNIT3_R", "SHOP_UNIT3_M", "SHOP_UNIT4_L", "SHOP_UNIT4_R", "SHOP_UNIT4_M", "SHOP_CHECKOUTS", "SHOP_BASKETS", "SHOP_LIGHTS",
    "OFFICE_DESKS", "OFFICE_CHAIRS", "OFFICE_WATERCOOLERS", "OFFICE_FILINGCABS",
    "LOUNGE_SOFAS", "LOUNGE_CHAIRS", "LOUNGE_TVSTANDS", "LOUNGE_TVS", "LOUNGE_TABLES", "LOUNGE_LAMPS", "LOUNGE_HIFI_TABLES", "LOUNGE_VIDEOS", "LOUNGE_HIFIS", "LOUNGE_CONSOLES",
    "BEDROOM_BEDS", "BEDROOM_TABLES", "BEDROOM_DRAWERS", "BEDROOM_WARDROBES",
    "KITCHEN_UNIT_L", "KITCHEN_UNIT_M", "KITCHEN_UNIT_R", "KITCHEN_UNIT_SINKS", "KITCHEN_UNIT_FRIDGES", "KITCHEN_UNIT_OVENS", "KITCHEN_UNIT_WASHERS", "KITCHEN_UNIT_CORNERS", "KITCHEN_MICROWAVES", "KITCHEN_TOASTERS", "KITCHEN_TVS",
    "MISC_PLANTS", "MISC_LIGHTS", "MISC_RUGS", "MISC_PLATES", "MISC_FOOD", "MISC_MAGAZINES", "MISC_CLOTHES",
};
// a name: mostly an exact one, otherwise a near miss (case, truncation, suffix, other table, junk)
static std::string PickName(Rng& r, const char* const* tab, size_t n) {
    std::string s = r.below(12) == 0 ? std::string(r.below(2) ? "" : "IT_") : std::string(tab[r.below((int)n)]);
    const int m = r.below(8);
    if (m == 1 && !s.empty()) s.pop_back();
    else if (m == 2) s += "X";
    else if (m == 3) for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    else if (m == 4) s = kGroupNames[r.below(6)] + std::string(r.below(2) ? "" : "_") ;
    else if (m == 5) s = kSubNames[r.below((int)(sizeof(kSubNames) / sizeof(*kSubNames)))];
    else if (m == 6 && !s.empty()) s[(size_t)r.below((int)s.size())] ^= 0x20;
    return s;
}

static void TestW3InteriorA() {
    using namespace w2; using namespace w3i;
    ROW("FurnitureGroup_c::Init", 0x5910A0, 20) { const int g = r.below(9); IC(return (uint64_t)(uint32_t)FG(g)->Init();, return (uint64_t)ExeT<uint32_t>(0x5910A0, FG(g));); } ENDROW;
    ROW("FurnitureGroup_c::Exit", 0x5910B0, 300) { const int g = r.below(9); IC(FG(g)->Exit(); return 0;, ExeT<void>(0x5910B0, FG(g)); return 0;); } ENDROW;
    ROW("FurnitureGroup_c::AddSubGroup", 0x5910E0, 400) {
        const int g = r.below(9), sid = r.below(4) == 0 ? (int)r.below(100000) - 50000 : r.below(15), a = r.below(10), b = r.below(10), c = r.below(10), dd = r.below(10), cp = r.below(2), tall = r.below(2), steal = r.below(2);
        IC(return B8(FG(g)->AddSubGroup(sid, a, b, c, dd, (uint8)cp, tall != 0, steal != 0));, return (uint64_t)ExeT<uint8_t>(0x5910E0, FG(g), sid, a, b, c, dd, cp, tall, steal););
    } ENDROW;
    ROW("FurnitureGroup_c::GetFurniture", 0x591130, 500) {
        const int g = r.below(9), sid = r.below(4) == 0 ? (int)r.below(20) : r.below(15), id = r.below(3) == 0 ? -1 - r.below(2) : r.below(45), wl = r.below(5) == 0 ? (int)r.below(256) : (int)r.below(120);
        IC(return U(FG(g)->GetFurniture(sid, (int16)id, (uint8)wl));, return (uint64_t)ExeT<uint32_t>(0x591130, FG(g), sid, id, wl););
    } ENDROW;
    ROW("FurnitureGroup_c::GetRandomId", 0x591170, 500) {
        const int g = r.below(9), sid = r.below(4) == 0 ? (int)r.below(20) : r.below(15), wl = r.below(5) == 0 ? (int)r.below(256) : (int)r.below(120);
        IC(return (uint64_t)(uint32_t)FG(g)->GetRandomId(sid, (uint8)wl);, return (uint64_t)ExeT<uint32_t>(0x591170, FG(g), sid, wl););
    } ENDROW;
    ROW("FurnitureGroup_c::AddFurniture", 0x5C0230, 500) {
        const int g = r.below(9), sid = r.below(4) == 0 ? (int)r.below(20) : r.below(15), mid = g_wired[0] ^ 0, id = r.below(5) == 0 ? -1 : r.below(300), wmin = r.below(256), wmax = r.below(256), ang = r.below(256);
        (void)mid;
        int k = r.below(4);
        Tw tw([](Rng& rr) { (void)rr; });
        IC(return B8(FG(g)->AddFurniture(sid, (uint16)g_wired[k], (int16)id, (uint8)wmin, (uint8)wmax, (uint8)ang));, return (uint64_t)ExeT<uint8_t>(0x5C0230, FG(g), sid, (int)g_wired[k], id, wmin, wmax, ang););
    } ENDROW;
    ROW("FurnitureSubGroup_c::AddFurniture", 0x5C00C0, 800) {
        const int k = r.below(4), id = r.below(5) == 0 ? -1 : r.below(300), wmin = r.below(256), wmax = r.below(256), ang = r.below(256);
        Tw tw([](Rng& rr) { EnsureSubGroup(rr); });
        IC(return B8(g_sgPtr->AddFurniture((uint16)g_wired[k], (int16)id, (uint8)wmin, (uint8)wmax, (uint8)ang));, return (uint64_t)ExeT<uint8_t>(0x5C00C0, g_sgPtr, (int)g_wired[k], id, wmin, wmax, ang););
    } ENDROW;
    ROW("FurnitureManager_c::Exit", 0x5911A0, 200) { IC(FM()->Exit(); return 0;, ExeT<void>(0x5911A0, FM()); return 0;); } ENDROW;
    ROW("FurnitureManager_c::GetFurniture", 0x5911E0, 500) {
        const int g = r.below(9), sid = r.below(4) == 0 ? (int)r.below(20) : r.below(15), id = r.below(3) == 0 ? -1 : r.below(45), wl = r.below(5) == 0 ? (int)r.below(256) : (int)r.below(120);
        IC(return U(FM()->GetFurniture(g, sid, (int16)id, (uint8)wl));, return (uint64_t)ExeT<uint32_t>(0x5911E0, FM(), g, sid, id, wl););
    } ENDROW;
    ROW("FurnitureManager_c::GetRandomId", 0x591220, 500) {
        const int g = r.below(9), sid = r.below(4) == 0 ? (int)r.below(20) : r.below(15), wl = r.below(5) == 0 ? (int)r.below(256) : (int)r.below(120);
        IC(return (uint64_t)(uint32_t)FM()->GetRandomId(g, sid, (uint8)wl);, return (uint64_t)ExeT<uint32_t>(0x591220, FM(), g, sid, wl););
    } ENDROW;
    ROW("FurnitureManager_c::GetGroupId", 0x5BFB40, 600) {
        const std::string nm = PickName(r, kGroupNames, 6);
        Tw tw([nm](Rng&) { std::memcpy(S(oStr), nm.c_str(), nm.size() + 1); });
        IC(return (uint64_t)(uint32_t)FurnitureManager_c::GetGroupId(reinterpret_cast<const char*>(S(oStr)));, return (uint64_t)(uint32_t)oracle::Fn<int __stdcall(const char*)>(0x5BFB40)(reinterpret_cast<const char*>(S(oStr))););
    } ENDROW;
    ROW("FurnitureManager_c::GetSubGroupId", 0x5BFBF0, 1500) {
        const std::string nm = PickName(r, kSubNames, sizeof(kSubNames) / sizeof(*kSubNames));
        Tw tw([nm](Rng&) { std::memcpy(S(oStr), nm.c_str(), nm.size() + 1); });
        IC(return (uint64_t)(uint32_t)FurnitureManager_c::GetSubGroupId(reinterpret_cast<const char*>(S(oStr)));, return (uint64_t)(uint32_t)oracle::Fn<int __stdcall(const char*)>(0x5BFBF0)(reinterpret_cast<const char*>(S(oStr))););
    } ENDROW;
}
