#pragma once

#include "dsound.h"
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <standalone/Fixups.h>

//! NOTSA (run build only, env `NOTSA_STANDALONE_MUTE=1`): silent DirectSound output WITHOUT touching the audio logic.
//!
//! The game's own code reads the volume back (`CAESmoothFadeThread::RequestFade/Service` call `IDirectSoundBuffer::GetVolume` and branch on it),
//! so clamping the values at the call sites would change the fade logic. Instead every secondary buffer the hardware layer creates is attached
//! to a patched COM vtable (a copy of the buffer's own vtable, only `SetVolume` / `GetVolume` replaced): `SetVolume(v)` records the LOGICAL
//! volume and sets DSBVOLUME_MIN on the real buffer; `GetVolume` returns the logical one. Everything else (Play/Stop/Lock/frequency/3D/status,
//! fades, streaming, position handling) runs unchanged on the real Wine buffer. The menu volume settings can not undo it: nothing but this
//! patched `SetVolume` ever reaches the real buffer's volume.
//! Without the env var, or in a build without NOTSA_STANDALONE_RUN, `NOTSA_AUDIO_MUTE_ATTACH` does nothing.
#ifdef NOTSA_STANDALONE_RUN
namespace notsa::audio_mute {
namespace detail {
// IDirectSoundBuffer8 vtable slots: IUnknown(3) + GetCaps GetCurrentPosition GetFormat [GetVolume=6] GetPan GetFrequency GetStatus Initialize Lock Play
// SetCurrentPosition SetFormat [SetVolume=15] SetPan SetFrequency Stop Unlock Restore + SetFX AcquireResources GetObjectInPath = 24
constexpr size_t SLOT_GETVOLUME = 6, SLOT_SETVOLUME = 15, NUM_SLOTS = 24;

using SetVolumeFn = HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, LONG);

struct State {
    std::mutex                          Lock;
    std::unordered_map<void*, LONG>     Logical;     // buffer -> volume the game asked for (hundredths of dB)
    std::vector<void**>                 Tables;      // patched vtable copies (kept alive for the process lifetime)
    void*                               OrigVtbl{};  // the vtable we copied from
    void**                              Patched{};
    SetVolumeFn                         RealSetVolume{};
    unsigned                            NumAttached{}, NumSetVolume{}, NumAudible{};
    LONG                                MaxLogical{DSBVOLUME_MIN}; // loudest volume the game asked for (logged in +5 dB steps; shows what the mute swallowed)
};
inline State& S() { static State s; return s; }

inline HRESULT STDMETHODCALLTYPE MutedSetVolume(IDirectSoundBuffer* self, LONG vol) {
    auto& s = S();
    if (vol > DSBVOLUME_MAX || vol < DSBVOLUME_MIN) {
        return DSERR_INVALIDPARAM; // same range check as dsound
    }
    unsigned n, a;
    LONG mx = 0; bool louder = false;
    {
        std::lock_guard g{s.Lock};
        s.Logical[self] = vol;
        n = ++s.NumSetVolume;
        if (vol > -3000) { ++s.NumAudible; } // would have been clearly audible (louder than -30 dB)
        if (vol >= s.MaxLogical + 500 || (vol > s.MaxLogical && vol >= -300)) { s.MaxLogical = vol; mx = vol; louder = true; } // log each +5 dB step of the loudest request, and anything above -3 dB once
        a = s.NumAudible;
    }
    if (n == 1 || n == 1000 || n == 100000 || n == 1000000) {
        notsa::standalone::Fixups::Log("[mute] %u SetVolume calls swallowed so far (%u of them louder than -30 dB); real buffer volume is DSBVOLUME_MIN", n, a);
    }
    if (louder) {
        notsa::standalone::Fixups::Log("[mute] new loudest requested volume %d (hundredths of dB) swallowed; real buffer volume stays DSBVOLUME_MIN", (int)mx);
    }
    return s.RealSetVolume(self, DSBVOLUME_MIN); // the real buffer stays at -100 dB
}

inline HRESULT STDMETHODCALLTYPE MutedGetVolume(IDirectSoundBuffer* self, LPLONG out) {
    if (!out) {
        return DSERR_INVALIDPARAM;
    }
    auto& s = S();
    std::lock_guard g{s.Lock};
    const auto it = s.Logical.find(self);
    *out = it != s.Logical.end() ? it->second : DSBVOLUME_MAX; // a fresh buffer is at full volume
    return DS_OK;
}
} // namespace detail

inline bool Enabled() {
    static const bool v = [] {
        const char* e = std::getenv("NOTSA_STANDALONE_MUTE");
        return e && *e && *e != '0';
    }();
    return v;
}

inline void Attach(IDirectSoundBuffer* buffer) {
    using namespace detail;
    if (!buffer || !Enabled()) {
        return;
    }
    auto& s = S();
    void** vtbl = *reinterpret_cast<void***>(buffer);
    {
        std::lock_guard g{s.Lock};
        if (s.OrigVtbl != vtbl) {
            if (s.OrigVtbl) {
                notsa::standalone::Fixups::Log("[mute] WARNING: second distinct buffer vtable %p (first %p), copying again", vtbl, s.OrigVtbl);
            }
            auto* copy = new void*[NUM_SLOTS];
            for (size_t i = 0; i < NUM_SLOTS; i++) {
                copy[i] = vtbl[i];
            }
            s.RealSetVolume = reinterpret_cast<SetVolumeFn>(vtbl[SLOT_SETVOLUME]);
            copy[SLOT_SETVOLUME] = reinterpret_cast<void*>(&MutedSetVolume);
            copy[SLOT_GETVOLUME] = reinterpret_cast<void*>(&MutedGetVolume);
            s.Tables.push_back(copy);
            s.OrigVtbl = vtbl;
            s.Patched  = copy;
        }
        *reinterpret_cast<void***>(buffer) = s.Patched;
        s.Logical[buffer] = DSBVOLUME_MAX; // a recycled address starts fresh
        ++s.NumAttached;
        if (s.NumAttached == 1) {
            notsa::standalone::Fixups::Log("[mute] NOTSA_STANDALONE_MUTE: DirectSound buffers are attached to a muted vtable (real volume DSBVOLUME_MIN, logical volume kept for GetVolume)");
        }
    }
    const auto hr = s.RealSetVolume(buffer, DSBVOLUME_MIN);
    if (FAILED(hr)) {
        notsa::standalone::Fixups::Log("[mute] WARNING: real SetVolume(MIN) failed with 0x%08X on buffer %p (no DSBCAPS_CTRLVOLUME?)", (unsigned)hr, (void*)buffer);
    }
}
}
#define NOTSA_AUDIO_MUTE_ATTACH(buf) ::notsa::audio_mute::Attach(buf)
#else
#define NOTSA_AUDIO_MUTE_ATTACH(buf) ((void)0)
#endif
