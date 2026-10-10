// (a) audio math: CAEAudioUtility, CAEVehicleAudioEntity pure helpers (fake audio entity + fake vehicle), see world_oracle_test.cpp
#include "Audio/AEAudioUtility.h"
#include "Audio/Entities/AEVehicleAudioEntity.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Physical.h"

// zeroed-then-randomised raw object of a game class (never constructed): the real layout, plain bytes
template<class T> struct Raw {
    alignas(16) uint8 b[(sizeof(T) + 15) & ~15] {};
    T& operator*() { return *reinterpret_cast<T*>(b); }
    T* operator->() { return reinterpret_cast<T*>(b); }
    T* get() { return reinterpret_cast<T*>(b); }
    void Fill(Rng& r) { for (auto& x : b) x = (uint8)r.u32(); }
};

static bool CmpF(std::string& d, const char* what, float got, float exe) {
    if (SameF(got, exe)) return true;
    d += std::string(what) + " got " + F(got) + " exe " + F(exe) + "; ";
    return false;
}
static std::string HexVA(unsigned va) { char b[16]; std::snprintf(b, 16, "%X", va); return b; }

// fake CAEVehicleAudioEntity with a fake vehicle
struct AudFx {
    Raw<CAEVehicleAudioEntity> ae;
    Raw<CVehicle>              veh;
    AudFx(Rng& r) {
        ae.Fill(r); veh.Fill(r);
        ae->m_Entity = veh.get();
        veh->m_nModelIndex = r.below(2) ? 457 /*MODEL_CADDY*/ : (uint16)(400 + r.below(200));
        veh->vehicleFlags.bIsDrowning = r.below(2);
        veh->m_pVehicleBeingTowed = r.below(2) ? veh.get() : nullptr;
        ae->m_State = r.below(2) ? CAEVehicleAudioEntity::eAEState::DUMMY_CRZ : (CAEVehicleAudioEntity::eAEState)r.below(5);
        ae->m_AuSettings.EngineVolumeOffset = r.below(4) ? (r.below(2) ? 0.f : GenF(r, 20.f)) : GenF(r, 30.f);
        ae->m_IsSingleGear = r.below(2);
        ae->m_AuGear = r.below(6);
        ae->m_CrzCount = r.below(3) ? r.f01() * 100.f : GenF(r, 500.f);
        ae->m_bNitroOnLastFrame = r.below(2);
        ae->m_CurrentNitroRatio = r.below(3) ? r.f01() : GenF(r, 1.5f);
    }
};

static void TestAudio() {
    Run("CAEAudioUtility::AudioLog10 0x4D9E50", [](Rng& r, std::string& d) {
        const float p = r.below(3) ? r.f01() * (r.below(2) ? 1.f : 1e-4f) : GenF(r, PickScale(r));
        const float a = CAEAudioUtility::AudioLog10(p);
        const float e = oracle::Fn<float __cdecl(float)>(0x4D9E50)(p);
        if (CmpF(d, "log10", a, e)) return true;
        d += "p " + F(p); return false;
    });
    Run("CAEAudioUtility::GetPiecewiseLinear 0x4D9D90", [](Rng& r, std::string& d) {
        float tab[24][2];
        const int n = 1 + r.below(8);
        float x0 = GenF(r, 1.f, S_NOHUGE);
        for (int i = 0; i < 24; ++i) {
            x0 += r.below(8) ? r.f01() * 0.5f : GenF(r, 2.f, S_NAN | S_ZERO | S_DEN | S_TINY);
            tab[i][0] = x0; tab[i][1] = GenF(r, 5.f);
        }
        const float x = r.below(4) ? x0 * r.f01() * 0.8f + tab[0][0] * 0.1f : GenF(r, 4.f);
        const float a = CAEAudioUtility::GetPiecewiseLinear(x, (int16)n, tab);
        const float e = oracle::Fn<float __cdecl(float, int16, const float(*)[2])>(0x4D9D90)(x, (int16)n, tab);
        if (CmpF(d, "pl", a, e)) return true;
        d += "x " + F(x) + " n " + std::to_string(n); for (int i = 0; i < n; ++i) d += " {" + F(tab[i][0]) + "," + F(tab[i][1]) + "}"; return false;
    });
    Run("CAEAudioUtility::ConvertFromBytesToMS 0x4D9EF0", [](Rng& r, std::string& d) {
        const uint32 len = r.below(3) ? r.u32() : r.u32() & 0xFFFFFF, rate = r.below(3) ? 8000 + r.u32() % 48000 : r.u32();
        const uint16 ch = (uint16)(r.below(4) ? 1 + r.below(2) : r.u32() & 0xFFFF);
        if (!(rate * ch)) return true;   // fdiv by 0 -> inf -> floor -> int conversion: uninteresting / exe UB
        const uint32 a = CAEAudioUtility::ConvertFromBytesToMS(len, rate, ch);
        const uint32 e = oracle::Fn<uint32 __cdecl(uint32, uint32, uint16)>(0x4D9EF0)(len, rate, ch);
        if (a == e) return true;
        d = "len " + std::to_string(len) + " rate " + std::to_string(rate) + " ch " + std::to_string(ch) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    });
    Run("CAEAudioUtility::ConvertFromMSToBytes 0x4D9F40", [](Rng& r, std::string& d) {
        const uint32 ms = r.below(3) ? r.u32() % 100000 : r.u32(), fr = r.below(3) ? 8000 + r.u32() % 48000 : r.u32();
        const uint16 mult = (uint16)(r.below(4) ? 1 + r.below(2) : 1 + (r.u32() & 0x7FFF));
        const uint32 a = CAEAudioUtility::ConvertFromMSToBytes(ms, fr, mult);
        const uint32 e = oracle::Fn<uint32 __cdecl(uint32, uint32, uint16)>(0x4D9F40)(ms, fr, mult);
        if (a == e) return true;
        d = "ms " + std::to_string(ms) + " fr " + std::to_string(fr) + " mult " + std::to_string(mult) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    });
    Run("CAEVehicleAudioEntity::GetFlyingMetalVolume 0x4F6150", [](Rng& r, std::string& d) {
        AudFx f(r); Raw<CPhysical> ph; ph.Fill(r);
        ph->m_vecTurnSpeed = GenV(r, r.below(2) ? 1.f : PickScale(r));
        const float a = f.ae->GetFlyingMetalVolume(ph.get());
        const float e = oracle::Fn<float __fastcall(void*, int, void*)>(0x4F6150)(f.ae.get(), 0, ph.get());
        if (CmpF(d, "vol", a, e)) return true;
        d += "ts " + V(ph->m_vecTurnSpeed); return false;
    });
    struct GI { const char* n; unsigned va; };
    for (const auto& g : { GI{"GetVolumeForDummyIdle", 0x4F51F0}, GI{"GetFrequencyForDummyIdle", 0x4F5310}, GI{"GetVolumeForDummyRev", 0x4F53D0}, GI{"GetFrequencyForDummyRev", 0x4F54F0} }) {
        Run(std::string("CAEVehicleAudioEntity::") + g.n + " 0x" + HexVA(g.va), [=](Rng& r, std::string& d) {
            AudFx f(r);
            const float ratio = r.below(4) ? r.f01() * 1.3f : GenF(r, 3.f), fade = r.below(4) ? r.f01() * 1.1f : GenF(r, 3.f);
            float a = 0.f;
            switch (g.va) {
            case 0x4F51F0: a = f.ae->GetVolumeForDummyIdle(ratio, fade); break;
            case 0x4F5310: a = f.ae->GetFrequencyForDummyIdle(ratio, fade); break;
            case 0x4F53D0: a = f.ae->GetVolumeForDummyRev(ratio, fade); break;
            default:       a = f.ae->GetFrequencyForDummyRev(ratio, fade); break;
            }
            const float e = oracle::Fn<float __fastcall(void*, int, float, float)>(g.va)(f.ae.get(), 0, ratio, fade);
            if (CmpF(d, "v", a, e)) return true;
            d += "ratio " + F(ratio) + " fade " + F(fade) + " state " + std::to_string((int)f.ae->m_State); return false;
        });
    }
    Run("CAEVehicleAudioEntity::GetFreqForIdle 0x4F5C60", [](Rng& r, std::string& d) {
        AudFx f(r); const float x = r.below(4) ? r.f01() * 1.2f : GenF(r, 3.f);
        const float a = f.ae->GetFreqForIdle(x);
        const float e = oracle::Fn<float __fastcall(void*, int, float)>(0x4F5C60)(f.ae.get(), 0, x);
        if (CmpF(d, "f", a, e)) return true;
        d += "x " + F(x); return false;
    });
    Run("CAEVehicleAudioEntity::GetBaseVolumeForBicycleTyre 0x4F60B0", [](Rng& r, std::string& d) {
        AudFx f(r); const float x = r.below(4) ? r.f01() * 1.2f : GenF(r, 3.f);
        const float a = f.ae->GetBaseVolumeForBicycleTyre(x);
        const float e = oracle::Fn<float __fastcall(void*, int, float)>(0x4F60B0)(f.ae.get(), 0, x);
        if (CmpF(d, "v", a, e)) return true;
        d += "x " + F(x); return false;
    });
}
