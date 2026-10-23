// ORACLE-W3-AUDLOAD: exe-oracle rows of the uncovered class A functions of game_sa/Audio/Loaders (executable wave3_audload_oracle_test: W3_AUDIO_ONLY + W3_AUDLOAD, the loader TUs linked for real).
// None of these functions does file IO (the bank loader's requests are only queued, Service() reads them later - it is class D and not part of this target), so no IO stubs are needed: the
// in-memory "fake bank" is the fixture itself (loader object, 4 bank slots of 0x12D4 bytes, 64 bank lookups, a 32 KB buffer, all in the compared fixture g_af). Decoder objects live in the
// separate (not compared) buffer g_dec because the WMA decoder is 0x20038 bytes; their rows are pure getters, compared through the return value.
#include "Audio/Loaders/AEMP3BankLoader.h"
#include "Audio/Loaders/AEDataStream.h"
#include "Audio/Loaders/AEStreamTransformer.h"
#include "Audio/Loaders/AEMFDecoder.h"
#include "Audio/Loaders/AEWMADecoder.h"
#include "Audio/Loaders/AEWaveDecoder.h"
#include "Audio/Loaders/AEVorbisDecoder.h"
#include "Audio/Loaders/AEMP3TrackLoader.h"

namespace w3al {
using namespace w3au; using namespace w2;
constexpr size_t kBL = 0x1000, kSlots = 0x2000, kLk = 0x7000, kOut = 0x7800, kBuf = 0x8000, kBufSz = 0x8000, kXf = 0x11000, kTb = 0x11100, kDsF = 0x12000, kTl = 0x12100;
alignas(16) static uint8_t g_dec[0x20200];
constexpr size_t kDsD = 0x20040;   // CAEDataStream (0x28) inside g_dec
static CAEMP3BankLoader* BL() { return reinterpret_cast<CAEMP3BankLoader*>(P(kBL)); }
template<class T> static T& D(size_t off) { return *reinterpret_cast<T*>(g_dec + off); }
static int g_cnt, g_lk;
static void BankBuild(Rng& r) {
    BuildFx(r);
    g_cnt = r.below(12) == 0 ? (int)r.below(2) : 1 + (int)r.below(4);   // slots in use (the buffer holds 4)
    g_lk  = r.below(12) == 0 ? 1 : 1 + (int)r.below(64);                 // bank lookups
    At<uint32_t>(kBL + 0) = (uint32_t)(uintptr_t)P(kSlots); At<uint32_t>(kBL + 4) = (uint32_t)(uintptr_t)P(kLk);
    At<int16_t>(kBL + 0xC) = (int16_t)g_cnt; At<int16_t>(kBL + 0xE) = (int16_t)g_lk; At<uint8_t>(kBL + 0x14) = (uint8_t)(r.below(6) != 0);
    At<uint32_t>(kBL + 0x18) = (uint32_t)kBufSz; At<uint32_t>(kBL + 0x1C) = (uint32_t)(uintptr_t)P(kBuf);
    uint32_t off = 0;
    for (int s = 0; s < 4; ++s) {
        const size_t b = kSlots + (size_t)s * 0x12D4;
        const uint32_t nb = 0x1000 + r.below(0x800);
        At<uint32_t>(b) = off; At<uint32_t>(b + 4) = nb; off += nb;
        At<int16_t>(b + 0x10) = (int16_t)(r.below(4) == 0 ? -1 : r.below(5) == 0 ? (int)g_lk : (int)r.below((uint32_t)g_lk));
        At<int16_t>(b + 0x12) = (int16_t)(r.below(3) == 0 ? -1 : 1 + (int)r.below(60));
        uint32_t run = 0;
        for (int k = 0; k < 400; ++k) {
            const size_t e = b + 0x14 + (size_t)k * 12;
            At<uint32_t>(e) = run; run += r.below(nb / 400 + 1);
            At<uint32_t>(e + 4) = r.below(3) == 0 ? 0xFFFFFFFFu : r.below(1000);
            At<uint16_t>(e + 8) = (uint16_t)(r.below(10) == 0 ? r.u32() : 8000 + r.below(40000));
            At<int16_t>(e + 10) = (int16_t)(r.below(3) ? (int)r.below(2500) - 500 : (int)(r.u32() & 0xFFFF));
        }
    }
    for (int k = 0; k < 64; ++k) { const size_t e = kLk + (size_t)k * 12; At<uint8_t>(e) = (uint8_t)r.u32(); At<uint32_t>(e + 4) = r.below(3) ? r.below(1u << 20) : r.u32(); At<uint32_t>(e + 8) = r.below(3) ? r.below(0x1800) : r.u32(); }
    for (int i = 0; i < 50; ++i) {   // requests: the dead fields (stream buffer pointers, BankNumBytes of single-sound requests) are zero like in the exe's idle queue
        const size_t e = kBL + 0x24 + (size_t)i * 32;
        At<uint32_t>(e + 0) = r.below(2) ? (uint32_t)(uintptr_t)P(kSlots + (size_t)r.below(4) * 0x12D4) : r.u32();
        At<uint32_t>(e + 4) = r.u32(); At<uint32_t>(e + 8) = 0; At<uint32_t>(e + 0xC) = 0; At<uint32_t>(e + 0x10) = 0;
        At<uint32_t>(e + 0x14) = r.below(4);
        At<int16_t>(e + 0x18) = (int16_t)(r.below(3) == 0 ? -1 : (int)r.below((uint32_t)g_lk + 1));
        At<int16_t>(e + 0x1A) = (int16_t)(r.below(3) == 0 ? -1 : (int)r.below(5));
        At<int16_t>(e + 0x1C) = (int16_t)(r.below(3) == 0 ? -1 : (int)r.below(6));
        At<uint8_t>(e + 0x1E) = (uint8_t)r.u32();
    }
    At<uint16_t>(kBL + 0x664) = (uint16_t)r.below(50); At<uint16_t>(kBL + 0x666) = (uint16_t)r.below(51); At<uint16_t>(kBL + 0x668) = (uint16_t)(r.below(6) == 0 ? 49 : r.below(50)); At<uint16_t>(kBL + 0x66A) = 4;
    for (int s = 0; s < 60; ++s) At<int16_t>(kBL + 0x66C + (size_t)s * 2) = (int16_t)(r.below(3) == 0 ? -1 : r.below(2) ? (int)r.below(6) : (int)r.below(400));
}
static void NormPad() { for (int i = 0; i < 50; ++i) At<uint8_t>(kBL + 0x24 + (size_t)i * 32 + 0x1F) = 0; }   // CAESoundRequest trailing pad byte: the port's struct assignment writes it (debug fill 0xCC), the exe leaves it; never read
static int16_t BankArg(Rng& r, int slot, bool allowUnk) {   // bank argument: mostly the bank in the slot (to hit the "loaded" paths), else any valid one, rarely UNK / above the table
    const int16_t sb = At<int16_t>(kSlots + (size_t)slot * 0x12D4 + 0x10);
    const uint32_t k = r.below(10);
    if (k < 4) return sb;
    if (k < 8 || !allowUnk) return (int16_t)r.below((uint32_t)g_lk);
    return (int16_t)(k == 8 ? -1 : g_lk + 1 + (int)r.below(5));
}
static int16_t SoundArg(Rng& r, int slot) {
    const uint32_t k = r.below(10);
    if (k < 4) return At<int16_t>(kBL + 0x66C + (size_t)slot * 2);
    if (k < 8) return (int16_t)r.below(8);
    return (int16_t)r.below(400);
}
static void DecBuild(Rng& r) {
    BuildFx(r); FillGarbage(r, g_dec, sizeof g_dec);
    D<uint32_t>(kDsD + 0x1C) = r.u32();   // CAEDataStream::m_nTrackId
}
} // namespace w3al

static void TestW3AudLoad() {
    using namespace w2; using namespace w3au; using namespace w3al;
    spdlog::set_level(spdlog::level::off);   // the port logs warnings the exe does not have; formatting them allocates from the compared arena
    // --- CAEMP3BankLoader ---------------------------------------------------------------------------------------------------------------------------------------------------------
    Run("CAEMP3BankLoader::GetBankSlotBuffer 0x4DFDE0", [&](Rng& r, std::string& d) {
        const int16_t slot = (int16_t)(r.below(8) == 0 ? (int)r.below(9) - 2 : (int)r.below(4));
        return ACase(r, d, [&]() -> uint64_t { return Cv(BL()->GetBankSlotBuffer((eSoundBankSlot)slot, At<uint32_t>(kOut))); }, [&]() -> uint64_t { return Cv(ExeT<uint8_t*>(0x4DFDE0, BL(), (int)slot, P(kOut))); }, &BankBuild);
    }, 1200);
    Run("CAEMP3BankLoader::GetSoundHeadroom 0x4E01E0", [&](Rng& r, std::string& d) {
        const int16_t slot = (int16_t)r.below(4), snd = (int16_t)r.below(400);
        return ACase(r, d, [&]() -> uint64_t { return Cv(BL()->GetSoundHeadroom((eSoundID)snd, (eSoundBankSlot)slot)); }, [&]() -> uint64_t { return Cv(ExeT<float>(0x4E01E0, BL(), (int)snd, (int)slot)); }, &BankBuild);
    }, 1200);
    Run("CAEMP3BankLoader::IsSoundBankLoaded 0x4E0220", [&](Rng& r, std::string& d) {
        const int16_t slot = (int16_t)r.below(4), bank = BankArg(r, slot, true);
        return ACase(r, d, [&]() -> uint64_t { return Cv(BL()->IsSoundBankLoaded((eSoundBank)bank, (eSoundBankSlot)slot)); }, [&]() -> uint64_t { return Cv((bool)(ExeT<uint32_t>(0x4E0220, BL(), (int)bank, (int)slot) & 0xFF)); }, &BankBuild);
    }, 1200);
    Run("CAEMP3BankLoader::GetSoundBankLoadingStatus 0x4E0250", [&](Rng& r, std::string& d) {
        const int16_t slot = (int16_t)r.below(4), bank = BankArg(r, slot, true);
        return ACase(r, d, [&]() -> uint64_t { return Cv(BL()->GetSoundBankLoadingStatus((eSoundBank)bank, (eSoundBankSlot)slot)); }, [&]() -> uint64_t { return Cv((bool)(ExeT<uint32_t>(0x4E0250, BL(), (int)bank, (int)slot) & 0xFF)); }, &BankBuild);
    }, 1200);
    Run("CAEMP3BankLoader::IsSoundLoaded 0x4E03B0", [&](Rng& r, std::string& d) {
        const int16_t slot = (int16_t)r.below(4), bank = BankArg(r, slot, true), snd = SoundArg(r, slot);
        return ACase(r, d, [&]() -> uint64_t { return Cv(BL()->IsSoundLoaded((eSoundBank)bank, (eSoundID)snd, (eSoundBankSlot)slot)); }, [&]() -> uint64_t { return Cv((bool)(ExeT<uint32_t>(0x4E03B0, BL(), (int)bank, (int)snd, (int)slot) & 0xFF)); }, &BankBuild);
    }, 1500);
    Run("CAEMP3BankLoader::GetSoundLoadingStatus 0x4E0400", [&](Rng& r, std::string& d) {
        const int16_t slot = (int16_t)r.below(4), bank = BankArg(r, slot, true), snd = SoundArg(r, slot);
        return ACase(r, d, [&]() -> uint64_t { return Cv(BL()->GetSoundLoadingStatus((eSoundBank)bank, (eSoundID)snd, (eSoundBankSlot)slot)); }, [&]() -> uint64_t { return Cv((bool)(ExeT<uint32_t>(0x4E0400, BL(), (int)bank, (int)snd, (int)slot) & 0xFF)); }, &BankBuild);
    }, 1500);
    Run("CAEMP3BankLoader::LoadSoundBank 0x4E0670", [&](Rng& r, std::string& d) {
        const int16_t slot = (int16_t)(r.below(10) == 0 ? (int)r.below(8) - 2 : (int)r.below(4)), bank = (int16_t)(r.below(10) == 0 ? -1 - (int)r.below(3) : (int)r.below(64));
        return ACase(r, d, [&]() -> uint64_t { if (bank < g_lk) BL()->LoadSoundBank((eSoundBank)bank, (eSoundBankSlot)slot); NormPad(); return 0; }, [&]() -> uint64_t { if (bank < g_lk) ExeT<void>(0x4E0670, BL(), (int)bank, (int)slot); NormPad(); return 0; }, &BankBuild);
    }, 3000);
    Run("CAEMP3BankLoader::LoadSound 0x4E07A0", [&](Rng& r, std::string& d) {
        const int16_t slot = (int16_t)(r.below(10) == 0 ? (int)r.below(8) - 2 : (int)r.below(4)), snd = (int16_t)(r.below(8) == 0 ? (int)r.below(100) + 395 - (int)r.below(3) * 400 : (int)r.below(8));
        return ACase(r, d, [&]() -> uint64_t { const int16_t bank = (int16_t)(g_lk > 1 ? (int)(slot & 7) % g_lk : 0); BL()->LoadSound((eSoundBank)bank, (eSoundID)snd, (eSoundBankSlot)slot); NormPad(); return 0; },
                          [&]() -> uint64_t { const int16_t bank = (int16_t)(g_lk > 1 ? (int)(slot & 7) % g_lk : 0); ExeT<void>(0x4E07A0, BL(), (int)bank, (int)snd, (int)slot); NormPad(); return 0; }, &BankBuild);
    }, 3000);
    Run("CAEBankLoader::GetBankLookup 0x4E01B0", [&](Rng& r, std::string& d) {   // the exe returns null for an uninitialised loader / bank >= count, the port asserts: valid calls only
        const int16_t bank = (int16_t)r.below(64);
        return ACase(r, d, [&]() -> uint64_t { return g_lk > bank ? Cv(&static_cast<CAEBankLoader*>(BL())->GetBankLookup((uint16)bank)) : 0; }, [&]() -> uint64_t { return g_lk > bank ? Cv(ExeT<void*>(0x4E01B0, BL(), (int)bank)) : 0; },
                     [](Rng& rr) { BankBuild(rr); At<uint8_t>(kBL + 0x14) = 1; });
    }, 300);
    Run("CAEBankLoader::CalculateBankSlotsInfosOffsets 0x4DFBA0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { static_cast<CAEBankLoader*>(BL())->CalculateBankSlotsInfosOffsets(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4DFBA0, BL()); return 0; }, &BankBuild);
    }, 200);
    // --- CAEStreamTransformer / CAEDataStream ---------------------------------------------------------------------------------------------------------------------------------
    Run("CAEStreamTransformer::Initialise 0x4F1750", [&](Rng& r, std::string& d) {
        auto* x = reinterpret_cast<CAEStreamTransformer*>(P(kXf));
        return ACase(r, d, [&]() -> uint64_t { x->Initialise(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F1750, x); return 0; });
    }, 20);
    Run("CAEStreamTransformer::TransformBuffer 0x4F17D0", [&](Rng& r, std::string& d) {
        auto* x = reinterpret_cast<CAEStreamTransformer*>(P(kXf));
        const uint32_t n = r.below(5) == 0 ? 0 : r.below(300), pos = r.below(3) ? r.below(1u << 24) : r.u32(), off = r.below(8);
        return ACase(r, d, [&]() -> uint64_t { x->TransformBuffer(P(kTb + off), n, pos); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F17D0, x, (void*)P(kTb + off), (size_t)n, pos); return 0; },
                     [](Rng& rr) { BuildFx(rr); const uint8_t t[16] = { 0xEA, 0x3A, 0xC4, 0xA1, 0x9A, 0xA8, 0x14, 0xF3, 0x48, 0xB0, 0xD7, 0x23, 0x9D, 0xE8, 0xFF, 0xF1 }; std::memcpy(P(kXf), t, 16); });
    }, 600);
    Run("CAEDataStream::Constructor 0x4DC620", [&](Rng& r, std::string& d) {   // vptr (port vtable vs exe 0x85F0BC) and m_pFileHandle (the port sets nullptr, the exe leaves it; every reader tests m_bIsOpen first) zeroed after the call on both sides
        const int32_t id = (int32_t)r.u32(); char* fn = (char*)(uintptr_t)(r.u32() & ~3u); const uint32_t st = r.u32(), len = r.u32(); const bool enc = r.below(2);
        return ACase(r, d, [&]() -> uint64_t { new (P(kDsF)) CAEDataStream(id, fn, st, len, enc); At<uint32_t>(kDsF) = 0; At<uint32_t>(kDsF + 4) = 0; return 0; },
                          [&]() -> uint64_t { oracle::Fn<void* __thiscall(void*, int, char*, uint32_t, uint32_t, int)>(0x4DC620)(P(kDsF), id, fn, st, len, (int)enc); At<uint32_t>(kDsF) = 0; At<uint32_t>(kDsF + 4) = 0; return 0; });
    }, 300);
    {
        auto ds = [] { return reinterpret_cast<CAEDataStream*>(P(kDsF)); };
        ULARGE_INTEGER ul{}; ul.QuadPart = 0x123456789ULL;
        Run("CAEDataStream::Write 0x4DC4D0", [&](Rng& r, std::string& d) { const uint32_t n = r.u32(); return ACase(r, d, [&]() -> uint64_t { return Cv(ds()->CAEDataStream::Write(P(kOut), n, nullptr)); }, [&]() -> uint64_t { return Cv(oracle::Fn<HRESULT __stdcall(void*, const void*, ULONG, ULONG*)>(0x4DC4D0)(ds(), P(kOut), n, nullptr)); }); }, 5);
        Run("CAEDataStream::SetSize 0x4DC4E0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(ds()->CAEDataStream::SetSize(ul)); }, [&]() -> uint64_t { return Cv(oracle::Fn<HRESULT __stdcall(void*, ULARGE_INTEGER)>(0x4DC4E0)(ds(), ul)); }); }, 5);
        Run("CAEDataStream::CopyTo 0x4DC4F0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(ds()->CAEDataStream::CopyTo(nullptr, ul, nullptr, nullptr)); }, [&]() -> uint64_t { return Cv(oracle::Fn<HRESULT __stdcall(void*, IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*)>(0x4DC4F0)(ds(), nullptr, ul, nullptr, nullptr)); }); }, 5);
        Run("CAEDataStream::Commit 0x4DC500", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(ds()->CAEDataStream::Commit(0)); }, [&]() -> uint64_t { return Cv(oracle::Fn<HRESULT __stdcall(void*, DWORD)>(0x4DC500)(ds(), 0)); }); }, 5);
        Run("CAEDataStream::Revert 0x4DC510", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(ds()->CAEDataStream::Revert()); }, [&]() -> uint64_t { return Cv(oracle::Fn<HRESULT __stdcall(void*)>(0x4DC510)(ds())); }); }, 5);
        Run("CAEDataStream::LockRegion 0x4DC520", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(ds()->CAEDataStream::LockRegion(ul, ul, 1)); }, [&]() -> uint64_t { return Cv(oracle::Fn<HRESULT __stdcall(void*, ULARGE_INTEGER, ULARGE_INTEGER, DWORD)>(0x4DC520)(ds(), ul, ul, 1)); }); }, 5);
        Run("CAEDataStream::UnlockRegion 0x4DC530", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(ds()->CAEDataStream::UnlockRegion(ul, ul, 1)); }, [&]() -> uint64_t { return Cv(oracle::Fn<HRESULT __stdcall(void*, ULARGE_INTEGER, ULARGE_INTEGER, DWORD)>(0x4DC530)(ds(), ul, ul, 1)); }); }, 5);
        Run("CAEDataStream::Clone 0x4DC540", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(ds()->CAEDataStream::Clone(nullptr)); }, [&]() -> uint64_t { return Cv(oracle::Fn<HRESULT __stdcall(void*, IStream**)>(0x4DC540)(ds(), nullptr)); }); }, 5);
    }
    // --- CAEMP3TrackLoader ctor ---------------------------------------------------------------------------------------------------------------------------------------------------
    Run("CAEMP3TrackLoader::Constructor 0x4E0930", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { new (P(kTl)) CAEMP3TrackLoader(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4E0930, P(kTl)); return 0; });
    }, 20);
    // --- decoders: getters on garbage objects (layouts validated by VALIDATE_SIZE; the data stream the decoder points to holds the track id) ---------------------------------------------
#define DECBUILD(INIT, DSOFF, DSCOPY) [](Rng& rr) { DecBuild(rr); D<uint8_t>(INIT) = (uint8_t)(rr.below(4) != 0); if ((DSOFF) != 0) D<uint32_t>(DSOFF) = (uint32_t)(uintptr_t)(g_dec + kDsD); if ((DSCOPY) != 0) D<uint32_t>(DSCOPY) = (uint32_t)(uintptr_t)(g_dec + kDsD); }
#define DECROW(NAME, ADDR, CASES, TYPE, METH, BUILD, RET) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { auto* o = reinterpret_cast<TYPE*>(g_dec); return ACase(r, d, [&]() -> uint64_t { return Cv(o->TYPE::METH()); }, [&]() -> uint64_t { return Cv(ExeT<RET>(ADDR, o)); }, BUILD); }, CASES)
    DECROW("CAEMFDecoder::GetStreamLengthMs", 0x4E7920, 60, CAEMFDecoder, GetStreamLengthMs, DECBUILD(0xC, 0, 0), int32_t);
    DECROW("CAEMFDecoder::GetSampleRate", 0x4E78C0, 60, CAEMFDecoder, GetSampleRate, DECBUILD(0xC, 0, 0), int32_t);
    DECROW("CAEMFDecoder::GetStreamID", 0x4E77B0, 60, CAEMFDecoder, GetStreamID, DECBUILD(0xC, 0, 0), int32_t);
    DECROW("CAEVorbisDecoder::GetStreamID", 0x502460, 60, CAEVorbisDecoder, GetStreamID, DECBUILD(0x1C, 4, 0x20), int32_t);
    DECROW("CAEWMADecoder::GetStreamLengthMs", 0x502AD0, 60, CAEWMADecoder, GetStreamLengthMs, DECBUILD(8, 0, 0), int32_t);
    DECROW("CAEWMADecoder::GetSampleRate", 0x502AB0, 60, CAEWMADecoder, GetSampleRate, DECBUILD(8, 0, 0), int32_t);
    DECROW("CAEWMADecoder::GetStreamID", 0x502750, 60, CAEWMADecoder, GetStreamID, DECBUILD(8, 4, 0), int32_t);
    DECROW("CAEWaveDecoder::GetSampleRate", 0x503300, 60, CAEWaveDecoder, GetSampleRate, DECBUILD(8, 0, 0), int32_t);
    DECROW("CAEWaveDecoder::GetStreamID", 0x503280, 60, CAEWaveDecoder, GetStreamID, DECBUILD(8, 4, 0xC), int32_t);
    Run("CAEVorbisDecoder::GetSampleRate 0x502560", [&](Rng& r, std::string& d) {
        auto* o = reinterpret_cast<CAEVorbisDecoder*>(g_dec);
        return ACase(r, d, [&]() -> uint64_t { return Cv(o->CAEVorbisDecoder::GetSampleRate()); }, [&]() -> uint64_t { return Cv(ExeT<int32_t>(0x502560, o)); },
                     [](Rng& rr) { DecBuild(rr); D<uint8_t>(0x1C) = (uint8_t)(rr.below(4) != 0); D<uint32_t>(0x24) = (uint32_t)(uintptr_t)(g_dec + 0x20100); D<int32_t>(0x20100 + 8) = (int32_t)(rr.below(3) ? 8000 + rr.below(48000) : rr.u32()); });
    }, 80);
    Run("CAEWaveDecoder::GetStreamLengthMs 0x503310", [&](Rng& r, std::string& d) {
        auto* o = reinterpret_cast<CAEWaveDecoder*>(g_dec);
        return ACase(r, d, [&]() -> uint64_t { return Cv(o->CAEWaveDecoder::GetStreamLengthMs()); }, [&]() -> uint64_t { return Cv(ExeT<int32_t>(0x503310, o)); },
                     [](Rng& rr) {
                         DecBuild(rr); D<uint8_t>(8) = (uint8_t)(rr.below(5) != 0);
                         static const uint32_t rates[] = { 8000, 11025, 22050, 32000, 44100, 48000 }; static const uint16_t ba[] = { 1, 2, 4, 4, 4, 8, 3, 4 };
                         D<uint32_t>(0x28) = rr.below(8) ? rates[rr.below(6)] : rr.u32(); D<uint16_t>(0x30) = ba[rr.below(8)];
                         D<uint32_t>(0x38) = rr.below(3) ? rr.below(1u << 26) : rr.u32();
                     });
    }, 1500);
    Run("CAEWMADecoder::GetStreamPlayTimeMs 0x502AF0", [&](Rng& r, std::string& d) {
        auto* o = reinterpret_cast<CAEWMADecoder*>(g_dec);
        return ACase(r, d, [&]() -> uint64_t { return Cv(o->CAEWMADecoder::GetStreamPlayTimeMs()); }, [&]() -> uint64_t { return Cv(ExeT<int32_t>(0x502AF0, o)); },
                     [](Rng& rr) {
                         DecBuild(rr); D<uint8_t>(8) = (uint8_t)(rr.below(5) != 0);
                         const uint32_t k = rr.below(4);
                         D<uint64_t>(0x10) = k == 0 ? ((uint64_t)rr.u32() << 32 | rr.u32()) : k == 1 ? (uint64_t)rr.u32() * rr.below(300) : (uint64_t)rr.below(1u << 30) * 1000 + rr.below(1000);
                         D<uint64_t>(0x18) = k == 0 ? ((uint64_t)rr.u32() << 32 | rr.u32()) : (uint64_t)rr.below(1u << 28);
                     });
    }, 3000);
    Run("CAEMP3BankLoader::GetSoundBuffer 0x4E0280", [&](Rng& r, std::string& d) {
        const int16_t slot = (int16_t)r.below(4);
        const uint32_t k = r.below(10);
        const int16_t ns = At<int16_t>(kSlots + (size_t)slot * 0x12D4 + 0x12);
        const int16_t snd = (int16_t)(k < 5 ? At<int16_t>(kBL + 0x66C + (size_t)slot * 2) : k < 9 ? (ns > 0 ? (int)r.below((uint32_t)ns + 2) : (int)r.below(6)) : (int)r.below(400));
        return ACase(r, d, [&]() -> uint64_t { return Cv(BL()->GetSoundBuffer((eSoundID)snd, (eSoundBankSlot)slot, At<uint32_t>(kOut), At<uint16_t>(kOut + 8))); },
                          [&]() -> uint64_t { return Cv(ExeT<uint8_t*>(0x4E0280, BL(), (int)snd, (int)slot, P(kOut), P(kOut + 8))); }, &BankBuild);
    }, 2500);
#undef DECROW
#undef DECBUILD
}
