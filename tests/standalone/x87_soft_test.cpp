// x87_soft_test.cpp - compares notsa::fp::soft (source/game_sa/Core/X87Soft.h) with the HOST x87 (inline asm, MSVC x86 only).
// The host FPU is the oracle: every case is run through the real instruction at precision control 24 / 53 / 64 and the full 80-bit
// result (fstp tbyte) is compared with the software one. No game headers, no CMake target; build by hand:
//   cl /nologo /O2 /arch:IA32 /EHsc /std:c++20 x87_soft_test.cpp        then run under Wine (Rosetta's x87) or on a real x86 machine:
//   x87_soft_test.exe [cases-per-op-and-precision (default 200000)] [seed] [ops filter: add,sqrt,sin,...]
// The exit code is non-zero if any case gives a different FLOAT (binary32) rounding of the result than the host, i.e. the practical criterion.
#include "../../source/game_sa/Core/X87Soft.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <deque>
#include <chrono>

using namespace notsa::fp::soft;

#if !(defined(_MSC_VER) && defined(_M_IX86))
#error "x87_soft_test.cpp needs MSVC x86 inline assembly (the host x87 is the oracle)"
#endif

// ---------------------------------------------------------------------------------------------------------------------------
// Host x87 oracle
// ---------------------------------------------------------------------------------------------------------------------------
namespace host {
struct Out { u8 a[10]; u8 b[10]; unsigned short sw; };

enum Op { ADD, SUB, MUL, DIV, SQRT, SIN, COS, TAN, SINCOS, PATAN, YL2X, YL2XP1, NOPS };

static void run(Op op, const u8* x, const u8* y, Out* out) {
    Out* o = out;
    unsigned short sw = 0;
    memset(o, 0, sizeof *o);
    switch (op) {
    case ADD:  __asm { mov eax, x
                       mov ecx, y
                       fnclex
                       fld tbyte ptr [eax]
                       fld tbyte ptr [ecx]
                       faddp st(1), st(0)
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx] } break;
    case SUB:  __asm { mov eax, x
                       mov ecx, y
                       fnclex
                       fld tbyte ptr [eax]
                       fld tbyte ptr [ecx]
                       fsubp st(1), st(0)
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx] } break;
    case MUL:  __asm { mov eax, x
                       mov ecx, y
                       fnclex
                       fld tbyte ptr [eax]
                       fld tbyte ptr [ecx]
                       fmulp st(1), st(0)
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx] } break;
    case DIV:  __asm { mov eax, x
                       mov ecx, y
                       fnclex
                       fld tbyte ptr [eax]
                       fld tbyte ptr [ecx]
                       fdivp st(1), st(0)
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx] } break;
    case SQRT: __asm { mov eax, x
                       fnclex
                       fld tbyte ptr [eax]
                       fsqrt
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx] } break;
    case SIN:  __asm { mov eax, x
                       fnclex
                       fld tbyte ptr [eax]
                       fsin
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx] } break;
    case COS:  __asm { mov eax, x
                       fnclex
                       fld tbyte ptr [eax]
                       fcos
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx] } break;
    case TAN:  __asm { mov eax, x
                       fnclex
                       fld tbyte ptr [eax]
                       fptan
                       fnstsw sw
                       mov edx, o
                       test sw, 0x400
                       jnz tan_c2
                       fstp tbyte ptr [edx + 10]       // the pushed 1.0
                       tan_c2:
                       fstp tbyte ptr [edx] } break;    // tan (or the unchanged operand when C2 is set: nothing was pushed)
    case SINCOS: __asm { mov eax, x
                       fnclex
                       fld tbyte ptr [eax]
                       fsincos
                       fnstsw sw
                       mov edx, o
                       test sw, 0x400
                       jnz sc_c2
                       fstp tbyte ptr [edx + 10]       // cos
                       sc_c2:
                       fstp tbyte ptr [edx] } break;   // sin (or the unchanged operand)
    case PATAN: __asm { mov eax, x
                       mov ecx, y
                       fnclex
                       fld tbyte ptr [eax]             // st1 = y
                       fld tbyte ptr [ecx]             // st0 = x
                       fpatan
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx] } break;
    case YL2X: __asm { mov eax, x
                       mov ecx, y
                       fnclex
                       fld tbyte ptr [eax]             // st1 = y
                       fld tbyte ptr [ecx]             // st0 = x
                       fyl2x
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx] } break;
    case YL2XP1: __asm { mov eax, x
                       mov ecx, y
                       fnclex
                       fld tbyte ptr [eax]
                       fld tbyte ptr [ecx]
                       fyl2xp1
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx] } break;
    default: break;
    }
    o->sw = sw;
}
static void setPC(int pc) { unsigned cw; _controlfp_s(&cw, pc == 24 ? _PC_24 : pc == 53 ? _PC_53 : _PC_64, _MCW_PC); }
} // namespace host

// ---------------------------------------------------------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------------------------------------------------------
static u64 g_rng = 0x9E3779B97F4A7C15ull;
static u64 rnd() { g_rng ^= g_rng >> 12; g_rng ^= g_rng << 25; g_rng ^= g_rng >> 27; return g_rng * 0x2545F4914F6CDD1Dull; }
static u32 rnd32() { return u32(rnd() >> 32); }
static double rndUnit() { return double(rnd() >> 11) * (1.0 / 9007199254740992.0); }       // [0,1) - the generator may use host FP, only the code under test may not

static F80 B(const u8* b) { return FromBytes(b); }
static void hex(const F80& a, char* s) { u8 b[10]; ToBytes(a, b); for (int i = 9; i >= 0; --i) { sprintf(s, "%02x", b[i]); s += 2; } *s = 0; }

// distance in extended ulps between two finite normal values of the same sign (else -1 meaning "not comparable")
static double ulpDist(const F80& a, const F80& b) {
    if (a.sign != b.sign || a.exp == 0 || b.exp == 0 || a.exp == 0x7FFF || b.exp == 0x7FFF) return -1;
    const F80& hi = (a.exp > b.exp || (a.exp == b.exp && a.mant >= b.mant)) ? a : b;
    const F80& lo = (&hi == &a) ? b : a;
    const u64 fh = hi.mant & 0x7FFFFFFFFFFFFFFFull, fl = lo.mant & 0x7FFFFFFFFFFFFFFFull;
    if (hi.exp == lo.exp) return double(fh - fl);
    if (hi.exp == lo.exp + 1) return double((u64(1) << 63) + fh - fl);
    return 1e18;
}

struct Stat {
    std::string name;
    int pc = 0;
    u64 n = 0, exact = 0, nanDiff = 0, f_ok = 0, d_ok = 0, c2diff = 0;
    double maxUlp = 0;
    u64 ulp1 = 0, ulp2p = 0;
    u64 firstBad = 0;
    char badMsg[400] = {};
    u64 fatal = 0;
};
static std::deque<Stat> g_stats;
static u64 g_fatalTotal = 0;
static int g_verbose = 0;

static void record(Stat& st, const F80& soft, const F80& ref, const char* what) {
    ++st.n;
    if (soft == ref) { ++st.exact; ++st.f_ok; ++st.d_ok; return; }
    const bool bothNaN = IsNaN(soft) && IsNaN(ref);
    if (bothNaN) {
        ++st.nanDiff; ++st.f_ok; ++st.d_ok;
        if (g_verbose >= 3 && st.firstBad < 8) { ++st.firstBad; char a[24], b[24]; hex(soft, a); hex(ref, b); printf("  NaN-diff %s pc%d %s soft=%s host=%s\n", st.name.c_str(), st.pc, what, a, b); }
        return;
    }
    const double ud = ulpDist(soft, ref);
    if (ud >= 0) { if (ud > st.maxUlp) st.maxUlp = ud; if (ud == 1) ++st.ulp1; else ++st.ulp2p; }
    const bool fok = ToFloatBits(soft) == ToFloatBits(ref);
    const bool dok = ToDoubleBits(soft) == ToDoubleBits(ref);
    if (fok) ++st.f_ok; else ++st.fatal;
    if (dok) ++st.d_ok;
    if ((!fok || (g_verbose && !dok) || g_verbose >= 3) && st.firstBad < 5) {
        ++st.firstBad;
        char a[24], b[24];
        hex(soft, a); hex(ref, b);
        printf("  MISMATCH %s pc%d %s soft=%s host=%s (%.1f ulp)\n", st.name.c_str(), st.pc, what, a, b, ud);
    }
}

static void report() {
    printf("\n%-14s %3s %10s %9s %9s %9s %9s %9s %8s %8s\n", "op/dist", "pc", "cases", "exact80%", "float%", "double%", "nan-only", "ulp1", "ulp2+", "maxulp");
    for (auto& s : g_stats) {
        if (!s.n) continue;
        printf("%-14s %3d %10llu %9.4f %9.5f %9.5f %9llu %9llu %8llu %8.1f%s\n", s.name.c_str(), s.pc, (unsigned long long)s.n,
               100.0 * s.exact / s.n, 100.0 * s.f_ok / s.n, 100.0 * s.d_ok / s.n, (unsigned long long)s.nanDiff,
               (unsigned long long)s.ulp1, (unsigned long long)s.ulp2p, s.maxUlp, s.fatal ? "  <-- FLOAT MISMATCHES" : "");
        if (s.name.find("info:") == std::string::npos) g_fatalTotal += s.fatal;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------
// Input generators (return F80 values)
// ---------------------------------------------------------------------------------------------------------------------------
static F80 mk(u64 mant, int exp, bool sign) { return F80{ mant, exp, sign }; }

// value kinds
enum Kind { K_FLOAT, K_DOUBLE, K_F80, K_WIDE, KINDS };
static const char* kindName[KINDS] = { "float", "double", "f80", "wide" };

static F80 genFloatE(int elo, int ehi) {                     // random float: sign, binary exponent in [elo, ehi], random mantissa
    const u32 e = u32(127 + elo + int(rnd() % u64(ehi - elo + 1)));
    return FromFloatBits((rnd32() & 0x80000000u) | (e << 23) | (rnd32() & 0x7FFFFF));
}
static F80 genDoubleE(int elo, int ehi) {                    // product of two floats rounded to double (what 'double' game code holds)
    const F80 p = Mul(genFloatE(elo / 2, ehi / 2), genFloatE(elo - elo / 2, ehi - ehi / 2), 53);
    return FromDoubleBits(ToDoubleBits(p));
}
static F80 gen80E(int elo, int ehi) {
    const int e = elo + int(rnd() % u64(ehi - elo + 1));
    return mk((u64(1) << 63) | (rnd() & 0x7FFFFFFFFFFFFFFFull), 16383 + e, (rnd() & 1) != 0);
}
static F80 genWide() {                                        // whole exponent range incl. denormal / near-overflow
    const int c = int(rnd() % 8);
    if (c == 0) { const int sh = int(rnd() % 64); return mk((rnd() | 1) >> sh, 0, (rnd() & 1) != 0); }          // denormal
    if (c == 1) return mk((u64(1) << 63) | (rnd() >> 1), 16383 - 16 + int(rnd() % 32), (rnd() & 1) != 0);          // around 2^+-16 of the extreme exponents
    const int e = (c & 1) ? 16383 - int(rnd() % 40) : 1 + int(rnd() % 40);
    return mk((u64(1) << 63) | (rnd() >> 1), (c == 2) ? 0x7FFE - int(rnd() % 40) : e, (rnd() & 1) != 0);
}
static F80 genK(int kind, int elo, int ehi) {
    switch (kind) {
    case K_FLOAT: return genFloatE(elo, ehi);
    case K_DOUBLE: return genDoubleE(elo, ehi);
    case K_F80: return gen80E(elo, ehi);
    default: return genWide();
    }
}

static std::vector<F80> specials() {
    std::vector<F80> v;
    const u64 INT = u64(1) << 63;
    const F80 base[] = {
        mk(0, 0, false), mk(1, 0, false), mk(0x7FFFFFFFFFFFFFFFull, 0, false), mk(INT, 0, false) /*pseudo-denormal*/, mk(INT, 1, false), mk(INT | 1, 1, false),
        mk(INT, 0x3FFF, false), mk(INT | 1, 0x3FFF, false), mk(~u64(0), 0x3FFE, false), mk(INT, 0x3FFE, false), mk(INT, 0x4000, false), mk(0xC000000000000000ull, 0x4000, false),
        mk(INT, 0x3FFF - 24, false), mk(INT, 0x3FFF + 24, false), mk(INT, 0x3FFF + 63, false), mk(INT, 0x3FFF + 64, false), mk(INT | 1, 0x3FFF + 63, false),
        mk(INT, 0x3FFF + 62, false), mk(0xC90FDAA22168C235ull, 0x4000, false), mk(0xC90FDAA22168C235ull, 0x3FFF, false), mk(0xC90FDAA22168C235ull, 0x3FFE, false),
        mk(~u64(0), 0x7FFE, false), mk(INT, 0x7FFE, false), mk(INT, 0x7FFF, false), mk(INT | 0x4000000000000000ull, 0x7FFF, false), mk(INT | 0x4000000000000001ull, 0x7FFF, false),
        mk(INT | 1, 0x7FFF, false), mk(INT | 0x2000000000000000ull, 0x7FFF, false), mk(INT | 0x4000000000000000ull | 0x2000000000000000ull, 0x7FFF, false),
        mk(INT | 0x3FFFFFFFFFFFFFFFull, 0x7FFF, false),
        mk(0, 0x7FFF, false) /*pseudo-inf*/, mk(0x4000000000000000ull, 0x7FFF, false) /*pseudo-NaN*/, mk(0x4000000000000000ull, 0x3FFF, false) /*unnormal*/, mk(1, 0x3FFF, false),
        mk(0, 0x3FFF, false), mk(INT, 0x3FFF - 64, false), mk(INT, 0x3FFF - 40, false), mk(INT, 2, false), mk(INT | 12345, 0x3C01, false), mk(INT, 0x4000 + 61, false),
        mk(0x9A209A84FBCFF799ull, 0x3FFD, false), mk(0xB17217F7D1CF79ACull, 0x3FFE, false), mk(0xB504F333F9DE6484ull, 0x3FFE, false), mk(0xB504F333F9DE6485ull, 0x3FFE, false),
        mk(0xA000000000000000ull, 0x3FFE, false), mk(0xD000000000000000ull, 0x3FFE, false),
    };
    for (const F80& b : base) { v.push_back(b); F80 n = b; n.sign = true; v.push_back(n); }
    return v;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Test drivers
// ---------------------------------------------------------------------------------------------------------------------------
typedef F80 (*Soft2)(const F80&, const F80&, int pc);
typedef F80 (*Soft1)(const F80&, int pc);

static Stat& stat(const std::string& name, int pc) {
    for (auto& s : g_stats) if (s.name == name && s.pc == pc) return s;
    g_stats.emplace_back(); g_stats.back().name = name; g_stats.back().pc = pc;
    return g_stats.back();
}

static F80 hostRes(host::Op op, const F80& a, const F80& b, host::Out& out) {
    u8 x[10], y[10];
    ToBytes(a, x); ToBytes(b, y);
    host::run(op, x, y, &out);
    return B(out.a);
}

static bool wanted(const char* filter, const char* op) {          // filter: comma separated exact op names (empty = all)
    if (!filter || !*filter) return true;
    const size_t n = strlen(op);
    for (const char* p = filter; *p; ) {
        const char* e = p; while (*e && *e != ',') ++e;
        if (size_t(e - p) == n && !strncmp(p, op, n)) return true;
        p = *e ? e + 1 : e;
    }
    return false;
}

static void testBinary(host::Op op, const char* name, Soft2 soft, u64 N, const char* filter) {
    if (!wanted(filter, name)) return;
    const std::vector<F80> sp = specials();
    for (int pc : { 24, 53, 64 }) {
        host::setPC(pc);
        // specials cross product
        {
            Stat& st = stat(std::string(name) + "/spec", pc);
            host::Out o;
            for (const F80& a : sp) for (const F80& b : sp) {
                const F80 ref = hostRes(op, a, b, o);
                const F80 got = soft(a, b, pc);
                if (g_verbose > 1 && !(ref == got) && (g_verbose > 2 || !(IsNaN(ref) && IsNaN(got))) && st.firstBad < 40) {
                    char h1[24], h2[24], h3[24], h4[24]; hex(a, h1); hex(b, h2); hex(got, h3); hex(ref, h4);
                    printf("  spec %s pc%d a=%s b=%s soft=%s host=%s\n", name, pc, h1, h2, h3, h4); ++st.firstBad;
                }
                record(st, got, ref, "spec");
            }
        }
        for (int kind = 0; kind < KINDS; ++kind) {
            Stat& st = stat(std::string(name) + "/" + kindName[kind], pc);
            host::Out o;
            for (u64 i = 0; i < N; ++i) {
                F80 a = genK(kind, -20, 20), b = genK(kind, -20, 20);
                switch (i & 7) {                       // shapes that stress the rounding / cancellation / alignment paths
                case 1: b = a; b.mant ^= rnd() & 0xFF; if (!(b.mant >> 63)) b.mant |= u64(1) << 63; break;                       // nearly equal
                case 2: b = a; b.sign = !b.sign; if (rnd() & 1) b.mant += (rnd() & 0xF) - 7; break;                               // x - x +- few ulp
                case 3: b = genK(kind, -80, 80); break;                                                                             // far apart
                case 4: a = genK(kind, -2, 2); b = genK(kind, -2, 2); break;
                default: break;
                }
                const F80 ref = hostRes(op, a, b, o);
                const F80 got = soft(a, b, pc);
                char what[96]; char ha[24], hb[24]; hex(a, ha); hex(b, hb); sprintf(what, "a=%s b=%s", ha, hb);
                record(st, got, ref, what);
            }
        }
    }
}

static void testUnary(host::Op op, const char* name, Soft1 soft, u64 N, const char* filter) {
    if (!wanted(filter, name)) return;
    const std::vector<F80> sp = specials();
    for (int pc : { 24, 53, 64 }) {
        host::setPC(pc);
        host::Out o;
        {
            Stat& st = stat(std::string(name) + "/spec", pc);
            for (const F80& a : sp) {
                const F80 ref = hostRes(op, a, a, o);
                const F80 got = soft(a, pc);
                if (g_verbose > 1 && !(ref == got) && (g_verbose > 2 || !(IsNaN(ref) && IsNaN(got))) && st.firstBad < 40) {
                    char h1[24], h3[24], h4[24]; hex(a, h1); hex(got, h3); hex(ref, h4);
                    printf("  spec %s pc%d a=%s soft=%s host=%s\n", name, pc, h1, h3, h4); ++st.firstBad;
                }
                record(st, got, ref, "spec");
            }
        }
        for (int kind = 0; kind < KINDS; ++kind) {
            Stat& st = stat(std::string(name) + "/" + kindName[kind], pc);
            for (u64 i = 0; i < N; ++i) {
                F80 a = genK(kind, -40, 40);
                a.sign = false; if (i & 1) a = genK(kind, -3, 3);
                if (kind == K_F80 && (i & 7) == 0) { a = genFloatE(-10, 10); a.sign = false; a = Mul(a, a, 64); }                 // perfect squares & friends
                const F80 ref = hostRes(op, a, a, o);
                const F80 got = soft(a, pc);
                char what[64]; sprintf(what, "i=%llu", (unsigned long long)i);
                record(st, got, ref, what);
            }
        }
    }
}

static F80 sAdd(const F80& a, const F80& b, int pc) { return Add(a, b, pc); }
static F80 sSub(const F80& a, const F80& b, int pc) { return Sub(a, b, pc); }
static F80 sMul(const F80& a, const F80& b, int pc) { return Mul(a, b, pc); }
static F80 sDiv(const F80& a, const F80& b, int pc) { return Div(a, b, pc); }
static F80 sSqrt(const F80& a, int pc) { return Sqrt(a, pc); }

// conversions: soft vs host fld/fstp of binary32/64
static void testConversions(u64 N) {
    Stat& s1 = stat("fld/fstp m64", 64);
    Stat& s2 = stat("fld/fstp m32", 64);
    host::setPC(64);
    for (u64 i = 0; i < N; ++i) {
        // double -> F80 -> double / float (the host does the same with fld qword / fstp qword / dword)
        u64 bits = rnd();
        if (i & 1) bits = (bits & 0x800FFFFFFFFFFFFFull) | (u64(rnd() % 2047) << 52);
        if ((i & 7) == 3) bits &= 0x800FFFFFFFFFFFFFull;                                  // denormal
        double d; memcpy(&d, &bits, 8);
        u8 w[10]; double back; float fl;
        unsigned short dummy;
        __asm { fld qword ptr [d]
                lea edx, w
                fstp tbyte ptr [edx] }
        F80 viaHost = FromBytes(w);
        F80 mine = FromDoubleBits(bits);
        record(s1, mine, viaHost, "fld m64");
        // narrowing from an arbitrary 80-bit value
        F80 v = (i & 1) ? genWide() : gen80E(-1100, 1100);
        u8 vb[10]; ToBytes(v, vb);
        __asm { lea eax, vb
                fld tbyte ptr [eax]
                fstp qword ptr [back]
                fld tbyte ptr [eax]
                fstp dword ptr [fl] }
        u64 hb; memcpy(&hb, &back, 8); u32 hf; memcpy(&hf, &fl, 4);
        ++s1.n; if (ToDoubleBits(v) == hb || (IsNaN(v))) ++s1.exact; else { ++s1.fatal; if (s1.firstBad++ < 5) { char a[24]; hex(v, a); printf("  MISMATCH fstp m64 v=%s soft=%016llx host=%016llx\n", a, (unsigned long long)ToDoubleBits(v), (unsigned long long)hb); } }
        ++s2.n; if (ToFloatBits(v) == hf || (IsNaN(v))) ++s2.exact; else { ++s2.fatal; if (s2.firstBad++ < 5) { char a[24]; hex(v, a); printf("  MISMATCH fstp m32 v=%s soft=%08x host=%08x\n", a, ToFloatBits(v), hf); } }
        // round-to-odd property: rounding the odd double to float == rounding the 80-bit value to float
        if (!IsNaN(v) && !IsInf(v)) {
            const double od = ToDoubleRoundToOdd(v);
            const F80 odF = FromDouble(od);
            ++s2.n; if (ToFloatBits(odF) == hf) ++s2.exact; else { ++s2.fatal; if (s2.firstBad++ < 5) { char a[24]; hex(v, a); printf("  MISMATCH round-to-odd v=%s\n", a); } }
        }
        (void)dummy;
    }
    s1.f_ok = s1.d_ok = s1.exact; s2.f_ok = s2.d_ok = s2.exact;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Transcendentals
// ---------------------------------------------------------------------------------------------------------------------------
struct SoftOut { F80 a, b; bool c2; bool two; };
typedef SoftOut (*SoftT)(const F80& x, const F80& y);     // y unused for unary functions

static F80 fromDoubleHost(double d) { return FromDouble(d); }

// a value generator per distribution; returns false for the unary 'second operand'
struct Dist { const char* name; F80 (*gen)(); };

static F80 gAng4piF()  { return FromFloat(float((rndUnit() * 2 - 1) * 12.566370614359172)); }
static F80 gAng4piD()  { return fromDoubleHost((rndUnit() * 2 - 1) * 12.566370614359172); }
static F80 gAng4piP()  { return genDoubleE(-4, 3); }                      // product of two floats
static F80 gR1e3F()    { return FromFloat(float((rndUnit() * 2 - 1) * 1000.0)); }
static F80 gR1e6F()    { return FromFloat(float((rndUnit() * 2 - 1) * 1.0e6)); }
static F80 gR1e6D()    { return fromDoubleHost((rndUnit() * 2 - 1) * 1.0e6); }
static F80 gSmall()    { return fromDoubleHost((rndUnit() * 2 - 1) * 1.0); }
static F80 gTiny()     { return gen80E(-80, -1); }
static F80 gE80()      { return gen80E(-70, 70); }
static F80 gHuge()     { return gen80E(50, 70); }
static F80 gNearPi2F() {                                                  // float / double nearest to k*pi/2 (+- few ulps)
    const int k = 1 + int(rnd() % 1000);
    const F80 v = Mul(FromInt64(k), kHalfPi, 64);
    if (rnd() & 1) { u32 b = ToFloatBits(v); b += u32(int(rnd() % 7) - 3); F80 r = FromFloatBits(b); r.sign = (rnd() & 1) != 0; return r; }
    u64 b = ToDoubleBits(v); b += u64(i64(int(rnd() % 7) - 3)); F80 r = FromDoubleBits(b); r.sign = (rnd() & 1) != 0; return r;
}
static F80 gNearPi2E() {                                                  // 80-bit values near k*pi/2 (k * the 80-bit constant, +- ulps)
    const int k = 1 + int(rnd() % 100000);
    F80 v = Mul(FromInt64(k), kHalfPi, 64);
    v.mant += u64(i64(int(rnd() % 9) - 4)); if (!(v.mant >> 63)) v.mant |= u64(1) << 63;
    v.sign = (rnd() & 1) != 0;
    return v;
}

static void runTrig(host::Op op, const char* name, SoftT soft, u64 N, const char* filter, const Dist* dists, int nd, bool needY = false) {
    if (!wanted(filter, name)) return;
    host::setPC(64);                                         // transcendental results ignore PC; the oracle runs at PC64 and PC24 (checked separately)
    for (int pass = 0; pass < 2; ++pass) {
        const int pc = pass == 0 ? 64 : 24;
        host::setPC(pc);
        host::Out o;
        for (int d = 0; d < nd; ++d) {
            Stat& st = stat(std::string(name) + "/" + dists[d].name, pc);
            Stat& st2 = stat(std::string(name) + "#2/" + dists[d].name, pc);
            const u64 n = pass == 0 ? N : N / 8;
            for (u64 i = 0; i < n; ++i) {
                const F80 x = dists[d].gen();
                const F80 y = needY ? dists[d].gen() : x;
                const F80 ref = hostRes(op, x, y, o);
                const SoftOut got = soft(x, y);
                char what[64], hx[24]; hex(x, hx); sprintf(what, "x=%s", hx);
                if (needY) { char hy[24]; hex(y, hy); sprintf(what, "y=%s x=%s", hx, hy); }
                if (g_verbose >= 4 && i < 4) { char h3[24], h4[24]; hex(got.a, h3); hex(ref, h4); printf("  dbg %s %s soft=%s host=%s\n", name, what, h3, h4); }
                record(st, got.a, ref, what);
                const bool hostC2 = (o.sw & 0x400) != 0;
                if (hostC2 != got.c2) { ++st.fatal; ++st.c2diff; if (st.firstBad++ < 5) printf("  C2 MISMATCH %s %s host=%d soft=%d\n", name, what, hostC2, got.c2); }
                if (got.two) { const F80 ref2 = B(o.b); record(st2, got.b, ref2, what); }
            }
        }
    }
}

static void runTrigSpecials(host::Op op, const char* name, SoftT soft, const char* filter, bool two) {
    if (!wanted(filter, name)) return;
    const std::vector<F80> sp = specials();
    host::Out o;
    for (int pc : { 64, 24 }) {
        host::setPC(pc);
        Stat& st = stat(std::string(name) + "/spec", pc);
        Stat& st2 = stat(std::string(name) + "#2/spec", pc);
        for (const F80& x : sp) for (const F80& y : sp) {
            if (!two && &y != &sp[0]) continue;
            if (op == host::YL2XP1 && y.sign && !IsNaN(y) && !IsZero(y) && y.exp >= 0x3FFF) continue;     // x <= -1: outside the instruction's domain (undefined; Rosetta returns garbage)
            const F80 ref = hostRes(op, x, y, o);
            const SoftOut got = soft(x, y);
            if (g_verbose > 1 && !(ref == got.a) && (g_verbose > 2 || !(IsNaN(ref) && IsNaN(got.a))) && st.firstBad < 40) {
                char h1[24], h2[24], h3[24], h4[24]; hex(x, h1); hex(y, h2); hex(got.a, h3); hex(ref, h4);
                printf("  spec %s pc%d x=%s y=%s soft=%s host=%s\n", name, pc, h1, h2, h3, h4); ++st.firstBad;
            }
            // fyl2xp1 with 1 + x == 1 (x != 0): the reference returns +-2^-135 noise or a zero of arbitrary sign -> informational statistic
            Stat* target = &st;
            if (op == host::YL2XP1 && !IsNaN(y) && !IsInf(y) && !IsZero(y) && Add(One(), y, 64) == One()) target = &stat(std::string(name) + "/info:1px==1 spec", pc);
            record(*target, got.a, ref, "spec");
            const bool hostC2 = (o.sw & 0x400) != 0;
            if (hostC2 != got.c2) { ++st.fatal; ++st.c2diff; if (st.firstBad++ < 5) printf("  C2 MISMATCH %s spec\n", name); }
            if (got.two) record(st2, got.b, B(o.b), "spec#2");
        }
    }
}

static SoftOut tSin(const F80& x, const F80&) { SoftOut r{}; r.a = Sin(x, &r.c2); return r; }
static SoftOut tCos(const F80& x, const F80&) { SoftOut r{}; r.a = Cos(x, &r.c2); return r; }

static SoftOut tTan(const F80& x, const F80&) { SoftOut r{}; const TanResult t = TanPush(x); r.a = t.tan; r.b = t.pushed; r.c2 = t.c2; r.two = !t.c2; return r; }
static SoftOut tSinCos(const F80& x, const F80&) { SoftOut r{}; const SinCosResult s = SinCos(x); r.a = s.sin; r.b = s.cos; r.c2 = s.c2; r.two = !s.c2; return r; }
static SoftOut tPatan(const F80& y, const F80& x) { SoftOut r{}; r.a = Atan2(y, x); return r; }
static SoftOut tYl2x(const F80& y, const F80& x) { SoftOut r{}; r.a = Yl2x(y, x); return r; }
static SoftOut tYl2xp1(const F80& y, const F80& x) { SoftOut r{}; r.a = Yl2xp1(y, x); return r; }

static const Dist kTrigDists[] = {
    { "ang4pi_f", gAng4piF }, { "ang4pi_d", gAng4piD }, { "ang4pi_p", gAng4piP }, { "r1e3_f", gR1e3F }, { "r1e6_f", gR1e6F }, { "r1e6_d", gR1e6D },
    { "small_d", gSmall }, { "tiny80", gTiny }, { "e80", gE80 }, { "huge80", gHuge }, { "nearpi2_fd", gNearPi2F }, { "nearpi2_80", gNearPi2E },
};


// two-operand distributions (y, x)
struct DistP { const char* name; void (*gen)(F80& y, F80& x); };
static F80 fl(double lo, double hi) { return FromFloat(float(lo + (hi - lo) * rndUnit())); }
static void pPairF(F80& y, F80& x)    { y = fl(-100, 100); x = fl(-100, 100); }
static void pPairMag(F80& y, F80& x)  { y = genFloatE(-14, 14); x = genFloatE(-14, 14); }
static void pPairD(F80& y, F80& x)    { y = genDoubleE(-10, 10); x = genDoubleE(-10, 10); }
static void pPairE(F80& y, F80& x)    { y = gen80E(-30, 30); x = gen80E(-30, 30); }
static void pClose(F80& y, F80& x)    { x = genFloatE(-6, 6); y = x; if (rnd() & 1) y.sign = !y.sign; y.mant += u64(rnd() % 5); if (rnd() & 1) x.sign = !x.sign; }
static void pFar(F80& y, F80& x)      { y = genK(K_F80, -60, 60); x = genK(K_F80, -60, 60); if (rnd() & 1) y = genK(K_F80, -6000, 6000); }
static void pAtan1(F80& y, F80& x)    { y = genK(rnd() & 1 ? K_FLOAT : K_DOUBLE, -12, 12); x = One(); }
static void pAxis(F80& y, F80& x)     { y = genFloatE(-8, 8); x = Mul(y, FromFloat(float(0.0009765625 * int(rnd() % 2000)) - 1.0f), 24); if (rnd() & 1) { F80 t = x; x = y; y = t; } }
static void pSmallX(F80& y, F80& x)   { y = genK(K_F80, -80, -1); x = genK(K_F80, -1, 20); }
static F80 yconst() { switch (rnd() % 4) { case 0: return One(); case 1: return kLg2; case 2: return kLn2; default: return genFloatE(-4, 4); } }
static void pLogF(F80& y, F80& x)     { y = yconst(); x = fl(0.0, 1.0e6); }
static void pLogDec(F80& y, F80& x)   { y = yconst(); x = genFloatE(-30, 30); x.sign = false; }
static void pLogD(F80& y, F80& x)     { y = yconst(); x = genDoubleE(-20, 20); x.sign = false; }
static void pLogE(F80& y, F80& x)     { y = yconst(); x = gen80E(-40, 40); x.sign = false; }
static void pLogNear1(F80& y, F80& x) { y = yconst(); x = One(); x.mant += u64(i64(int(rnd() % 4001)) - 2000) << (rnd() % 40); if (!(x.mant >> 63)) { x.exp = 0x3FFE; x.mant = ~(rnd() >> 20); x.mant |= u64(1) << 63; } }
static void pLogHuge(F80& y, F80& x)  { y = yconst(); x = genK(K_WIDE, 0, 0); x.sign = false; }
static void pLogP1(F80& y, F80& x)    { y = yconst(); x = fl(-0.29, 0.41); }
static void pLogP1S(F80& y, F80& x)   { y = yconst(); x = genK(K_F80, -64, -4); }
static void pLogP1T(F80& y, F80& x)   { y = yconst(); x = genK(K_F80, -100, -65); }      // 1 + x == 1: the reference returns +-2^-135 style noise instead of 0

static void runPair(host::Op op, const char* name, SoftT soft, u64 N, const char* filter, const DistP* dists, int nd) {
    if (!wanted(filter, name)) return;
    for (int pass = 0; pass < 2; ++pass) {
        const int pc = pass == 0 ? 64 : 24;
        host::setPC(pc);
        host::Out o;
        for (int d = 0; d < nd; ++d) {
            Stat& st = stat(std::string(name) + "/" + dists[d].name, pc);
            const u64 n = pass == 0 ? N : N / 8;
            for (u64 i = 0; i < n; ++i) {
                F80 y, x; dists[d].gen(y, x);
                const F80 ref = hostRes(op, y, x, o);
                const SoftOut got = soft(y, x);
                char what[96], hx[24], hy[24]; hex(x, hx); hex(y, hy); sprintf(what, "y=%s x=%s", hy, hx);
                record(st, got.a, ref, what);
            }
        }
    }
}

static const DistP kPatanDists[] = { { "pair_f", pPairF }, { "mag_f", pPairMag }, { "pair_d", pPairD }, { "pair_e80", pPairE }, { "close", pClose }, { "far", pFar }, { "atan1", pAtan1 }, { "axis", pAxis }, { "smallx", pSmallX } };
static const DistP kYl2xDists[] = { { "x_f", pLogF }, { "x_dec", pLogDec }, { "x_d", pLogD }, { "x_e80", pLogE }, { "near1", pLogNear1 }, { "wide", pLogHuge } };
static const DistP kYl2xp1Dists[] = { { "x_f", pLogP1 }, { "small", pLogP1S }, { "info:1px==1", pLogP1T } };

int main(int argc, char** argv) {
    const u64 N = argc > 1 ? strtoull(argv[1], 0, 10) : 200000;
    if (argc > 2) g_rng += strtoull(argv[2], 0, 10) * 0x9E3779B97F4A7C15ull;
    if (!g_rng) g_rng = 1;
    const char* filter = argc > 3 ? argv[3] : "";
    g_verbose = getenv("X87T_VERBOSE") ? atoi(getenv("X87T_VERBOSE")) : 0;
    const auto t0 = std::chrono::steady_clock::now();

    if (wanted(filter, "conv")) testConversions(N);
    testBinary(host::ADD, "add", sAdd, N, filter);
    testBinary(host::SUB, "sub", sSub, N, filter);
    testBinary(host::MUL, "mul", sMul, N, filter);
    testBinary(host::DIV, "div", sDiv, N, filter);
    testUnary(host::SQRT, "sqrt", sSqrt, N, filter);
    runTrigSpecials(host::SIN, "sin", tSin, filter, false);
    runTrig(host::SIN, "sin", tSin, N, filter, kTrigDists, sizeof kTrigDists / sizeof *kTrigDists);
    runTrigSpecials(host::COS, "cos", tCos, filter, false);
    runTrig(host::COS, "cos", tCos, N, filter, kTrigDists, sizeof kTrigDists / sizeof *kTrigDists);
    runTrigSpecials(host::TAN, "tan", tTan, filter, false);
    runTrig(host::TAN, "tan", tTan, N, filter, kTrigDists, sizeof kTrigDists / sizeof *kTrigDists);
    runTrigSpecials(host::SINCOS, "sincos", tSinCos, filter, false);
    runTrig(host::SINCOS, "sincos", tSinCos, N / 4, filter, kTrigDists, sizeof kTrigDists / sizeof *kTrigDists);
    runTrigSpecials(host::PATAN, "patan", tPatan, filter, true);
    runPair(host::PATAN, "patan", tPatan, N, filter, kPatanDists, sizeof kPatanDists / sizeof *kPatanDists);
    runTrigSpecials(host::YL2X, "yl2x", tYl2x, filter, true);
    runPair(host::YL2X, "yl2x", tYl2x, N, filter, kYl2xDists, sizeof kYl2xDists / sizeof *kYl2xDists);
    runTrigSpecials(host::YL2XP1, "yl2xp1", tYl2xp1, filter, true);
    runPair(host::YL2XP1, "yl2xp1", tYl2xp1, N, filter, kYl2xp1Dists, sizeof kYl2xp1Dists / sizeof *kYl2xp1Dists);

    report();
    printf("\nelapsed %.1f s, fatal float mismatches: %llu\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), (unsigned long long)g_fatalTotal);
    return g_fatalTotal ? 1 : 0;
}
