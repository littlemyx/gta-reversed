// ORACLE-W3-REPLAY: exe-oracle rows for the A + B functions of game_sa/Replay.cpp (the replay buffer: record / playback packets, buffer bookkeeping, pool conversion tables, player info
// snapshot, clothes packets). Included by wave2_oracle_all.inc when W3_REPLAY_ONLY is defined (executable wave3_replay_oracle_test = the W2 harness: deterministic arena, the whole exe data
// image - which holds the 8 replay buffers of 100000 bytes at 0x97FB88, their status bytes, Record / Playback, the conversion tables, CReplay::PlayerInfo, the pool globals and CWorld::Players -
// plus the fixture blocks compared after every case).
// Fixtures: valid packet streams in all 8 buffers (every packet type 1..19 with its exe size, small pool references so that lookups hit, END terminator, garbage after the END and in buffers
// whose status is 0), the ped / vehicle pools (CPool headers at 0xB74490 / 0xB74494 over host storage with the exe strides 0x7C4 / 0xA18, random slot flags, random entity bytes), the
// conversion tables, Record at the buffer boundary, Mode, player info, clothes descriptions and packets.
#include "Replay.h"
#include "Pools/PedPool.h"
#include "Pools/VehiclePool.h"
#include "PedClothesDesc.h"
#include "World.h"
#include "Animation/AnimBlendAssociation.h"
#include "Animation/AnimBlendClumpData.h"
#include "Plugins/RpAnimBlendPlugin/RpAnimBlend.h"
#include "Cam.h"

struct W3R_RwObj { using type = RwObject* CEntity::*; friend type W2Get(W3R_RwObj); };
template struct W2Rob<W3R_RwObj, &CEntity::m_pRwObject>;

namespace w3r {
using namespace w2;
constexpr uintptr_t kBuf = 0x97FB88, kStat = 0x97FB7C, kRecord = 0x97FB70, kPlayback = 0x97FB64, kMode = 0xA43088, kPedConv = 0x97F838, kVehConv = 0x97F680;
constexpr size_t kBufSz = 100000, kPedStride = 0x7C4, kVehStride = 0xA18, kPedMax = 140, kVehMax = 110;
static_assert(sizeof(CCopPed) == kPedStride && sizeof(CHeli) == kVehStride, "the exe's pool strides");
alignas(16) static uint8_t g_pedStore[kPedMax * kPedStride];
alignas(16) static uint8_t g_vehStore[kVehMax * kVehStride];
static uint8_t g_pedFlags[kPedMax], g_vehFlags[kVehMax];
alignas(16) static uint8_t g_poolObj[2][256];
alignas(16) static uint8_t g_blob[2][512];       // generic in / out blocks (clothes desc + packet, focus position ...)
static int g_pedN = 0, g_vehN = 0;
static uint8_t* Buf(int slot) { return reinterpret_cast<uint8_t*>(kBuf + kBufSz * slot); }
static uint8_t* Stat() { return reinterpret_cast<uint8_t*>(kStat); }
static int8_t& Mode() { return StaticRef<int8_t>(kMode); }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
static int PoolMax = 14;                          // largest pool reference a packet may carry (the fixtures' lookups hit with a fair probability)

static const uint8_t kSize[20] = { 4, 52, 56, 8, 52, 88, 4, 8, 4, 8, 28, 20, 16, 4, 4, 56, 56, 60, 76, 120 };   // = the exe table 0x45C8A8 (verified by the FindSizeOfPacket row)

// one random packet of `type` at p (size kSize[type]): garbage payload, byte +1 (poolRef) and byte +3 (a ped update's vehicle index) small
static void Packet(Rng& r, uint8_t* p, int type) {
    for (int i = 1; i < kSize[type]; ++i) p[i] = (uint8_t)r.u32();
    p[0] = (uint8_t)type;
    p[1] = (uint8_t)r.below(PoolMax + 2);
    if (type == REPLAY_PACKET_PED_UPDATE) p[3] = (uint8_t)r.below(PoolMax + 3);
    if (type == REPLAY_PACKET_GENERAL) for (int k = 0; k < 3; ++k) { const float f = GenF(r, 4000.f, S_ZERO | S_NAN | S_INF | S_DEN | S_HUGE); std::memcpy(p + 0x4C + 4 * k, &f, 4); }
}
// a valid stream in one buffer: packets, END (0), garbage behind it. `bytes` = target length (0 = random)
static void Stream(Rng& r, int slot, size_t bytes = 0) {
    uint8_t* b = Buf(slot);
    if (!bytes) bytes = r.below(4) == 0 ? 0 : r.below(3) == 0 ? r.below(3000) : r.below(500);
    size_t off = 0;
    const int mix = (int)r.below(4);
    while (off < bytes && off + 120 < kBufSz - 8) {
        int t = (int)r.below(19) + 1;
        if (mix == 1 && r.below(2)) t = r.below(2) ? REPLAY_PACKET_END_OF_FRAME : REPLAY_PACKET_VEHICLE;
        if (mix == 2 && r.below(2)) t = r.below(2) ? REPLAY_PACKET_PED_HEADER : REPLAY_PACKET_PED_UPDATE;
        if (mix == 3 && r.below(3) == 0) t = REPLAY_PACKET_GENERAL;
        Packet(r, b + off, t); off += kSize[t];
    }
    b[off] = 0;
    if (r.below(2)) for (int i = 1; i < 40; ++i) b[off + i] = (uint8_t)r.u32();      // garbage behind the END packet (must not be read)
}
static void BuildBuffers(Rng& r, bool allActive = false) {
    for (int s = 0; s < 8; ++s) {
        Stat()[s] = allActive ? (uint8_t)(1 + r.below(2)) : (uint8_t)(r.below(3) == 0 ? 0 : 1 + r.below(2));
        Stream(r, s);
    }
    if (r.below(6) == 0) Stat()[0] = 0;
    PoolMax = r.below(3) ? 14 : 3 + (int)r.below(130);
}
static bool g_full = false;       // the exe's InitialiseXPoolConversionTable loop runs over the fixed 140 / 110 slots (the game's pool sizes): those rows use full-size pools
static void BuildPools(Rng& r) {
    g_pedN = (g_full || r.below(4) == 0) ? (int)kPedMax : 1 + (int)r.below(24);
    g_vehN = (g_full || r.below(4) == 0) ? (int)kVehMax : 1 + (int)r.below(24);
    FillGarbage(r, g_pedStore, (size_t)g_pedN * kPedStride);
    FillGarbage(r, g_vehStore, (size_t)g_vehN * kVehStride);
    const int usedPct = (int)r.below(101);
    for (int i = 0; i < g_pedN; ++i) g_pedFlags[i] = (uint8_t)((r.below(100) < (uint32_t)usedPct ? 0 : 0x80) | r.below(128));
    for (int i = 0; i < g_vehN; ++i) g_vehFlags[i] = (uint8_t)((r.below(100) < (uint32_t)usedPct ? 0 : 0x80) | r.below(128));
    new (g_poolObj[0]) CPedPool(g_pedN, g_pedStore, g_pedFlags);
    new (g_poolObj[1]) CVehiclePool(g_vehN, g_vehStore, g_vehFlags);
    for (int i = 0; i < g_pedN; ++i) g_pedFlags[i] = (uint8_t)((r.below(100) < (uint32_t)usedPct ? 0 : 0x80) | r.below(128));   // the ctor reset the flags
    for (int i = 0; i < g_vehN; ++i) g_vehFlags[i] = (uint8_t)((r.below(100) < (uint32_t)usedPct ? 0 : 0x80) | r.below(128));
    StaticRef<void*>(0xB74490) = g_poolObj[0]; StaticRef<void*>(0xB74494) = g_poolObj[1];
}
static void BuildRecord(Rng& r, bool nearEnd) {
    const uint32_t slot = r.below(8);
    uint32_t off = nearEnd ? 99984 - 70 + (uint32_t)r.below(90) : (uint32_t)r.below(100000 - 8);
    if (off > 99990) off = 99990;
    uint8_t* rec = reinterpret_cast<uint8_t*>(kRecord);
    FillGarbage(r, rec, 12);
    std::memcpy(rec + 0, &off, 4); const uint32_t base = (uint32_t)(kBuf + kBufSz * slot); std::memcpy(rec + 4, &base, 4); rec[8] = (uint8_t)slot;
    Mode() = r.below(5) ? 0 : (r.below(2) ? 1 : (int8_t)r.u32());
    if (Mode() == 0 && r.below(4)) Mode() = 0;
}
// the exe writes a deleted-vehicle / deleted-ped packet field by field and leaves its padding byte (+1) alone, the port writes the whole packet with a zero padding byte (never read): zero the
// destination so that the padding byte is equal on both sides (every other byte of the packet is compared)
static void ZeroDst() { const uint32_t off = StaticRef<uint32_t>(kRecord), base = StaticRef<uint32_t>(kRecord + 4); if (off < kBufSz - 16) std::memset(reinterpret_cast<void*>(base + off), 0, 16); }
static void BuildAll(Rng& r) {
    BuildBuffers(r); BuildPools(r); BuildRecord(r, false);
    Mode() = r.below(3) ? 0 : 1;
    FillGarbage(r, reinterpret_cast<void*>(kPedConv), 140 * 4); FillGarbage(r, reinterpret_cast<void*>(kVehConv), 110 * 4);
    FillGarbage(r, reinterpret_cast<void*>(kPlayback), 12);
    FillGarbage(r, g_blob, sizeof g_blob);
}
static void BuildActive(Rng& r) { g_full = true; BuildAll(r); g_full = false; for (int s = 0; s < 8; ++s) if (!Stat()[s] && r.below(2)) Stat()[s] = 1; }

template<class PF, class EF>
static bool VCase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildAll) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg pe{ (uintptr_t)g_pedStore, (size_t)g_pedN * kPedStride }, ve{ (uintptr_t)g_vehStore, (size_t)g_vehN * kVehStride };
    Reg pf_{ (uintptr_t)g_pedFlags, kPedMax }, vf_{ (uintptr_t)g_vehFlags, kVehMax }, po{ (uintptr_t)g_poolObj, sizeof g_poolObj }, bl{ (uintptr_t)g_blob, sizeof g_blob };
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { pe, ve, pf_, vf_, po, bl, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}

// ---- a ped with a fake clump (RpAnimBlend plugin slot at the exe's ClumpOffset 0xB5F878 -> CAnimBlendClumpData with 0..6 blend associations), a matrix, maybe a vehicle (pool slot) ----
alignas(16) static uint8_t g_pf[0x1400];
constexpr size_t kPfClump = 0x000, kPfCD = 0x100, kPfAs = 0x200, kPfAsSz = 0x100, kPfMat = 0x1000;
constexpr uint32_t kPfClumpOff = 0x40;
static void BuildPed(Rng& r) {
    BuildAll(r);
    static_assert(sizeof(CAnimBlendAssociation) <= kPfAsSz && sizeof(CMatrixLink) <= 0x100);
    FillGarbage(r, g_pf, sizeof g_pf);
    StaticRef<uint32_t>(0xB5F878) = kPfClumpOff;
    auto* cd = reinterpret_cast<CAnimBlendClumpData*>(g_pf + kPfCD);
    *reinterpret_cast<void**>(g_pf + kPfClump + kPfClumpOff) = cd;
    const int n = r.below(5) == 0 ? 0 : 1 + (int)r.below(6);
    static const float kBl[] = { 0.f, 0.25f, 0.5f, 0.5f, 1.f, 1.f, -0.5f, 0.75f };
    CAnimBlendLink* prev = &cd->m_AnimList; prev->next = nullptr;
    for (int i = 0; i < n; ++i) {
        auto* a = reinterpret_cast<CAnimBlendAssociation*>(g_pf + kPfAs + i * kPfAsSz);
        a->m_Link.prev = prev; a->m_Link.next = nullptr; prev->next = &a->m_Link; prev = &a->m_Link;
        a->m_BlendAmount = r.below(4) ? kBl[r.below(8)] : r.f01();
        const float tm = r.below(3) ? r.f01() * 4.f : GenF(r, 100.f, S_ZERO); a->m_CurrentTime = tm;
        const float sp = r.below(3) ? 0.5f + r.f01() : GenF(r, 100.f, S_ZERO); a->m_Speed = sp;
        { const int16_t id = (int16_t)r.below(r.below(8) ? 300 : 70000), gr = (int16_t)(r.below(3) ? r.below(120) : r.u32()); std::memcpy(&a->m_AnimId, &id, 2); std::memcpy(&a->m_AnimGroupId, &gr, 2); }
    }
    auto* ped = reinterpret_cast<CPed*>(g_pedStore);
    g_pf[kPfClump] = 2;   // rpCLUMP
    ped->*W2Get(W3R_RwObj{}) = reinterpret_cast<RwObject*>(g_pf + kPfClump);
    ped->m_matrix = reinterpret_cast<CMatrixLink*>(g_pf + kPfMat);
    ped->m_pVehicle = r.below(3) ? reinterpret_cast<CVehicle*>(g_vehStore + (size_t)r.below(g_vehN) * kVehStride) : nullptr;
    ped->bInVehicle = r.below(2);
    ped->m_fCurrentRotation = r.below(4) ? GenF(r, 7.f, S_ZERO) : GenF(r, 1e9f, S_ZERO | S_NAN | S_INF | S_DEN | S_HUGE);
    CWorld::PlayerInFocus = 0;
    CWorld::Players[0].m_pPed = r.below(2) ? reinterpret_cast<CPlayerPed*>(ped) : reinterpret_cast<CPlayerPed*>(g_pedStore + kPedStride);
    gbFirstPersonRunThisFrame = r.below(2);
    uint32_t off = StaticRef<uint32_t>(kRecord); if (off > 99900) { off = 99900; StaticRef<uint32_t>(kRecord) = off; }
}
} // namespace w3r

#define RROW(NAME, ADDR) Run(NAME " " #ADDR, [&](Rng& r, std::string& d) -> bool {
#define REND(CASES) }, std::max(10, (CASES) / w3r_div()));
static int w3r_div() { static const int v = std::getenv("W3R_DIV") ? std::max(1, std::atoi(std::getenv("W3R_DIV"))) : 1; return v; }

static void TestW3ReplayPure() {
    using namespace w2; using namespace w3r;
    RROW("CReplay::FindSizeOfPacket", 0x45C850)
        const uint32_t t = r.below(3) ? r.below(24) : (r.u32() & 0xFF);
        const bool ok = VCase(r, d, [&]() -> uint64_t { return CReplay::FindSizeOfPacket((eReplayPacket)t); }, [&]() -> uint64_t { return ExeC<uint32_t, uint32_t>(0x45C850, t); });
        if (!ok) { char b[96]; std::snprintf(b, sizeof b, " t=%u port=%u exe=%u", t, (unsigned)CReplay::FindSizeOfPacket((eReplayPacket)t), (unsigned)ExeC<uint32_t, uint32_t>(0x45C850, t)); d += b; }
        return ok;
    REND(600)
    RROW("CReplay::IsThisVehicleUsedInRecording", 0x45DE40)
        const int32_t idx = r.below(8) == 0 ? (int32_t)r.u32() : (int32_t)r.below(PoolMax + 3) - 1;
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)CReplay::IsThisVehicleUsedInRecording(idx); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, int32_t>(0x45DE40, idx); });
    REND(1500)
    RROW("CReplay::IsThisPedUsedInRecording", 0x45DDE0)
        const int32_t idx = r.below(8) == 0 ? (int32_t)r.u32() : (int32_t)r.below(PoolMax + 3) - 1;
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)CReplay::IsThisPedUsedInRecording(idx); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, int32_t>(0x45DDE0, idx); });
    REND(1500)
    RROW("CReplay::FindFirstFocusCoordinate", 0x45D6C0)
        auto* out = reinterpret_cast<CVector*>(g_blob[0]);
        return VCase(r, d, [&]() -> uint64_t { CReplay::FindFirstFocusCoordinate(*out); return 0; }, [&]() -> uint64_t { ExeC<void, CVector*>(0x45D6C0, out); return 0; });
    REND(1500)
    RROW("CReplay::NumberFramesAvailableToPlay", 0x45D670)
        // W3R_NOHANG (pre-fix measurement only): the unfixed port loops forever on an end-of-frame packet in an "active" buffer; such a case is reported as a mismatch instead of being run
        static bool hang = false; static const bool nohang = std::getenv("W3R_NOHANG") != nullptr;
        return VCase(r, d, [&]() -> uint64_t { return (nohang && hang) ? 0xDEADull : CReplay::NumberFramesAvailableToPlay(); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x45D670); },
            +[](Rng& rr) { BuildAll(rr); hang = false; if (Stat()[0]) for (int s = 0; s < 8; ++s) for (size_t o = 0; Buf(s)[o]; o += kSize[Buf(s)[o]]) if (Buf(s)[o] == 8) hang = true; });
    REND(1500)
    // clothes: a CPedClothesDesc and the 120-byte packet
    static const auto Floats = [](Rng& r, float* f, int n) { for (int i = 0; i < n; ++i) f[i] = r.below(8) == 0 ? GenF(r, 1e9f, S_ZERO | S_NAN | S_INF | S_DEN | S_HUGE) : GenF(r, 1000.f, S_ZERO); };
    RROW("CReplay::StoreClothesDesc", 0x45C750)
        auto* desc = reinterpret_cast<CPedClothesDesc*>(g_blob[0]); auto* pk = reinterpret_cast<tReplayClothesBlock*>(g_blob[1]);
        static_assert(sizeof(CPedClothesDesc) <= 256 && sizeof(tReplayClothesBlock) == 120);
        return VCase(r, d, [&]() -> uint64_t { CReplay::StoreClothesDesc(*desc, *pk); return 0; }, [&]() -> uint64_t { ExeC<void, const void*, void*>(0x45C750, desc, pk); return 0; },
            +[](Rng& rr) { BuildAll(rr); auto* ds = reinterpret_cast<CPedClothesDesc*>(g_blob[0]); Floats(rr, &ds->m_fFatStat, 1); Floats(rr, &ds->m_fMuscleStat, 1); });
    REND(1500)
    RROW("CReplay::RestoreClothesDesc", 0x45C7D0)
        auto* desc = reinterpret_cast<CPedClothesDesc*>(g_blob[0]); auto* pk = reinterpret_cast<tReplayClothesBlock*>(g_blob[1]);
        return VCase(r, d, [&]() -> uint64_t { CReplay::RestoreClothesDesc(*desc, *pk); return 0; }, [&]() -> uint64_t { ExeC<void, void*, void*>(0x45C7D0, desc, pk); return 0; },
            +[](Rng& rr) { BuildAll(rr); reinterpret_cast<tReplayClothesBlock*>(g_blob[1])->type = REPLAY_PACKET_CLOTHES; });
    REND(1500)
}

static void TestW3ReplayState() {
    using namespace w2; using namespace w3r;
    RROW("CReplay::EnableReplays", 0x45B160)
        StaticRef<uint8_t>(0x8A6160) = (uint8_t)r.u32();
        return VCase(r, d, [&]() -> uint64_t { CReplay::EnableReplays(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45B160); return 0; });
    REND(30)
    RROW("CReplay::DisableReplays", 0x45B150)
        StaticRef<uint8_t>(0x8A6160) = (uint8_t)r.u32();
        return VCase(r, d, [&]() -> uint64_t { CReplay::DisableReplays(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45B150); return 0; });
    REND(30)
    RROW("CReplay::ShouldStandardCameraBeProcessed", 0x45C440)
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)CReplay::ShouldStandardCameraBeProcessed(); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t>(0x45C440); },
            +[](Rng& rr) { BuildAll(rr); Mode() = rr.below(2) ? (int8_t)rr.below(3) : (int8_t)rr.u32(); });
    REND(100)
    // conversion table lookups
    static const auto Idx = [](Rng& r, int n) { return (int32_t)(r.below(n)); };
    RROW("CReplay::FindPoolIndexForPed", 0x45C450)
        const int32_t i = Idx(r, 140);
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CReplay::FindPoolIndexForPed(i); }, [&]() -> uint64_t { return ExeC<uint32_t, int32_t>(0x45C450, i); });
    REND(300)
    RROW("CReplay::FindPoolIndexForVehicle", 0x45C460)
        const int32_t i = Idx(r, 110);
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CReplay::FindPoolIndexForVehicle(i); }, [&]() -> uint64_t { return ExeC<uint32_t, int32_t>(0x45C460, i); });
    REND(300)
    RROW("CReplay::CanWeFindPoolIndexForPed", 0x45C470)
        const int32_t i = Idx(r, 140);
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)CReplay::CanWeFindPoolIndexForPed(i); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, int32_t>(0x45C470, i); });
    REND(300)
    RROW("CReplay::CanWeFindPoolIndexForVehicle", 0x45C490)
        const int32_t i = Idx(r, 110);
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)CReplay::CanWeFindPoolIndexForVehicle(i); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, int32_t>(0x45C490, i); });
    REND(300)
    // the player info snapshot (CWorld::Players[0] <-> CReplay::PlayerInfo, 0x190 bytes)
    static const auto Players = [](Rng& rr) { BuildAll(rr); FillGarbage(rr, &CWorld::Players[0], sizeof(CPlayerInfo)); FillGarbage(rr, &CReplay::PlayerInfo, sizeof(CPlayerInfo)); };
    RROW("CReplay::StorePlayerInfoVariables", 0x45F020)
        return VCase(r, d, [&]() -> uint64_t { CReplay::StorePlayerInfoVariables(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45F020); return 0; }, +Players);
    REND(150)
    RROW("CReplay::RestorePlayerInfoVariables", 0x45E1F0)
        return VCase(r, d, [&]() -> uint64_t { CReplay::RestorePlayerInfoVariables(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45E1F0); return 0; }, +Players);
    REND(150)
}

static void TestW3ReplayBuffers() {
    using namespace w2; using namespace w3r;
    RROW("CReplay::MarkEverythingAsNew", 0x45D430)
        return VCase(r, d, [&]() -> uint64_t { CReplay::MarkEverythingAsNew(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45D430); return 0; });
    REND(300)
    RROW("CReplay::EmptyReplayBuffer", 0x45EC90)
        return VCase(r, d, [&]() -> uint64_t { CReplay::EmptyReplayBuffer(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45EC90); return 0; });
    REND(300)
    RROW("CReplay::Init", 0x45E220)
        return VCase(r, d, [&]() -> uint64_t { CReplay::Init(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45E220); return 0; });
    REND(150)
    RROW("CReplay::GoToNextBlock", 0x45E2A0)
        return VCase(r, d, [&]() -> uint64_t { CReplay::GoToNextBlock(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45E2A0); return 0; });
    REND(600)
    RROW("CReplay::RecordVehicleDeleted", 0x45EBB0)
        const uint32_t pick = r.u32();
        const auto V = [pick]() { return reinterpret_cast<CVehicle*>(g_vehStore + (size_t)(pick % g_vehN) * kVehStride); };
        return VCase(r, d, [&]() -> uint64_t { CReplay::RecordVehicleDeleted(V()); return 0; }, [&]() -> uint64_t { ExeC<void, void*>(0x45EBB0, V()); return 0; },
            +[](Rng& rr) { BuildAll(rr); BuildRecord(rr, rr.below(2)); ZeroDst(); });
    REND(900)
    RROW("CReplay::RecordPedDeleted", 0x45EC20)
        const uint32_t pick = r.u32();
        const auto P = [pick]() { return reinterpret_cast<CPed*>(g_pedStore + (size_t)(pick % g_pedN) * kPedStride); };
        return VCase(r, d, [&]() -> uint64_t { CReplay::RecordPedDeleted(P()); return 0; }, [&]() -> uint64_t { ExeC<void, void*>(0x45EC20, P()); return 0; },
            +[](Rng& rr) { BuildAll(rr); BuildRecord(rr, rr.below(2)); ZeroDst(); });
    REND(900)
    RROW("CReplay::InitialiseVehiclePoolConversionTable", 0x45EFA0)
        return VCase(r, d, [&]() -> uint64_t { CReplay::InitialiseVehiclePoolConversionTable(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45EFA0); return 0; }, +BuildActive);
    REND(900)
    RROW("CReplay::InitialisePedPoolConversionTable", 0x45EF20)
        return VCase(r, d, [&]() -> uint64_t { CReplay::InitialisePedPoolConversionTable(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45EF20); return 0; }, +BuildActive);
    REND(900)
    RROW("CReplay::InitialisePoolConversionTables", 0x45F370)
        return VCase(r, d, [&]() -> uint64_t { CReplay::InitialisePoolConversionTables(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x45F370); return 0; }, +BuildActive);
    REND(300)
}


static void TestW3ReplayPed() {
    using namespace w2; using namespace w3r;
    RROW("CReplay::StorePedAnimation", 0x45B170)
        auto* ped = reinterpret_cast<CPed*>(g_pedStore); auto* st = reinterpret_cast<CStoredAnimationState*>(g_blob[0]);
        return VCase(r, d, [&]() -> uint64_t { CReplay::StorePedAnimation(ped, *st); return 0; }, [&]() -> uint64_t { ExeC<void, void*, void*>(0x45B170, ped, st); return 0; }, +BuildPed);
    REND(900)
    RROW("CReplay::StorePedUpdate", 0x45C940)
        auto* ped = reinterpret_cast<CPed*>(g_pedStore); const uint8_t idx = (uint8_t)(r.below(4) ? r.below(140) : r.u32());
        return VCase(r, d, [&]() -> uint64_t { CReplay::StorePedUpdate(ped, idx); return 0; }, [&]() -> uint64_t { ExeC<void, void*, uint32_t>(0x45C940, ped, idx); return 0; }, +BuildPed);
    REND(1200)
}

static void TestW3ReplayAll() {
    using namespace w3r;
    (void)spdlog::default_logger_raw();
    spdlog::set_level(spdlog::level::off);
    TestW3ReplayPure();
    TestW3ReplayState();
    TestW3ReplayBuffers();
    TestW3ReplayPed();
    StaticRef<void*>(0xB74490) = nullptr; StaticRef<void*>(0xB74494) = nullptr;
}
