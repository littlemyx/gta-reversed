#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

//! NOTSA (run build only, env `NOTSA_AUDIO_STATS=<file>`): SILENT mixer statistics, to compare the standalone's sound manager / hardware layer with the exe's without listening.
//! Counters are bumped at the points where the audio pipeline can silently refuse work; `notsa::audio_stats::Service()` (called at the end of `CAESoundManager::Service`) prints one
//! line per `NOTSA_AUDIO_STATS_EVERY` frames (default 60) and, when something is suspicious (sound table nearly full, refused requests, old sounds), a breakdown of the active sounds.
//! The exe's equivalent can be sampled from outside with tools/standalone/audio_stats_exe.py (same struct layout: `CAESoundManager` at 0xB62CB0).
#ifdef NOTSA_STANDALONE_RUN
namespace notsa::audio_stats {
struct Counters {
    std::atomic<uint32_t> reqOk{}, reqRefused{};            //!< CAESoundManager::RequestNewSound: all requests / those that found the 300-sound table full
    std::atomic<uint32_t> hwPlayCalls{}, hwBadChannel{}, hwNoBuffer{}; //!< CAEAudioHardware::PlaySound called / channel index rejected / the bank loader had no buffer for (sound, slot)
    std::atomic<uint32_t> hwSetBufFail{}, hwCreateFail{}, hwLockFail{}; //!< SetAudioBuffer returned false / CreateSoundBuffer failed / Lock failed
    std::atomic<uint32_t> hwPlayOk{};                       //!< CAEAudioHardware::PlaySound reached ch->Play
    std::atomic<uint32_t> dsPlayFail{}, dsSetVolFail{}, dsSetFreqFail{}; //!< DirectSound calls that returned an error (needs NOTSA_AUDIO_TAP too)
    std::atomic<uint32_t> collFull{}, collAdd{};                       //!< CAECollisionAudioEntity::AddCollisionSoundToList: list full
    std::atomic<int32_t>  createHr{}, lockHr{}, playHr{}, setVolHr{}; //!< last failing HRESULTs
};
inline Counters& C() { static Counters c; return c; }

struct State {
    std::mutex M;
    FILE*      F{};
    bool       On{};
    uint32_t   Every{ 60 };
};
inline State& S() { static State s; return s; }

inline bool Enabled() {
    static const bool v = [] {
        const char* e = std::getenv("NOTSA_AUDIO_STATS");
        if (!e || !*e) return false;
        auto& s = S();
        s.F = std::fopen(e, "wb");
        if (!s.F) return false;
        if (const char* n = std::getenv("NOTSA_AUDIO_STATS_EVERY"); n && *n) s.Every = (uint32_t)std::max(1, std::atoi(n));
        s.On = true;
        return true;
    }();
    return v;
}

template<typename... A>
inline void Log(const char* fmt, A... a) {
    auto& s = S();
    if (!s.On) return;
    std::lock_guard g{ s.M };
    std::fprintf(s.F, fmt, a...);
    std::fputc('\n', s.F);
    std::fflush(s.F);
}

void Service(); // AESoundManager.cpp
} // namespace notsa::audio_stats
#define NOTSA_AUDIO_STAT(field) do { if (::notsa::audio_stats::Enabled()) { ++::notsa::audio_stats::C().field; } } while (0)
#define NOTSA_AUDIO_STAT_HR(field, hrfield, hr) do { if (::notsa::audio_stats::Enabled()) { ++::notsa::audio_stats::C().field; ::notsa::audio_stats::C().hrfield = (int32_t)(hr); } } while (0)
#else
#define NOTSA_AUDIO_STAT(field) ((void)0)
#define NOTSA_AUDIO_STAT_HR(field, hrfield, hr) ((void)0)
#endif
