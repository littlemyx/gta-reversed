#include "StdInc.h"

#ifdef NOTSA_USE_SDL3
#include <SDL3/SDL.h>
#include "SDLWrapper.hpp"
#include <bindings/imgui_impl_sdl3.h>
#include <WindowedMode.hpp>
#include "PostEffects.h"
#include "UIRenderer.h"
#ifdef NOTSA_STANDALONE_RUN
#include "standalone/Fixups.h"
#include <algorithm>
#endif

static bool IsInFullscreen()
{
    // TODO: Currently with our hacky windowed mode, looks like there's no
    // good way to gather this information.
    return false;
}


#ifdef NOTSA_STANDALONE_RUN
// Test-only input injector: NOTSA_STANDALONE_INPUT="wait:3000;key:return;wait:500;down:w;wait:2000;up:w;..." (also a path to a file with the same text).
// Times are ms of wall clock; `wait:N` advances the script clock by N after the previous action; `key:K` = down now + up 150 ms later;
// K = return|escape|up|down|left|right|space|tab|lshift|lctrl|f1..f12|<single char>.
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
namespace {
struct InjEv { uint64_t t; bool down; SDL_Keycode key; };
std::vector<InjEv> s_InjEvents;
size_t             s_InjNext = 0;
bool               s_InjInit = false;
uint64_t           s_InjStart = 0;

SDL_Keycode InjKey(const std::string& n) {
    static const std::pair<const char*, SDL_Keycode> k[] = { { "return", SDLK_RETURN }, { "escape", SDLK_ESCAPE }, { "up", SDLK_UP }, { "down", SDLK_DOWN },
        { "left", SDLK_LEFT }, { "right", SDLK_RIGHT }, { "space", SDLK_SPACE }, { "tab", SDLK_TAB }, { "lshift", SDLK_LSHIFT }, { "lctrl", SDLK_LCTRL },
        { "enter", SDLK_RETURN } };
    for (auto& [name, v] : k) { if (n == name) return v; }
    if (n.size() >= 2 && n[0] == 'f' && isdigit((unsigned char)n[1])) return SDLK_F1 + (std::atoi(n.c_str() + 1) - 1);
    return n.empty() ? 0 : (SDL_Keycode)tolower((unsigned char)n[0]);
}
void InjInit() {
    s_InjInit = true;
    const char* env = std::getenv("NOTSA_STANDALONE_INPUT");
    if (!env) return;
    std::string text = env;
    if (std::ifstream f{ env }) { text.assign(std::istreambuf_iterator<char>(f), {}); }
    std::replace(text.begin(), text.end(), '\n', ';');
    std::stringstream ss(text);
    uint64_t t = 0;
    for (std::string item; std::getline(ss, item, ';');) {
        const auto c = item.find(':');
        if (c == std::string::npos) continue;
        const auto cmd = item.substr(0, c), arg = item.substr(c + 1);
        if (cmd == "wait") { t += std::atoll(arg.c_str()); }
        else if (cmd == "key") { s_InjEvents.push_back({ t, true, InjKey(arg) }); s_InjEvents.push_back({ t + 150, false, InjKey(arg) }); t += 150; }
        else if (cmd == "down" || cmd == "up") { s_InjEvents.push_back({ t, cmd == "down", InjKey(arg) }); }
    }
    std::stable_sort(s_InjEvents.begin(), s_InjEvents.end(), [](auto& a, auto& b) { return a.t < b.t; });
    s_InjStart = GetTickCount64();
    notsa::standalone::Fixups::Log("input injector: %u events", (unsigned)s_InjEvents.size());
}
void InjPump() {
    if (!s_InjInit) InjInit();
    const uint64_t now = GetTickCount64() - s_InjStart;
    while (s_InjNext < s_InjEvents.size() && s_InjEvents[s_InjNext].t <= now) {
        const auto& ev = s_InjEvents[s_InjNext++];
        SDL_Event e{};
        e.type         = ev.down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.key      = ev.key;
        e.key.down     = ev.down;
        e.key.scancode = SDL_GetScancodeFromKey(ev.key, nullptr);
        SDL_PushEvent(&e);
        notsa::standalone::Fixups::Log("injected key %d %s at %u ms", (int)ev.key, ev.down ? "down" : "up", (unsigned)now);
    }
}
} // namespace
#endif

namespace notsa {
namespace SDLWrapper {
bool Initialize() {
    return true;
}

void Terminate() {

}

void ProcessEvents() {
    ZoneScoped;
#ifdef NOTSA_STANDALONE_RUN
    InjPump();
#endif

    // Now process events
    const auto* const imCtx = ImGui::GetCurrentContext();
    const auto* const imIO  = imCtx ? &imCtx->IO : nullptr;
    for (SDL_Event e; SDL_PollEvent(&e);) {
#ifndef NOTSA_STANDALONE_RUN // no imgui backend in the standalone run build (UIRenderer stub)
        if (imIO) {
            ImGui_ImplSDL3_ProcessEvent(&e);
        }
#endif

        switch (e.type) {
        case SDL_EVENT_QUIT: {
            RsGlobal.quit = true;
            continue;
        }
        case SDL_EVENT_WINDOW_RESIZED: {
            const auto w = e.window.data1,
                       h = e.window.data2;

            NOTSA_LOG_DEBUG(
                "SDL: Window resized: {} x {}",
                w, h
            );

            continue;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (notsa::ui::UIRenderer::GetInstance().IsActive()) {
                break;
            }
            static CVector2D s_MousePos{};

            // Use desktop cursor speed if it's windowed, accelerate ingame otherwise.
            if (IsInFullscreen() && FrontEndMenuManager.m_bMenuActive) {
                s_MousePos.x += e.motion.xrel * CCamera::m_fMouseAccelHorzntl * 100.f;
                s_MousePos.y += e.motion.yrel * CCamera::m_fMouseAccelVertical * 100.f;
            } else {
                s_MousePos.x = e.motion.x;
                s_MousePos.y = e.motion.y;
            }
            FrontEndMenuManager.m_nMousePosWinX = (int32)(s_MousePos.x);
            FrontEndMenuManager.m_nMousePosWinY = (int32)(s_MousePos.y);
            break;
        }
        }

        if (CPad::ProcessEvent(e, imIO && imIO->WantCaptureMouse, imIO && imIO->WantCaptureKeyboard)) {
            continue;
        }

        //NOTSA_LOG_DEBUG("SDL: Unprocessed event: {}", e.type);
    }
}
}; // namespace SDLWrapper
}; // namespace notsa
#endif
