// MAC-W3: audio diagnostics for the native Mac build, reachable through the run-build input injector (NOTSA_STANDALONE_INPUT="...;cmd:radio:3;..."), called from InjRunCmd
// (app/platform/SDLWrapper.cpp, NOTSA_PORTABLE only). Portable-only file; runs on the main thread like every injector command.
//   cmd:radio:ID        AudioEngine.StartRadio(ID, NORMAL)   (the radio stream path: CAERadioTrackManager -> track loader -> OGG/WAV decoder -> CAEStreamingChannel)
//   cmd:radiooff        AudioEngine.StopRadio(nullptr, false)
//   cmd:audiostat       logs the hardware channels and the streaming channel state
#include "StdInc.h"

#include <string>

#include "AudioEngine.h"
#include "Hardware/AEAudioHardware.h"
#include "Hardware/AEStreamingChannel.h"

namespace notsa::portable {
bool AudioCmd(const char* name, const std::string& args) {
    if (!std::strcmp(name, "radio")) {
        const int id = std::atoi(args.c_str());
        NOTSA_LOG_INFO("[audio-cmd] StartRadio({})", id);
        AudioEngine.StartRadio((eRadioID)id, eBassSetting::NORMAL);
        return true;
    }
    if (!std::strcmp(name, "radiooff")) {
        NOTSA_LOG_INFO("[audio-cmd] StopRadio");
        AudioEngine.StopRadio(nullptr, false);
        return true;
    }
    if (!std::strcmp(name, "audiostat")) {
        int used = 0, playing = 0;
        for (int i = 0; i < AEAudioHardware.m_nNumChannels; ++i) {
            const auto* ch = AEAudioHardware.m_aChannels[i];
            if (ch && ch->m_pDirectSoundBuffer) { ++used; }
            if (ch && ch->IsBufferPlaying()) { ++playing; }
        }
        const auto* sc = AEAudioHardware.m_pStreamingChannel;
        NOTSA_LOG_INFO("[audio-cmd] hardware initialised={} channels={} with a buffer={} playing={} streaming state={} radio on={}", AEAudioHardware.m_bInitialised,
                       AEAudioHardware.m_nNumChannels, used, playing, sc ? (int)sc->m_nState : -1, CAudioEngine::IsRadioOn());
        return true;
    }
    return false;
}
} // namespace notsa::portable
