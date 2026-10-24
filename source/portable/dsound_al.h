#pragma once
// MAC-W3: test / diagnostics surface of the OpenAL-backed DirectSound device of the portable build (portable/dsound_al.cpp). The game itself only sees DirectSoundCreate(8)
// of winshim/dsound.h; this header lets a host test create a device with an explicit output (ALC_SOFT_loopback, no sound card) and drive it deterministically.
#ifdef NOTSA_PORTABLE
#include <cstddef>
#include <cstdint>

#include "winshim/dsound.h"

namespace notsa::portable::dsal {
enum class Output {
    Auto,     // environment (NOTSA_AUDIO=device|loopback|null), default: the default OpenAL device, falling back to Null when it cannot be opened
    Device,   // the default OpenAL playback device
    Loopback, // ALC_SOFT_loopback device (nothing is audible): the mixed signal goes through an OpenAL source and is read back with alcRenderSamplesSOFT
    Null,     // no OpenAL: the mixer runs on the steady clock and its output is discarded (silent)
};
struct Options {
    Output      output{Output::Auto};
    std::uint32_t rate{48000};    // mix rate for Loopback / Null (Device: the rate the device reports)
    bool        manual{false};    // no pump thread: the caller drives the device with Render() (tests; Loopback and Null only)
    const char* wavPath{nullptr}; // also append everything that was output (post OpenAL for Loopback) to this 16-bit stereo WAV file
};

// the frames the mixer produces per OpenAL buffer (and the granularity of the play cursors)
inline constexpr std::size_t kChunkFrames = 512;

HRESULT Create(const Options& opt, IDirectSound8** out);

// ---- manual-mode drivers (devices created with Options::manual = true) ----
// Mixes `frames` frames of interleaved signed 16-bit stereo into `out` (Loopback: through the OpenAL source and alcRenderSamplesSOFT). Returns the frames written.
std::size_t Render(IDirectSound8* dev, std::int16_t* out, std::size_t frames);

// ---- introspection ----
bool          UsesOpenAL(IDirectSound8* dev);
const char*   OutputName(IDirectSound8* dev); // "device" | "loopback" | "null"
std::uint32_t MixRate(IDirectSound8* dev);
} // namespace notsa::portable::dsal
#endif
