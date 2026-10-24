// ORACLE-C-MODELS part 5 (included by wave3_oracle_models.inc): class-C rows that need REAL vftables on the model infos.
// A garbage vptr makes every virtual call fault (the case carries no information), so, like W3FuncCase of the vehicle tests, the EXE's vftable (RH_ScopedVirtualClass VA) is written into word 0 of
// the info objects before the exe call and the PORT's vftable (the compiler's ??_7Cls@@6B@ through /alternatename) before the port call; the words are cleared after each call, so the compare
// of the fixture memory and the data image ignores them. Virtual callees then run their real code on each side.
#define W3M_PORT_VT(C) extern "C" const uint32_t w3mvt_##C[]; __pragma(comment(linker, "/alternatename:_w3mvt_" #C "=??_7" #C "@@6B@"))
W3M_PORT_VT(CAtomicModelInfo) W3M_PORT_VT(CDamageAtomicModelInfo) W3M_PORT_VT(CLodAtomicModelInfo) W3M_PORT_VT(CTimeModelInfo) W3M_PORT_VT(CLodTimeModelInfo)
W3M_PORT_VT(CWeaponModelInfo) W3M_PORT_VT(CClumpModelInfo) W3M_PORT_VT(CVehicleModelInfo) W3M_PORT_VT(CPedModelInfo) W3M_PORT_VT(CBaseModelInfo)
namespace w3m {
struct MVt { const char* name; uint32_t exeVt; const uint32_t* portVt; };
static const MVt kMVt[] = {
    { "CAtomicModelInfo", 0x85BBF0, w3mvt_CAtomicModelInfo }, { "CDamageAtomicModelInfo", 0x85BC30, w3mvt_CDamageAtomicModelInfo }, { "CLodAtomicModelInfo", 0x85BC70, w3mvt_CLodAtomicModelInfo },
    { "CTimeModelInfo", 0x85BCB0, w3mvt_CTimeModelInfo }, { "CLodTimeModelInfo", 0x85BCF0, w3mvt_CLodTimeModelInfo }, { "CWeaponModelInfo", 0x85BD78, w3mvt_CWeaponModelInfo },
    { "CClumpModelInfo", 0x85BD30, w3mvt_CClumpModelInfo }, { "CVehicleModelInfo", 0x85C5C8, w3mvt_CVehicleModelInfo }, { "CPedModelInfo", 0x85BDC0, w3mvt_CPedModelInfo },
    { "CBaseModelInfo", 0x85BB9C, w3mvt_CBaseModelInfo },
};
enum { kVtAtomic, kVtDamage, kVtLod, kVtTime, kVtLodTime, kVtWeapon, kVtClump, kVtVehicle, kVtPed, kVtBase };
static void PutVt(void* obj, int cls, int side /*1 port, 2 exe, 0 clear*/) {
    *reinterpret_cast<uint32_t*>(obj) = side == 1 ? (uint32_t)(uintptr_t)kMVt[cls].portVt : side == 2 ? kMVt[cls].exeVt : 0u;
}

// ---- CModelInfo::Is*Model: ms_modelInfoPtrs[0..15] = 16 info objects of random classes (a share of null entries), vptrs injected per side
constexpr size_t kMis = 0x4000, kMisStride = 0x400, kNumMis = 16;
static int g_misCls[kNumMis];
static void BuildMis(Rng& r) {
    BuildMi(r);
    for (size_t i = 0; i < kNumMis; ++i) {
        g_misCls[i] = r.below(10) < 6 ? (int)kVtVehicle : (int)r.below(9);   // not CBaseModelInfo: its GetModelType is _purecall (abort) on both sides
        uint8_t* o = P(kMis + i * kMisStride);
        // vehicle type field of the vehicle infos: game values mixed with garbage
        const uint32_t vt = r.below(8) == 0 ? r.u32() : r.below(12);
        *reinterpret_cast<int32_t*>(o + offsetof(CVehicleModelInfo, m_nVehicleType)) = (int32_t)vt;
        StaticRef<void*>(kModelPtrs + i * 4) = r.below(5) == 0 ? nullptr : o;
    }
}
static void BuildMisCol(Rng& r) {
    BuildMis(r);
    for (size_t i = 0; i < kNumMis; ++i) StaticRef<void*>(kModelPtrs + i * 4) = P(kMis + i * kMisStride);   // every entry present (the exe dereferences the "other" model)
    g_misCls[0] = (int)r.below(9);
    const int16_t other = r.below(4) == 0 ? (int16_t)-1 : (int16_t)r.below(kNumMis);
    uint8_t* o = P(kMis);
    *reinterpret_cast<int16_t*>(o + offsetof(CTimeModelInfo, m_timeInfo) + offsetof(CTimeInfo, m_nOtherTimeModel)) = other;
    *reinterpret_cast<int16_t*>(o + offsetof(CLodTimeModelInfo, m_lodTimeInfo) + offsetof(CTimeInfo, m_nOtherTimeModel)) = other;
}
static void MisVt(int side) { for (size_t i = 0; i < kNumMis; ++i) PutVt(P(kMis + i * kMisStride), g_misCls[i], side); }

// ---- CModelInfo::Add*Model: the store's next item gets the class vptr (the exe's virtual Init is slot 6), the store count is a small value
struct AddStore { uint32_t countVa; size_t itemSize; int cls; };
static bool AddCase(Rng& r, std::string& d, const AddStore& st, const std::function<void*(int32_t)>& portFn, uint32_t exeAddr, int32_t index) {
    return MCase(r, d,
        [&]() -> uint64_t {
            void* item = reinterpret_cast<uint8_t*>(st.countVa + 4) + (size_t)StaticRef<uint32_t>(st.countVa) * st.itemSize;
            PutVt(item, st.cls, 1); const uint64_t v = U(portFn(index)); PutVt(item, st.cls, 0); return v; },
        [&]() -> uint64_t {
            void* item = reinterpret_cast<uint8_t*>(st.countVa + 4) + (size_t)StaticRef<uint32_t>(st.countVa) * st.itemSize;
            PutVt(item, st.cls, 2); const uint64_t v = (uint32_t)ExeC<uint32_t>(exeAddr, index); PutVt(item, st.cls, 0); return v; },
        [](Rng& rr) { BuildMi(rr); });
}
} // namespace w3m

static void TestW3MVtInfra();
static void TestW3ModelsPart5() {
    TestW3MVtInfra();
    using namespace w2; using namespace w3m;
#define ISM(NAME, ADDR, EXPR) \
    Run("CModelInfo::" NAME " " #ADDR, [&](Rng& r, std::string& d) { \
        const int32_t idx = (int32_t)r.below(kNumMis); \
        return MCase(r, d, [&]() -> uint64_t { MisVt(1); const uint64_t v = (EXPR(idx)) ? 1 : 0; MisVt(0); return v; }, \
                           [&]() -> uint64_t { MisVt(2); const uint64_t v = ExeC<uint8_t>(ADDR, idx) != 0 ? 1 : 0; MisVt(0); return v; }, &BuildMis); }, 400)
    ISM("IsBoatModel", 0x4C5A70, CModelInfo::IsBoatModel);
    ISM("IsTrainModel", 0x4C5AD0, CModelInfo::IsTrainModel);
    ISM("IsHeliModel", 0x4C5B00, CModelInfo::IsHeliModel);
    ISM("IsPlaneModel", 0x4C5B30, CModelInfo::IsPlaneModel);
    ISM("IsBikeModel", 0x4C5B60, CModelInfo::IsBikeModel);
    ISM("IsFakePlaneModel", 0x4C5B90, CModelInfo::IsFakePlaneModel);
    ISM("IsMonsterTruckModel", 0x4C5BC0, CModelInfo::IsMonsterTruckModel);
    ISM("IsQuadBikeModel", 0x4C5BF0, CModelInfo::IsQuadBikeModel);
    ISM("IsBmxModel", 0x4C5C20, CModelInfo::IsBmxModel);
    ISM("IsTrailerModel", 0x4C5C50, CModelInfo::IsTrailerModel);
#undef ISM
    Run("CBaseModelInfo::SetColModel 0x4C4BC0", [&](Rng& r, std::string& d) {
        auto* col = reinterpret_cast<CColModel*>((uintptr_t)(r.u32() | 1));   // never dereferenced
        const bool lod = r.below(3) != 0;
        return MCase(r, d, [&]() -> uint64_t { MisVt(1); reinterpret_cast<CBaseModelInfo*>(P(kMis))->SetColModel(col, lod); MisVt(0); return 0; },
                           [&]() -> uint64_t { MisVt(2); ExeT<void>(0x4C4BC0, P(kMis), col, (uint32_t)lod); MisVt(0); return 0; }, &BuildMisCol);
    }, 600);
    Run("CModelInfo::IsVehicleModelType 0x4C5C80", [&](Rng& r, std::string& d) {
        const int32_t idx = r.below(6) == 0 ? (int32_t)(19990 + r.below(30)) : (int32_t)r.below(kNumMis);   // 20000 and up: -1 before any lookup
        return MCase(r, d, [&]() -> uint64_t { MisVt(1); const uint64_t v = (uint32_t)CModelInfo::IsVehicleModelType(idx); MisVt(0); return v; },
                           [&]() -> uint64_t { MisVt(2); const uint64_t v = ExeC<uint32_t>(0x4C5C80, idx); MisVt(0); return v; }, &BuildMis);
    }, 400);
    // Add*Model: exe stores at CStore {count, items[]}: Atomic 0xAAE950 (0x20), DamageAtomic 0xB1BF58, LodAtomic 0xB1C934, Time 0xB1C960, LodTime 0xB1E128, Weapon 0xB1E158, Clump 0xB1E958, Vehicle 0xB1F650 (0x308), Ped 0xB478F8
#define ADDM(NAME, ADDR, STORE, CLS, KIND, STOREVA) \
    Run("CModelInfo::" #NAME " " #ADDR, [&](Rng& r, std::string& d) { \
        const AddStore st{ STOREVA, sizeof(KIND), CLS }; \
        g_tweak = [&](Rng& rr) { StaticRef<uint32_t>(STOREVA) = rr.below(3); }; \
        const bool ok = AddCase(r, d, st, [&](int32_t i) -> void* { return CModelInfo::NAME(i); }, ADDR, (int32_t)r.below(20000)); g_tweak = nullptr; return ok; }, 200)
    ADDM(AddAtomicModel, 0x4C6620, ms_atomicModelInfoStore, kVtAtomic, CAtomicModelInfo, 0xAAE950);
    ADDM(AddDamageAtomicModel, 0x4C6650, ms_damageAtomicModelInfoStore, kVtDamage, CDamageAtomicModelInfo, 0xB1BF58);
    ADDM(AddLodAtomicModel, 0x4C6680, ms_lodAtomicModelInfoStore, kVtLod, CLodAtomicModelInfo, 0xB1C934);
    ADDM(AddTimeModel, 0x4C66B0, ms_timeModelInfoStore, kVtTime, CTimeModelInfo, 0xB1C960);
    ADDM(AddLodTimeModel, 0x4C66E0, ms_lodTimeModelInfoStore, kVtLodTime, CLodTimeModelInfo, 0xB1E128);
    ADDM(AddWeaponModel, 0x4C6710, ms_weaponModelInfoStore, kVtWeapon, CWeaponModelInfo, 0xB1E158);
    ADDM(AddClumpModel, 0x4C6740, ms_clumpModelInfoStore, kVtClump, CClumpModelInfo, 0xB1E958);
    ADDM(AddVehicleModel, 0x4C6770, ms_vehicleModelInfoStore, kVtVehicle, CVehicleModelInfo, 0xB1F650);
    ADDM(AddPedModel, 0x4C67A0, ms_pedModelInfoStore, kVtPed, CPedModelInfo, 0xB478F8);
#undef ADDM
}
void W3M_ForceVftables(void* buf);
static void TestW3MVtInfra() {
    { static volatile bool never = false; if (never) W3M_ForceVftables(nullptr); }
    using namespace w3m;
    for (const auto& c : kMVt) {
        std::string un; for (int i = 0; i < 17; ++i) if (c.portVt[i] == 0x400000 || c.portVt[i] < 0x401000) un += " " + std::to_string(i);
        std::printf("w3m vt: %-24s port vftable %p slot4 %08X, exe slot4 %08X, bad slots:%s\n", c.name, (const void*)c.portVt, c.portVt[4], *reinterpret_cast<const uint32_t*>(c.exeVt + 16), un.empty() ? " none" : un.c_str());
    }
}
// the compiler emits a class's vftable only where one of its constructors is used: construct (never executed) one of each so that ??_7Cls@@6B@ exists in this test
__declspec(noinline) void W3M_ForceVftables(void* buf) {
    new (buf) CDamageAtomicModelInfo(); new (buf) CTimeModelInfo(); new (buf) CLodTimeModelInfo(); new (buf) CWeaponModelInfo(); new (buf) CPedModelInfo();
    new (buf) CAtomicModelInfo(); new (buf) CLodAtomicModelInfo(); new (buf) CClumpModelInfo(); new (buf) CVehicleModelInfo();
}
