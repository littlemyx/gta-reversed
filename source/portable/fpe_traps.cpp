// MAC-FPTRAP: see fpe_traps.h. Portable-only (globbed by source/CMakeLists.txt for the portable target); empty unless GTASA_FPE_TRAPS is ON.
#include "fpe_traps.h"

#if defined(NOTSA_FPE_TRAPS) && defined(NOTSA_PORTABLE) && defined(__APPLE__) && defined(__aarch64__)
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ucontext.h>
#include <mach-o/dyld.h>

namespace notsa::fpe {
namespace {
constexpr std::uint64_t kIOE = 1u << 8, kDZE = 1u << 9, kOFE = 1u << 10;
constexpr unsigned kMaxSites = 8192, kMaxThreads = 32, kMaxBt = 24;

struct Site {
    std::atomic<std::uint64_t> pc{0};
    std::atomic<std::uint32_t> count{0};
    std::atomic<std::uint32_t> kinds{0}; // OR of ESR[7,4:0]
    std::atomic<std::uint64_t> caller{0}; // lr of the first hit
    std::atomic<std::uint32_t> whitelisted{0};
    std::atomic<std::uint32_t> unemulated{0};
};
Site                      g_sites[kMaxSites];
std::atomic<unsigned>     g_nSites{0};
std::atomic<std::uint64_t> g_overflowHits{0};

struct Thr { std::atomic<pthread_t> id{nullptr}; char name[24]{}; };
Thr                       g_threads[kMaxThreads];
std::atomic<unsigned>     g_nThreads{0};
std::atomic<bool>         g_dirty{false};
std::atomic<bool>         g_inited{false};
std::uint64_t             g_enableMask = kIOE | kDZE;
bool                      g_abortMode  = false;
bool                      g_whitelist  = true;
unsigned                  g_btDepth    = 10;
int                       g_logFd      = 2;
char                      g_summaryPath[512];
struct sigaction          g_prevIll{}, g_prevFpe{};

std::uint64_t ReadFpcr() { std::uint64_t v; __asm__ volatile("mrs %0, fpcr" : "=r"(v)); return v; }
void          WriteFpcr(std::uint64_t v) { __asm__ volatile("msr fpcr, %0" ::"r"(v)); }

void Out(const char* s, std::size_t n) { (void)!::write(g_logFd, s, n); }
template <class... A> void Outf(const char* fmt, A... a) {
    char b[768];
    const int n = std::snprintf(b, sizeof b, fmt, a...);
    if (n > 0) { Out(b, std::min<std::size_t>((std::size_t)n, sizeof b - 1)); }
}

const char* KindName(unsigned k, char* buf) { // ESR ISS: IOF=0 DZF=1 OFF=2 UFF=3 IXF=4 IDF=7
    buf[0] = 0;
    if (k & 1) std::strcat(buf, "INVALID|");
    if (k & 2) std::strcat(buf, "DIVBYZERO|");
    if (k & 4) std::strcat(buf, "OVERFLOW|");
    if (k & 8) std::strcat(buf, "UNDERFLOW|");
    if (k & 16) std::strcat(buf, "INEXACT|");
    if (k & 0x80) std::strcat(buf, "DENORM|");
    if (const auto l = std::strlen(buf)) buf[l - 1] = 0;
    return buf;
}

// "module!symbol+off" (dladdr only knows exported / symtab names; the raw module-relative offset is always printed so `nm` / `atos -o` can finish the job)
struct Sym { char mod[64]; char name[160]; std::uint64_t off; std::uint64_t rel; bool ours; };
Sym Resolve(std::uint64_t pc) {
    Sym s{};
    Dl_info di{};
    if (::dladdr((void*)pc, &di)) {
        const char* m = di.dli_fname ? std::strrchr(di.dli_fname, '/') : nullptr;
        std::snprintf(s.mod, sizeof s.mod, "%s", m ? m + 1 : (di.dli_fname ? di.dli_fname : "?"));
        std::snprintf(s.name, sizeof s.name, "%s", di.dli_sname ? di.dli_sname : "?");
        s.off  = di.dli_saddr ? pc - (std::uint64_t)di.dli_saddr : 0;
        s.rel  = pc - (std::uint64_t)di.dli_fbase;
        s.ours = std::strstr(s.mod, "gta_reversed") != nullptr;
    } else {
        std::snprintf(s.mod, sizeof s.mod, "?"); std::snprintf(s.name, sizeof s.name, "?");
    }
    return s;
}

// Sites known to be benign by design (classification: .notes/reports/MAC-FPTRAP.md). Matched against the mangled symbol of the trapping PC and, when `caller` is
// set, of its caller (lr). A whitelisted site is still counted (summary, marked [WL]) but its first hit is not logged and NOTSA_FPE_MODE=abort does not fire.
constexpr struct { const char* sym; const char* caller; const char* why; } kWhitelist[] = {
    { "DistToMathematicalLine", nullptr, "`distSq <= 0 ? 0 : sqrt(distSq)`: clang if-converts it into fsqrt + fcsel, the discarded arm takes the sqrt of a (rounding-)negative distSq (MAC-NAN1: operands are finite, not NaN); the result is 0 as in the exe" },
    { "_Z20FrameUpdateCallBackTILb", nullptr, "NaN / 1e38 quaternion lane of the LAST key frame of PROLOG1.IFP (exporter junk in the file, MAC-ANIM1): applied for 1-2 frames when a cutscene object's anim reaches its end, the exe evaluates the same data (its fcomp + jp sends NaN to the `!(dot < 0)` branch like the port); consumers are NaN-safe (FtolCast NaN -> 0, NOTSA_MATRIX_CHECK clean)" },
    { "_ZN6CTimer6UpdateEv", nullptr, "game_FPS = 1000 / (clock delta == 0): inf, same masked x87 result in the exe (frozen clock in menus / pause)" },
    { "_Z10CameraSizePN2rw6CameraEPNS_4RectEff", nullptr, "start-up only: viewWindow / CDraw::ms_fAspectRatio while the ratio is still 0 (BSS in the exe too), the view window is set again by CCamera::Process" },
    { "_Z32RpAnimBlendClumpUpdateAnimationsPN2rw5ClumpEfb", nullptr, "clang if-converts `totalTime == 0 ? 1 : totalBlendAmnt / totalTime` into fdiv + fcsel: 0/0 of the discarded arm (variable unused)" },
    { "_ZN12_GLOBAL__N_111ConvertIm2D", nullptr, "clang if-converts `rhw != 0 ? 1 / rhw : ...` into fdiv + fcsel: 1/0 of the discarded arm" },
    { "_ZN8CVehicle17DoLightEffectImpl", nullptr, "std::sqrt(dot) with dot < 0 for lights facing away from the camera: NaN is only consumed under `angle > 0` (exe: masked fsqrt, same)" },
    { "_ZN15CAEAudioUtility20ConvertFromBytesToMS", nullptr, "SampleFrequency == 0 sounds: bytes / 0 = inf (or 0/0) -> NOTSA_FTOL range guard = exe's fistp INT_MIN, low 16 bits 0" },
    { "_ZN12_GLOBAL__N_120ProcessLineTriangleT", nullptr, "line parallel to the triangle plane: t = num / 0 (inf / NaN), rejected by the `!(t < max)` NaN-aware test, matches the exe (NaN => false)" },
    { "_ZN13FxEmitterBP_c6RenderEPN2rw6CameraEjfb", nullptr, "particle with totalLife 0: currentLife / totalLife = 0/0, exe x87 yields the same NaN" },
    { "_ZL23ClampSpriteQuadToScreen", nullptr, "halfSize == 0 sprite: x / 0 = +-inf corner coordinates, same masked x87 division in the exe; the quad is then clipped away" },
    { "_ZN6CRadar13ClipRadarPoly", nullptr, "slope of a vertical edge (dx == 0) = inf, mirrored 1:1 from the exe's masked x87 division" },
    { "_ZN10CWorldScan9ScanWorld", nullptr, "scan-line slope of a horizontal / vertical edge = inf or 0/0, same masked x87 division in the exe (water block scan)" },
    { "_ZN12_GLOBAL__N_117TestLineTriangleT", nullptr, "line parallel to the triangle plane: t = num / 0, rejected by the NaN-aware range test (same as ProcessLineTriangleT)" },
    { "log10", "_ZN21CAEWeatherAudioEntity7ServiceEv", "log10(0) = -inf (no wind / rain), exe fyl2x gives -inf too; the volume is clamped afterwards" },
    { "log10f", "_ZN6CEvent13GetSoundLevelEPK7CEntityR7CVector", "log10(0) = -inf for a zero distance, exe fyl2x gives the same (-inf) and the level is only compared / clamped by the callers" },
};
const char* WhitelistReason(const Sym& s, std::uint64_t lr) {
    if (!g_whitelist) return nullptr;
    Sym c{}; bool haveC = false;
    for (const auto& w : kWhitelist) {
        if (!std::strstr(s.name, w.sym)) continue;
        if (!w.caller) return w.why;
        if (!haveC) { c = Resolve(lr); haveC = true; }
        if (std::strstr(c.name, w.caller)) return w.why;
    }
    return nullptr;
}

Site* FindSite(std::uint64_t pc, bool& isNew) {
    isNew = false;
    unsigned h = (unsigned)((pc >> 2) * 0x9E3779B1u) % kMaxSites;
    for (unsigned i = 0; i < kMaxSites; ++i) {
        Site& s = g_sites[(h + i) % kMaxSites];
        std::uint64_t cur = s.pc.load(std::memory_order_acquire);
        if (cur == pc) return &s;
        if (cur == 0) {
            std::uint64_t expected = 0;
            if (s.pc.compare_exchange_strong(expected, pc)) { isNew = true; g_nSites++; return &s; }
            if (expected == pc) return &s;
        }
    }
    return nullptr;
}

void DumpSummary(int fd) {
    // sorted by count, simple selection of the top entries
    struct Row { std::uint64_t pc; std::uint32_t count, kinds, wl; std::uint64_t caller; };
    static Row rows[kMaxSites]; // only the summary writer touches it (helper thread / atexit, never concurrently with itself in practice)
    unsigned n = 0;
    for (unsigned i = 0; i < kMaxSites; ++i) {
        const auto pc = g_sites[i].pc.load();
        if (pc) rows[n++] = {pc, g_sites[i].count.load(), g_sites[i].kinds.load(), g_sites[i].whitelisted.load(), g_sites[i].caller.load()};
    }
    std::sort(rows, rows + n, [](const Row& a, const Row& b) { return a.count > b.count; });
    auto put = [&](const char* fmt, auto... a) { char b[768]; const int k = std::snprintf(b, sizeof b, fmt, a...); if (k > 0) (void)!::write(fd, b, std::min<std::size_t>((std::size_t)k, sizeof b - 1)); };
    put("# fpe summary: %u distinct PCs, overflow hits %llu\n", n, (unsigned long long)g_overflowHits.load());
    for (unsigned i = 0; i < n; ++i) {
        char kb[64]; const Sym s = Resolve(rows[i].pc); const Sym c = Resolve(rows[i].caller);
        put("%8u  %-22s %s %s!%s+%llu (rel 0x%llx)  <- %s!%s+%llu (rel 0x%llx)\n", rows[i].count, KindName(rows[i].kinds, kb), rows[i].wl ? "[WL]" : "    ", s.mod, s.name,
            (unsigned long long)s.off, (unsigned long long)s.rel, c.mod, c.name, (unsigned long long)c.off, (unsigned long long)c.rel);
    }
}
void WriteSummaryFile() {
    if (!g_summaryPath[0]) return;
    char tmp[560]; std::snprintf(tmp, sizeof tmp, "%s.tmp", g_summaryPath);
    const int fd = ::open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    DumpSummary(fd);
    ::close(fd);
    ::rename(tmp, g_summaryPath);
}
void AtExit() {
    DumpSummary(g_logFd);
    WriteSummaryFile();
}


// Emulates the trapped instruction in software (the handler runs with the traps masked) and steps over it: the thread stays armed, no coverage is lost after a hit and a
// site that traps 100k times (menu loop) costs one signal each, not a disarm / re-arm round trip. Scalar FADD/FSUB/FMUL/FDIV/FSQRT/FCVTZS/FCVTZU/FCMP(E) only; anything
// else (NEON vector forms, fused ops) returns false and takes the disarm / re-arm fallback.
bool Emulate(ucontext_t* uc, std::uint32_t insn) {
    auto*    ns = &uc->uc_mcontext->__ns;
    auto*    ss = &uc->uc_mcontext->__ss;
    const unsigned rd = insn & 31, rn = (insn >> 5) & 31, rm = (insn >> 16) & 31;
    auto getF = [&](unsigned r) { float f; std::memcpy(&f, &ns->__v[r], 4); return f; };
    auto getD = [&](unsigned r) { double d; std::memcpy(&d, &ns->__v[r], 8); return d; };
    auto setF = [&](unsigned r, float f) { ns->__v[r] = 0; std::memcpy(&ns->__v[r], &f, 4); };
    auto setD = [&](unsigned r, double d) { ns->__v[r] = 0; std::memcpy(&ns->__v[r], &d, 8); };
    const unsigned ty = (insn >> 22) & 3; // 0 = single, 1 = double
    if ((insn & 0xFF200C00u) == 0x1E200800u && ty <= 1) { // FMUL FDIV FADD FSUB
        const unsigned op = (insn >> 12) & 15;
        if (op > 3) return false;
        volatile double dres = 0; volatile float fres = 0;
        if (ty == 0) {
            volatile float a = getF(rn), b = getF(rm);
            fres = op == 0 ? a * b : op == 1 ? a / b : op == 2 ? a + b : a - b;
            setF(rd, fres);
        } else {
            volatile double a = getD(rn), b = getD(rm);
            dres = op == 0 ? a * b : op == 1 ? a / b : op == 2 ? a + b : a - b;
            setD(rd, dres);
        }
        return true;
    }
    if ((insn & 0xFFBFFC00u) == 0x1E21C000u && ty <= 1) { // FSQRT
        if (ty == 0) { volatile float a = getF(rn); setF(rd, __builtin_sqrtf(a)); } else { volatile double a = getD(rn); setD(rd, __builtin_sqrt(a)); }
        return true;
    }
    if (((insn & 0x7F3FFC00u) == 0x1E380000u || (insn & 0x7F3FFC00u) == 0x1E390000u) && ty <= 1) { // FCVTZS / FCVTZU (scalar, to W / X)
        const bool sf = insn >> 31, uns = (insn >> 16) & 1;
        const double v = ty == 0 ? (double)getF(rn) : getD(rn);
        std::uint64_t r;
        if (v != v) r = 0;
        else if (uns) r = v <= 0 ? 0 : v >= (sf ? 18446744073709551616.0 : 4294967296.0) ? (sf ? ~0ull : 0xFFFFFFFFull) : (std::uint64_t)v;
        else {
            const double lo = sf ? -9223372036854775808.0 : -2147483648.0, hi = -lo;
            r = v <= lo ? (std::uint64_t)(sf ? INT64_MIN : (std::int64_t)INT32_MIN) : v >= hi ? (std::uint64_t)(sf ? INT64_MAX : (std::int64_t)INT32_MAX) : (std::uint64_t)(std::int64_t)v;
        }
        if (!sf) r &= 0xFFFFFFFFull;
        if (rd != 31) ss->__x[rd] = r;
        return true;
    }
    // MAC-NAN1: more forms seen in the logs (every un-emulated instruction runs with the traps masked for ~2 ms and can leave a quiet NaN that a later, trapping consumer then
    // reports as if it were a producer: the "NaN lane in FrameUpdateCallBackT" of MAC-FPTRAP was one of these artefacts).
    auto cvtInt = [](double v, bool sf, bool uns, int rmode) -> std::uint64_t { // FCVT{N,P,M,Z}{S,U}: rmode 0 nearest-even, 1 +inf, 2 -inf, 3 zero; NaN -> 0, saturating
        if (v != v) return 0;
        double r = rmode == 0 ? __builtin_nearbyint(v) : rmode == 1 ? __builtin_ceil(v) : rmode == 2 ? __builtin_floor(v) : __builtin_trunc(v);
        if (uns) { return r <= 0 ? 0 : r >= (sf ? 18446744073709551616.0 : 4294967296.0) ? (sf ? ~0ull : 0xFFFFFFFFull) : (std::uint64_t)r; }
        const double lo = sf ? -9223372036854775808.0 : -2147483648.0, hi = -lo;
        return r <= lo ? (std::uint64_t)(sf ? INT64_MIN : (std::int64_t)INT32_MIN) : r >= hi ? (std::uint64_t)(sf ? INT64_MAX : (std::int64_t)INT32_MAX) : (std::uint64_t)(std::int64_t)r;
    };
    if ((insn & 0x7F260000u) == 0x1E200000u && ty <= 1 && ((insn >> 10) & 0x3F) == 0) { // FCVT{N,P,M,Z}{S,U} (scalar to W / X); the existing Z-only case below is a subset
        const bool sf = insn >> 31, uns = (insn >> 16) & 1;
        const double v = ty == 0 ? (double)getF(rn) : getD(rn);
        std::uint64_t r = cvtInt(v, sf, uns, (int)((insn >> 19) & 3));
        if (!sf) r &= 0xFFFFFFFFull;
        if (rd != 31) ss->__x[rd] = r;
        return true;
    }
    if ((insn & 0x7F3F0000u) == 0x1E180000u && ty <= 1) { // FCVTZS fixed point (scalar to W / X, #fbits): scale field = 64 - fbits (sf) / 32 - fbits
        const bool sf = insn >> 31;
        const unsigned scale = (insn >> 10) & 0x3F, fbits = (sf ? 64u : 32u) - scale;
        const double v = (ty == 0 ? (double)getF(rn) : getD(rn)) * __builtin_ldexp(1.0, (int)fbits);
        std::uint64_t r = cvtInt(v, sf, false, 3);
        if (!sf) r &= 0xFFFFFFFFull;
        if (rd != 31) ss->__x[rd] = r;
        return true;
    }
    if ((insn & 0xDFBFFC00u) == 0x5EA1B800u) { // FCVTZS / FCVTZU scalar SIMD (Sd = Sn, Dd = Dn)
        const bool uns = (insn >> 29) & 1, dbl = (insn >> 22) & 1;
        const double v = dbl ? getD(rn) : (double)getF(rn);
        const std::uint64_t r = cvtInt(v, dbl, uns, 3);
        if (dbl) setD(rd, std::bit_cast<double>(r)); else setF(rd, std::bit_cast<float>((std::uint32_t)r));
        return true;
    }
    if ((insn & 0xFFA0FC00u) == 0x7EA0D400u) { // FABD scalar (S / D)
        if (((insn >> 22) & 1) == 0) { volatile float a = getF(rn), b = getF(rm); setF(rd, __builtin_fabsf(a - b)); }
        else { volatile double a = getD(rn), b = getD(rm); setD(rd, __builtin_fabs(a - b)); }
        return true;
    }
    if ((insn & 0xBFA0FC00u) == 0x2EA0E400u || (insn & 0xBFA0FC00u) == 0x2E20E400u) { // FCMGT (U=1, 0x2EA0E4..) / FCMGE (U=1, 0x2E20E4..) vector, register form
        const bool q = (insn >> 30) & 1, dbl = (insn >> 22) & 1, gt = ((insn >> 23) & 1) == 1; // bit 23 = 1: FCMGT, 0: FCMGE
        const unsigned lanes = dbl ? (q ? 2 : 1) : (q ? 4 : 2);
        std::uint8_t out[16] = {}, a[16], b[16];
        std::memcpy(a, &ns->__v[rn], 16); std::memcpy(b, &ns->__v[rm], 16);
        for (unsigned i = 0; i < lanes; ++i) {
            bool m;
            if (dbl) { double x, y; std::memcpy(&x, a + 8 * i, 8); std::memcpy(&y, b + 8 * i, 8); m = gt ? x > y : x >= y; if (m) std::memset(out + 8 * i, 0xFF, 8); }
            else { float x, y; std::memcpy(&x, a + 4 * i, 4); std::memcpy(&y, b + 4 * i, 4); m = gt ? x > y : x >= y; if (m) std::memset(out + 4 * i, 0xFF, 4); }
        }
        std::memcpy(&ns->__v[rd], out, 16);
        return true;
    }
    if ((insn & 0xFF20FC07u) == 0x1E202000u && ty <= 1) { // FCMP / FCMPE (register or #0.0)
        const bool zero = (insn >> 3) & 1;
        double a, b;
        if (ty == 0) { a = getF(rn); b = zero ? 0.0f : getF(rm); } else { a = getD(rn); b = zero ? 0.0 : getD(rm); }
        std::uint32_t nzcv = (a != a || b != b) ? 0x3 : a == b ? 0x6 : a < b ? 0x8 : 0x2;
        ss->__cpsr = (ss->__cpsr & 0x0FFFFFFFu) | (nzcv << 28);
        return true;
    }
    return false;
}

void OnTrap(int sig, siginfo_t* si, void* vuc) {
    WriteFpcr(ReadFpcr() & ~(std::uint64_t)(kIOE | kDZE | kOFE)); // the handler itself computes with the traps masked
    auto* uc  = static_cast<ucontext_t*>(vuc);
    auto* mc  = uc->uc_mcontext;
    const unsigned esr = mc->__es.__esr;
    if ((esr >> 26) != 0x2C) { // not a trapped FP exception: hand over to whoever was installed before us (crash handler)
        const struct sigaction& prev = (sig == SIGILL) ? g_prevIll : g_prevFpe;
        if (prev.sa_flags & SA_SIGINFO && prev.sa_sigaction) { prev.sa_sigaction(sig, si, vuc); return; }
        ::signal(sig, SIG_DFL);
        return;
    }
    const std::uint64_t pc = __darwin_arm_thread_state64_get_pc(mc->__ss);
    const std::uint64_t lr = __darwin_arm_thread_state64_get_lr(mc->__ss);
    const unsigned kind = (esr & 0x1F) | (esr & 0x80);

    bool isNew = false;
    Site* site = FindSite(pc, isNew);
    if (site) {
        site->count.fetch_add(1, std::memory_order_relaxed);
        site->kinds.fetch_or(kind, std::memory_order_relaxed);
    } else {
        g_overflowHits++;
    }
    const bool log = site && isNew;
    if (log || g_abortMode) {
        char kb[64]; const Sym s = Resolve(pc);
        const char* wl = WhitelistReason(s, lr);
        if (site && isNew) { site->caller.store(lr); if (wl) site->whitelisted.store(1); }
        if (!wl || g_abortMode) {
            Outf("[fpe] %s at %s!%s+%llu (rel 0x%llx) pc=0x%llx%s\n", KindName(kind, kb), s.mod, s.name, (unsigned long long)s.off, (unsigned long long)s.rel, (unsigned long long)pc,
                 g_abortMode ? " [abort]" : "");
            std::uint64_t frames[kMaxBt]; unsigned nf = 0;
            frames[nf++] = lr;
            auto* fp = reinterpret_cast<std::uint64_t*>(__darwin_arm_thread_state64_get_fp(mc->__ss));
            while (fp && nf < std::min(g_btDepth, kMaxBt) && ((std::uintptr_t)fp & 7) == 0 && (std::uintptr_t)fp > 0x10000) {
                frames[nf++] = fp[1];
                auto* next = reinterpret_cast<std::uint64_t*>(fp[0]);
                if (next <= fp || (std::uintptr_t)next - (std::uintptr_t)fp > (1u << 24)) break;
                fp = next;
            }
            for (unsigned i = 0; i < nf; ++i) {
                const Sym f = Resolve(frames[i]);
                Outf("[fpe]    #%u %s!%s+%llu (rel 0x%llx)\n", i, f.mod, f.name, (unsigned long long)f.off, (unsigned long long)f.rel);
            }
            { // operands of the trapped scalar insn (Rn / Rm as single and double), tells NaN-in (sNaN / loaded garbage) from NaN-made (0/0, sqrt(<0), inf-inf)
                const std::uint32_t insn = *reinterpret_cast<const std::uint32_t*>(pc);
                const unsigned rn = (insn >> 5) & 31, rm = (insn >> 16) & 31;
                auto* ns = &mc->__ns;
                float fn, fm; double dn, dm; std::memcpy(&fn, &ns->__v[rn], 4); std::memcpy(&fm, &ns->__v[rm], 4); std::memcpy(&dn, &ns->__v[rn], 8); std::memcpy(&dm, &ns->__v[rm], 8);
                std::uint32_t bn, bm; std::memcpy(&bn, &ns->__v[rn], 4); std::memcpy(&bm, &ns->__v[rm], 4);
                Outf("[fpe]    insn 0x%08x  Rn(v%u) = %g [0x%08x] / %g   Rm(v%u) = %g [0x%08x] / %g\n", insn, rn, (double)fn, bn, dn, rm, (double)fm, bm, dm);
            }
        }
    }
    if (g_abortMode) { AtExit(); ::abort(); }
    // the instruction is emulated (IEEE default result) and skipped; if it is not one of the emulated forms the thread runs it with the traps masked and the helper re-arms it
    const std::uint32_t insn = *reinterpret_cast<const std::uint32_t*>(pc);
    if (Emulate(uc, insn)) {
        __darwin_arm_thread_state64_set_pc_fptr(mc->__ss, reinterpret_cast<void*>(pc + 4));
        return;
    }
    if (site) { site->unemulated.fetch_add(1, std::memory_order_relaxed); }
    if (log) { Outf("[fpe]    (instruction 0x%08x not emulated: thread re-armed after a short pause)\n", insn); }
    mc->__ns.__fpcr &= ~(unsigned)(kIOE | kDZE | kOFE);
    g_dirty.store(true, std::memory_order_release);
}

void OnRearm(int, siginfo_t*, void* vuc) { // SIGUSR2 from the helper thread: set the trap bits again in the interrupted context
    auto* uc = static_cast<ucontext_t*>(vuc);
    uc->uc_mcontext->__ns.__fpcr |= (unsigned)g_enableMask;
}

void* HelperMain(void*) {
    unsigned tick = 0, rearmTicks = 0;
    for (;;) {
        ::usleep(1500);
        if (g_dirty.exchange(false, std::memory_order_acq_rel)) { rearmTicks = 3; } // 3 rounds: a SIGUSR2 that lands inside OnTrap itself is lost
        if (rearmTicks > 0) {
            --rearmTicks;
            const unsigned n = g_nThreads.load();
            for (unsigned i = 0; i < n && i < kMaxThreads; ++i) {
                if (const auto t = g_threads[i].id.load()) { ::pthread_kill(t, SIGUSR2); }
            }
        }
        if (++tick % 3000 == 0) { WriteSummaryFile(); }
    }
    return nullptr;
}
} // namespace

void InstallHandlers() {
    struct sigaction sa{};
    sa.sa_sigaction = OnTrap;
    sa.sa_flags     = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    struct sigaction cur{};
    ::sigaction(SIGILL, nullptr, &cur);
    if (cur.sa_sigaction != OnTrap) { ::sigaction(SIGILL, &sa, &g_prevIll); }
    ::sigaction(SIGFPE, nullptr, &cur);
    if (cur.sa_sigaction != OnTrap) { ::sigaction(SIGFPE, &sa, &g_prevFpe); }
}

void ArmCurrentThread(const char* name) {
    if (!g_inited.load()) return;
    const unsigned slot = g_nThreads.fetch_add(1);
    if (slot < kMaxThreads) {
        std::snprintf(g_threads[slot].name, sizeof g_threads[slot].name, "%s", name ? name : "?");
        g_threads[slot].id.store(::pthread_self());
    }
    WriteFpcr(ReadFpcr() | g_enableMask);
    Outf("[fpe] armed thread '%s' (fpcr=0x%llx)\n", name ? name : "?", (unsigned long long)ReadFpcr());
}

void DisarmCurrentThread() {
    const auto self = ::pthread_self();
    for (unsigned i = 0; i < kMaxThreads; ++i) {
        auto t = g_threads[i].id.load();
        if (t == self) { g_threads[i].id.compare_exchange_strong(t, nullptr); }
    }
    WriteFpcr(ReadFpcr() & ~(std::uint64_t)(kIOE | kDZE | kOFE));
}

void Init() {
    if (g_inited.exchange(true)) return;
    if (const char* m = std::getenv("NOTSA_FPE_MODE")) { g_abortMode = std::strcmp(m, "abort") == 0; }
    const char* exc = std::getenv("NOTSA_FPE_EXC");
    g_enableMask    = 0;
    for (const char* p = exc && *exc ? exc : "iz"; *p; ++p) {
        if (*p == 'i') g_enableMask |= kIOE;
        if (*p == 'z') g_enableMask |= kDZE;
        if (*p == 'o') g_enableMask |= kOFE;
    }
    if (const char* w = std::getenv("NOTSA_FPE_NOWHITELIST"); w && *w == '1') g_whitelist = false;
    if (const char* b = std::getenv("NOTSA_FPE_BT"); b && std::atoi(b) > 0) g_btDepth = (unsigned)std::atoi(b);
    if (const char* l = std::getenv("NOTSA_FPE_LOG"); l && *l) {
        const int fd = ::open(l, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) g_logFd = fd;
    }
    if (const char* s = std::getenv("NOTSA_FPE_SUMMARY"); s && *s) { std::snprintf(g_summaryPath, sizeof g_summaryPath, "%s", s); }

    InstallHandlers();
    struct sigaction ra{};
    ra.sa_sigaction = OnRearm;
    ra.sa_flags     = SA_SIGINFO | SA_RESTART;
    sigemptyset(&ra.sa_mask);
    ::sigaction(SIGUSR2, &ra, nullptr);
    std::atexit(AtExit);

    pthread_t helper;
    pthread_attr_t attr; ::pthread_attr_init(&attr); ::pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ::pthread_create(&helper, &attr, HelperMain, nullptr);

    Outf("[fpe] FP traps ON: mask 0x%llx (%s%s%s), mode %s, whitelist %s\n", (unsigned long long)g_enableMask, (g_enableMask & kIOE) ? "INVALID " : "", (g_enableMask & kDZE) ? "DIVBYZERO " : "",
         (g_enableMask & kOFE) ? "OVERFLOW" : "", g_abortMode ? "abort" : "log", g_whitelist ? "on" : "off");
    ArmCurrentThread("main");
}
} // namespace notsa::fpe
#endif
