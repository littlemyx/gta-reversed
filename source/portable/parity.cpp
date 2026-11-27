// PARITY input record / playback, see parity.h and C:/gta-scratch/parity/FORMAT.md
#ifdef NOTSA_STATEHASH
#include "StdInc.h"
#include "parity.h"
#include "statehash.h"
#include "Pad.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

namespace notsa::rr { int LoadLogRecFrame(); }

namespace notsa::parity {
namespace {
constexpr const char* kPadNames[24] = { "LeftStickX", "LeftStickY", "RightStickX", "RightStickY", "LeftShoulder1", "LeftShoulder2", "RightShoulder1", "RightShoulder2", "DPadUp", "DPadDown", "DPadLeft", "DPadRight",
    "Start", "Select", "ButtonSquare", "ButtonTriangle", "ButtonCross", "ButtonCircle", "ShockButtonL", "ShockButtonR", "m_bChatIndicated", "m_bPedWalk", "m_bVehicleMouseLook", "m_bRadioTrackSkip" };
constexpr const char* kMsNames[7] = { "l", "r", "m", "wu", "wd", "x1", "x2" };
static_assert(sizeof(CControllerState) == 24 * sizeof(int16), "CControllerState is 24 int16");
constexpr size_t kKeys = sizeof(CKeyboardState) / sizeof(int16);
static_assert(sizeof(CKeyboardState) == 312 * sizeof(int16), "CKeyboardState is 312 int16");

struct MouseRec {
    uint8_t b[7]{}; float wheel = 0.f, dx = 0.f, dy = 0.f;
    bool operator==(const MouseRec& o) const { return !std::memcmp(b, o.b, sizeof b) && wheel == o.wheel && dx == o.dx && dy == o.dy; }
};
struct PadRec { int16_t v[24]{}; bool operator==(const PadRec& o) const { return !std::memcmp(v, o.v, sizeof v); } };
struct KeyRec { int16_t v[kKeys]{}; bool operator==(const KeyRec& o) const { return !std::memcmp(v, o.v, sizeof v); } };

const char* RecPath() { static const char* p = [] { const char* e = std::getenv("NOTSA_PARITY_REC"); return e && *e ? e : nullptr; }(); return p; }
const char* PlayPath() { static const char* p = [] { const char* e = std::getenv("NOTSA_PARITY_PLAY"); return e && *e ? e : nullptr; }(); return p; }
int CurFrame() { return notsa::rr::LoadLogRecFrame(); }

// ---- record side
FILE*                    g_rf = nullptr;
std::vector<std::string> g_cmds;                // commands of the current frame
PadRec                   g_padNow, g_padLast;
KeyRec                   g_keyNow, g_keyLast;
MouseRec                 g_msNow, g_msLast;
bool                     g_havePad = false, g_haveKey = false, g_haveMs = false, g_first = true;

std::string JsonStr(const std::string& s) { std::string o = "\""; for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; } return o + "\""; }
std::string Fl(float f) { char b[40]; std::snprintf(b, sizeof b, "%.9g", (double)f); return b; }

// ---- play side
struct PlayRec { unsigned f = 0; bool hasPad = false, hasKeys = false, hasMs = false; PadRec pad; KeyRec keys; MouseRec ms; std::vector<std::string> cmds; };
std::vector<PlayRec> g_play; size_t g_playNext = 0; bool g_playLoaded = false;
PadRec g_curPad; KeyRec g_curKeys; MouseRec g_curMs; bool g_curHasPad = false, g_curHasKeys = false, g_curHasMs = false;

// minimal reader for exactly the shape written by OnFrameEnd (one record per line)
bool FindNum(const std::string& s, const char* key, double& out) {
    const std::string k = std::string("\"") + key + "\":"; const size_t p = s.find(k); if (p == std::string::npos) return false;
    out = std::strtod(s.c_str() + p + k.size(), nullptr); return true;
}
template<typename Fn> void ParseObj(const std::string& s, size_t from, Fn&& fn) { // {"name":num,...} from the char after '{' up to '}'
    const size_t to = s.find('}', from); size_t i = from;
    while (i < to) { const size_t q = s.find('"', i); if (q == std::string::npos || q >= to) break; const size_t q2 = s.find('"', q + 1); const std::string name = s.substr(q + 1, q2 - q - 1); char* e = nullptr; const double v = std::strtod(s.c_str() + q2 + 2, &e); fn(name, v); i = (size_t)(e - s.c_str()); }
}
void Load() {
    g_playLoaded = true;
    std::ifstream in(PlayPath());
    std::string line;
    while (std::getline(in, line)) {
        double f; if (line.empty() || !FindNum(line, "f", f) || line.compare(0, 5, "{\"f\":")) continue;
        PlayRec r; r.f = (unsigned)f;
        if (const size_t p = line.find("\"pad\":{"); p != std::string::npos) { r.hasPad = true; ParseObj(line, p + 7, [&](const std::string& n, double v) { for (int i = 0; i < 24; i++) if (n == kPadNames[i]) r.pad.v[i] = (int16_t)v; }); }
        if (const size_t p = line.find("\"kb\":["); p != std::string::npos) {
            r.hasKeys = true; size_t i = p + 6;
            while (i < line.size()) { // [idx,val] pairs until the closing ] of the outer list
                if (line[i] == '[') { char* e = nullptr; const long idx = std::strtol(line.c_str() + i + 1, &e, 10); const long v = std::strtol(e + 1, &e, 10); if (idx >= 0 && (size_t)idx < kKeys) r.keys.v[idx] = (int16_t)v; i = (size_t)(e - line.c_str()) + 1; }
                else if (line[i] == ']') break; else i++;
            }
        }
        if (const size_t p = line.find("\"ms\":{"); p != std::string::npos) { r.hasMs = true; ParseObj(line, p + 6, [&](const std::string& n, double v) {
            for (int i = 0; i < 7; i++) if (n == kMsNames[i]) r.ms.b[i] = v != 0;
            if (n == "wheel") r.ms.wheel = (float)v; if (n == "dx") r.ms.dx = (float)v; if (n == "dy") r.ms.dy = (float)v; }); }
        if (const size_t p = line.find("\"cmd\":["); p != std::string::npos) { size_t i = p + 7; while (i < line.size() && line[i] != ']') { if (line[i] == '"') { std::string c; i++; while (i < line.size() && line[i] != '"') { if (line[i] == '\\') i++; c += line[i++]; } r.cmds.push_back(c); } i++; } }
        g_play.push_back(std::move(r));
    }
    std::fprintf(stderr, "[parity] play: %zu records from %s\n", g_play.size(), PlayPath());
}
} // namespace

bool Recording() { return RecPath() != nullptr; }
bool Playing() { return PlayPath() != nullptr; }

void RecCmd(const std::string& cmd) {
    if (!RecPath() || CurFrame() < 0) return;
    if (!cmd.compare(0, 6, "mdkey:") || !cmd.compare(0, 8, "rr_start") || !cmd.compare(0, 7, "rr_stop")) return;
    g_cmds.push_back(cmd);
}
void PadHook(CPad& p) {
    if (CurFrame() < 0) return;
    if (PlayPath()) { if (g_curHasPad) std::memcpy(&p.NewState, g_curPad.v, sizeof g_curPad.v); return; }
    if (RecPath()) { std::memcpy(g_padNow.v, &p.NewState, sizeof g_padNow.v); g_havePad = true; }
}
void MouseHook(CPad& p) {
    if (CurFrame() < 0) return;
    auto& m = p.NewMouseControllerState;
    if (PlayPath()) {
        if (g_curHasMs) {
            m.isMouseLeftButtonPressed = g_curMs.b[0]; m.isMouseRightButtonPressed = g_curMs.b[1]; m.isMouseMiddleButtonPressed = g_curMs.b[2]; m.isMouseWheelMovedUp = g_curMs.b[3]; m.isMouseWheelMovedDown = g_curMs.b[4];
            m.isMouseFirstXPressed = g_curMs.b[5]; m.isMouseSecondXPressed = g_curMs.b[6]; m.m_fWheelMoved = g_curMs.wheel; m.m_AmountMoved.x = g_curMs.dx; m.m_AmountMoved.y = g_curMs.dy;
        }
        return;
    }
    if (RecPath()) {
        MouseRec r; r.b[0] = m.isMouseLeftButtonPressed; r.b[1] = m.isMouseRightButtonPressed; r.b[2] = m.isMouseMiddleButtonPressed; r.b[3] = m.isMouseWheelMovedUp; r.b[4] = m.isMouseWheelMovedDown; r.b[5] = m.isMouseFirstXPressed; r.b[6] = m.isMouseSecondXPressed;
        r.wheel = m.m_fWheelMoved; r.dx = m.m_AmountMoved.x; r.dy = m.m_AmountMoved.y; g_msNow = r; g_haveMs = true;
    }
}
void KeysHook(CPad& p) {
    if (CurFrame() < 0) return;
    if (PlayPath()) { if (g_curHasKeys) std::memcpy(&p.NewKeyState, g_curKeys.v, sizeof g_curKeys.v); return; }
    if (RecPath()) { std::memcpy(g_keyNow.v, &p.NewKeyState, sizeof g_keyNow.v); g_haveKey = true; }
}
void PlayPump(const std::function<bool(const std::string&)>& run) {
    if (!PlayPath() || CurFrame() < 0) return;
    if (!g_playLoaded) Load();
    static size_t s_cmd = 0; // next command of the current record
    while (g_playNext < g_play.size() && (int)g_play[g_playNext].f <= CurFrame()) {
        auto& r = g_play[g_playNext];
        if (s_cmd == 0) { // states of a record apply once, at its first pump
            if (r.hasPad) { g_curPad = r.pad; g_curHasPad = true; }
            if (r.hasKeys) { g_curKeys = r.keys; g_curHasKeys = true; }
            if (r.hasMs) { g_curMs = r.ms; g_curHasMs = true; }
        }
        for (; s_cmd < r.cmds.size(); s_cmd++) { if (!run(r.cmds[s_cmd])) return; }
        s_cmd = 0; g_playNext++;
    }
}
void OnFrameEnd(unsigned frame) {
    if (!RecPath()) return;
    if (!g_rf) {
        g_rf = std::fopen(RecPath(), "w"); if (!g_rf) return;
        const char* tag = std::getenv("NOTSA_RR_TAG"); const char* seed = std::getenv("NOTSA_RR_SEED"); const char* step = std::getenv("NOTSA_RR_STEP_MS");
        std::fprintf(g_rf, "{\"hdr\":\"parity-input\",\"v\":0,\"build\":\"%s\",\"seed\":%s,\"step_ms\":%s,\"start\":\"boot route (New Game, until control, +600 ms, clock 06:30), frame 0 = the frame of cmd:rr_start\",\"padFields\":[",
            tag ? tag : "unset", seed ? seed : "12345", step ? step : "20");
        for (int i = 0; i < 24; i++) std::fprintf(g_rf, "%s\"%s\"", i ? "," : "", kPadNames[i]);
        std::fprintf(g_rf, "],\"kbLayout\":\"int16[312]: FKeys[12], standardKeys[256], esc insert del home end pgup pgdn up down left right scroll pause numlock div mul sub add enter decimal num1..num9 num0 back tab capslock extenter lshift rshift shift lctrl rctrl lalt ralt lwin rwin apps\"}\n");
        std::atexit([] { if (g_rf) { std::fclose(g_rf); g_rf = nullptr; } });
    }
    const std::string head = "{\"f\":" + std::to_string(frame);
    std::string o = head;
    const bool padCh = g_havePad && (g_first || !(g_padNow == g_padLast));
    const bool keyCh = g_haveKey && (g_first || !(g_keyNow == g_keyLast));
    const bool msCh = g_haveMs && (g_first || !(g_msNow == g_msLast));
    if (padCh) { o += ",\"pad\":{"; for (int i = 0; i < 24; i++) { o += std::string(i ? "," : "") + "\"" + kPadNames[i] + "\":" + std::to_string(g_padNow.v[i]); } o += "}"; g_padLast = g_padNow; }
    if (keyCh) { o += ",\"kb\":["; bool f1 = true; for (size_t i = 0; i < kKeys; i++) if (g_keyNow.v[i]) { o += (f1 ? "[" : ",[") + std::to_string(i) + "," + std::to_string(g_keyNow.v[i]) + "]"; f1 = false; } o += "]"; g_keyLast = g_keyNow; }
    if (msCh) { o += ",\"ms\":{"; for (int i = 0; i < 7; i++) o += std::string("\"") + kMsNames[i] + "\":" + (g_msNow.b[i] ? "1" : "0") + ","; o += "\"wheel\":" + Fl(g_msNow.wheel) + ",\"dx\":" + Fl(g_msNow.dx) + ",\"dy\":" + Fl(g_msNow.dy) + "}"; g_msLast = g_msNow; }
    if (!g_cmds.empty()) { o += ",\"cmd\":["; for (size_t i = 0; i < g_cmds.size(); i++) o += (i ? "," : "") + JsonStr(g_cmds[i]); o += "]"; }
    if (o.size() > head.size()) { o += "}\n"; std::fputs(o.c_str(), g_rf); std::fflush(g_rf); }
    g_cmds.clear(); g_first = false; g_havePad = g_haveKey = g_haveMs = false;
}
} // namespace notsa::parity
#endif
