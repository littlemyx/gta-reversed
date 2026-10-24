// MAC-W0a: portable replacement of app/platform/win/VideoPlayer/VideoPlayer.cpp (DirectShow graph) and
// app/platform/win/VideoModeSelectDialog.cpp (Win32 dialog resource); both are excluded from the portable target. Portable-only file.
#include "StdInc.h"

#include "VideoPlayer.h"
#include "VideoModeSelectDialog.h"
#include "app_win32_compat.h"

namespace VideoPlayer {
// W-A1: no movie playback (DirectShow in the original). Play() reports once and returns; the game then sits in GAME_STATE_PLAYING_LOGO / _INTRO
// until the usual skip input (WinMain.cpp), exactly as the original does when a movie cannot be played. Later slice: an SDL3 + libav/AVFoundation player.
void InjectHooks() {}
void Shutdown() {}
void WaitState(int32) {}
void OnGraphNotify() {}
void UpdateWindow() {}
void Release() {}
void Play(int32, const char* path) {
    notsa::portable::Unsupported("VideoPlayer::Play (DirectShow); movies are skipped");
    NOTSA_LOG_WARN("VideoPlayer::Play('{}'): not implemented on this platform, movie skipped", path ? path : "");
}
auto GetMediaControl() -> IMediaControl* { return nullptr; }
} // namespace VideoPlayer

// W-A2: no video mode / adapter selection dialog (Win32 dialog resource IDD_DIALOG1). psSelectDevice does not reach it in the standalone-run
// configuration (NOTSA_STANDALONE_RUN keeps the default sub system and searches the mode); if it is ever reached: report failure ("user cancelled").
LRESULT CreateVidModeSelectDialog(HINSTANCE, HWND) {
    notsa::portable::Unsupported("CreateVidModeSelectDialog (Win32 dialog)");
    return FALSE;
}
