// SCMTRACE-1 (see scmtrace.h). Compiled only with -DGTASA_SCMTRACE=ON.
#include "StdInc.h"

#ifdef NOTSA_SCMTRACE
#include "scmtrace.h"
#include "Scripts/RunningScript.h"
#include "Scripts/TheScripts.h"
#ifdef NOTSA_MDRIVER
#include "mdriver.h"
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <bit>

namespace notsa::scmtrace {
namespace {
struct Info {
    uint32_t              id;
    bool                  first = true;
    std::vector<uint32_t> loc; // shadow of the thread's local variables (zero at start: the first command's diff shows the parameters copied in by START_NEW_SCRIPT)
};
FILE*                                     g_f = nullptr;
bool                                      g_init = false;
int32_t                                   g_armed = -1;
int32_t                                   g_rootMission = -1;
uint32_t                                  g_nextId = 1;
uint32_t                                  g_frame0 = 0;
bool                                      g_haveFrame0 = false;
std::unordered_map<const CRunningScript*, Info> g_threads;
// the command in flight
CRunningScript*                           g_cur = nullptr;
Info*                                     g_curInfo = nullptr;
uint16_t                                  g_curRaw = 0;
const uint8_t*                            g_curPtr = nullptr;
CRunningScript*                           g_root = nullptr;
uint32_t                                  g_curIp = 0;
const char*                               g_curBase = "main";
char                                      g_curBaseBuf[16];
int                                       g_cond = -1;
bool                                      g_endPending = false;
std::string                               g_pending; // S / E rows produced during the command, flushed after its X/G/L rows
std::vector<uint8_t>                      g_gsnap;
size_t                                    g_gsize = 0;

// SHA-256 (header: main.scm hash)
struct Sha256 {
    uint32_t h[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
    uint8_t  buf[64]; uint64_t len = 0; size_t n = 0;
    static uint32_t R(uint32_t x, int k) { return (x >> k) | (x << (32 - k)); }
    void block(const uint8_t* p) {
        static const uint32_t K[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
        uint32_t w[64];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4*i] << 24 | (uint32_t)p[4*i+1] << 16 | (uint32_t)p[4*i+2] << 8 | p[4*i+3];
        for (int i = 16; i < 64; i++) { uint32_t s0 = R(w[i-15],7) ^ R(w[i-15],18) ^ (w[i-15] >> 3), s1 = R(w[i-2],17) ^ R(w[i-2],19) ^ (w[i-2] >> 10); w[i] = w[i-16] + s0 + w[i-7] + s1; }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + (R(e,6)^R(e,11)^R(e,25)) + ((e&f)^(~e&g)) + K[i] + w[i], t2 = (R(a,2)^R(a,13)^R(a,22)) + ((a&b)^(a&c)^(b&c));
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    void update(const uint8_t* p, size_t k) { len += k; while (k) { size_t c = std::min(k, 64 - n); std::memcpy(buf + n, p, c); n += c; p += c; k -= c; if (n == 64) { block(buf); n = 0; } } }
    std::string hex() {
        uint64_t bits = len * 8; uint8_t pad = 0x80; update(&pad, 1); pad = 0; while (n != 56) update(&pad, 1);
        uint8_t l[8]; for (int i = 0; i < 8; i++) l[i] = (uint8_t)(bits >> (56 - 8 * i)); update(l, 8);
        char s[65]; for (int i = 0; i < 8; i++) std::snprintf(s + 8 * i, 9, "%08x", h[i]); return s;
    }
};

std::string MainScmHash() {
    CFileMgr::ChangeDir("\\");
    CFileMgr::SetDir("");
    auto* file = CFileMgr::OpenFile("data\\script\\main.scm", "rb");
    if (!file) return "unknown";
    Sha256 sh; static uint8_t b[65536];
    for (;;) { const auto n = CFileMgr::Read(file, b, sizeof b); if (n <= 0) break; sh.update(b, (size_t)n); }
    CFileMgr::CloseFile(file);
    return sh.hex();
}

void Open() {
    if (g_init) return;
    g_init = true;
    const char* p = std::getenv("NOTSA_SCMTRACE");
    if (!p || !*p) return;
    g_f = std::fopen(p, "wb");
}

void Line(const std::string& s) { if (g_f) { std::fwrite(s.data(), 1, s.size(), g_f); } }

uint32_t Frame() { return g_haveFrame0 ? CTimer::GetFrameCounter() - g_frame0 : 0; }

//! base name + offset of the script's current IP: the mission block, the main.scm block (by address range, not by m_BaseIP)
void Locate(const CRunningScript* s, const char*& base, uint32_t& ip) {
    const uint8_t* p = s->m_IP;
    const uint8_t* mb = CTheScripts::MissionBlock.data();
    if (p >= mb && p < mb + CTheScripts::MissionBlock.size()) {
        std::snprintf(g_curBaseBuf, sizeof g_curBaseBuf, "m%d", g_rootMission);
        base = g_curBaseBuf; ip = (uint32_t)(p - mb);
    } else if (p >= CTheScripts::ScriptSpace.data() && p < CTheScripts::ScriptSpace.data() + CTheScripts::MainSCMBlock.size()) {
        base = "main"; ip = (uint32_t)(p - CTheScripts::ScriptSpace.data());
    } else { // a streamed script (own buffer): s<index in StreamedScripts>, ip = offset inside it
        std::snprintf(g_curBaseBuf, sizeof g_curBaseBuf, "s%d", s->m_BaseIP ? (int)CTheScripts::StreamedScripts.GetStreamedScriptWithThisStartAddress(s->m_BaseIP) : -1);
        base = g_curBaseBuf; ip = s->m_BaseIP ? (uint32_t)(p - s->m_BaseIP) : 0;
    }
}

std::string Name(const CRunningScript* s) {
    char n[9]; std::memcpy(n, s->m_szName, 8); n[8] = 0;
    return n;
}

std::string Val(uint32_t v) {
    char b[48];
    // type is unknown for a diffed dword: a value whose bit pattern is a plausible float (|x| in ~6e-11..1e9) is printed as float with an 'f' suffix, anything else as int
    const uint32_t a = v & 0x7FFFFFFFu;
    if (a >= 0x2E800000u && a <= 0x4E800000u) { std::snprintf(b, sizeof b, "%.9gf", (double)std::bit_cast<float>(v)); }
    else { std::snprintf(b, sizeof b, "%d", (int32_t)v); }
    return b;
}

uint32_t* LocalsOf(CRunningScript* s, size_t& n) {
    if (s->m_ThisMustBeTheOnlyMissionRunning) { n = CTheScripts::LocalVariablesForCurrentMission.size(); return reinterpret_cast<uint32_t*>(CTheScripts::LocalVariablesForCurrentMission.data()); }
    n = s->m_LocalVars.size(); return reinterpret_cast<uint32_t*>(s->m_LocalVars.data());
}
} // namespace

bool Enabled() { Open(); return g_f != nullptr; }

void ArmRoot(int32_t missionId) { if (Enabled()) { g_armed = missionId; } }

bool Traced(const CRunningScript* s) { return g_threads.find(s) != g_threads.end(); }
CRunningScript* Root() { return (g_root && g_threads.find(g_root) != g_threads.end()) ? g_root : nullptr; }

void OnStart(CRunningScript* s) {
    if (!Enabled()) return;
    const bool root = g_armed >= 0;
    if (!root && !g_cur) return; // not started by a traced thread
    Info info; info.id = g_nextId++;
    uint32_t parent = 0;
    if (root) {
        g_rootMission = g_armed; g_armed = -1; g_root = s; g_frame0 = CTimer::GetFrameCounter(); g_haveFrame0 = true;
        const char* step = std::getenv("NOTSA_RR_STEP_MS"); const char* commit = std::getenv("NOTSA_SCMTRACE_COMMIT");
        std::string h = "# scmtrace 1\n# main.scm sha256 " + MainScmHash() + "\n# commit " + (commit ? commit : "unknown") + "\n# mission m" + std::to_string(g_rootMission)
            + "\n# timestep " + (step ? step : "20") + " ms fixed (virtual clock, NOTSA_RR=1)\n# start injected mission:" + std::to_string(g_rootMission) + " (same as LOAD_AND_LAUNCH_MISSION " + std::to_string(g_rootMission) + "), frame 0 = the frame of the start, mission thread at ip 0\n"
            + "# columns S frame thread_id name base ip parent_id | E frame thread_id name | X frame thread_id name base ip opcode cond | G frame thread_id index value | L frame thread_id index value\n"
            + "# index: G = byte offset in the global space, L = local slot; ip / opcode hex (opcode keeps the 0x8000 NOT bit); cond = value after NOT for conditional commands else '-'; value = int, or float %.9g with an 'f' suffix (bit-pattern heuristic)\n"
            + "# order within a command: X, then G rows, then L rows, then S/E rows caused by it\n";
        Line(h);
        const uint8_t* sp = CTheScripts::ScriptSpace.data();
        g_gsize = (sp[0] == 0x02 && sp[1] == 0x00 && sp[2] == 0x01) ? (size_t)(*reinterpret_cast<const int32_t*>(sp + 3)) : CTheScripts::MainSCMBlock.size();
        if (g_gsize > CTheScripts::MainSCMBlock.size()) g_gsize = CTheScripts::MainSCMBlock.size();
        g_gsnap.assign(g_gsize, 0);
    } else if (auto it = g_threads.find(g_cur); it != g_threads.end()) {
        parent = it->second.id;
    }
    info.loc.assign(1024, 0);
    // S row (the name is still "noname" here: SCRIPT_NAME runs as the thread's first command; X rows carry the current name)
    const char* base; uint32_t ip; Locate(s, base, ip);
    char b[200];
    std::snprintf(b, sizeof b, "S\t%u\t%u\t%s\t%s\t%x\t%s\n", Frame(), info.id, Name(s).c_str(), base, ip, parent ? std::to_string(parent).c_str() : "-");
    if (root) Line(b); else g_pending += b;
    g_threads.emplace(s, std::move(info));
}

void OnEnd(CRunningScript* s) {
    if (!g_f) return;
    auto it = g_threads.find(s);
    if (it == g_threads.end()) return;
    if (s == g_cur) { g_endPending = true; return; }
    char b[96]; std::snprintf(b, sizeof b, "E\t%u\t%u\t%s\n", Frame(), it->second.id, Name(s).c_str());
    Line(b);
#ifdef NOTSA_MDRIVER
    notsa::mdriver::AuxForget(s);
#endif
    g_threads.erase(it);
}

static int g_auxCond = -1;
int AuxCond() { return g_auxCond; }
void AuxReset() { g_auxCond = -1; }
void OnCond(bool state) { g_auxCond = state ? 1 : 0; if (g_cur) g_cond = state ? 1 : 0; }

void Begin(CRunningScript* s) {
    g_cur = s; g_curInfo = &g_threads.find(s)->second;
    g_curRaw = *reinterpret_cast<const uint16_t*>(s->m_IP); g_curPtr = s->m_IP;
    Locate(s, g_curBase, g_curIp);
    g_cond = -1; g_endPending = false;
    std::memcpy(g_gsnap.data(), CTheScripts::ScriptSpace.data(), g_gsize);
    if (!g_curInfo->first) {
        size_t n; const uint32_t* l = LocalsOf(s, n);
        std::memcpy(g_curInfo->loc.data(), l, std::min<size_t>(n, 1024) * 4);
    }
}

void End(CRunningScript* s) {
    const uint32_t id = g_curInfo->id, fr = Frame();
#ifdef NOTSA_MDRIVER
    notsa::mdriver::OnCommand(s, g_curPtr, g_curRaw, g_cond);
#endif
    const std::string name = Name(s);
    char b[200];
    std::snprintf(b, sizeof b, "X\t%u\t%u\t%s\t%s\t%x\t%x\t%c\n", fr, id, name.c_str(), g_curBase, g_curIp, (unsigned)g_curRaw, g_cond < 0 ? '-' : (char)('0' + g_cond));
    Line(b);
    const uint8_t* now = CTheScripts::ScriptSpace.data();
    if (std::memcmp(now, g_gsnap.data(), g_gsize) != 0) {
        for (size_t o = 0; o + 4 <= g_gsize; o += 4) {
            uint32_t a, c; std::memcpy(&a, g_gsnap.data() + o, 4); std::memcpy(&c, now + o, 4);
            if (a != c) { std::snprintf(b, sizeof b, "G\t%u\t%u\t%zu\t", fr, id, o); Line(b); Line(Val(c)); Line("\n");
#ifdef NOTSA_MDRIVER
                notsa::mdriver::AuxGlobalWrite(s, (uint32_t)o);
#endif
            }
        }
    }
    {
        size_t n; const uint32_t* l = LocalsOf(s, n);
        for (size_t i = 0; i < std::min<size_t>(n, 1024); i++) {
            if (g_curInfo->loc[i] != l[i]) { std::snprintf(b, sizeof b, "L\t%u\t%u\t%zu\t", fr, id, i); Line(b); Line(Val(l[i])); Line("\n"); }
        }
    }
    g_curInfo->first = false;
    Line(g_pending); g_pending.clear();
    if (g_endPending) {
        std::snprintf(b, sizeof b, "E\t%u\t%u\t%s\n", fr, id, name.c_str()); Line(b);
#ifdef NOTSA_MDRIVER
        notsa::mdriver::AuxForget(s);
#endif
        g_threads.erase(s);
    }
    g_cur = nullptr; g_curInfo = nullptr; g_endPending = false;
}
} // namespace notsa::scmtrace
#endif
