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
                       fstp tbyte ptr [edx + 10]       // the pushed 1.0 (or the unchanged operand when C2 is set)
                       fstp tbyte ptr [edx] } break;
    case SINCOS: __asm { mov eax, x
                       fnclex
                       fld tbyte ptr [eax]
                       fsincos
                       fnstsw sw
                       mov edx, o
                       fstp tbyte ptr [edx + 10]       // cos
                       fstp tbyte ptr [edx] } break;   // sin
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
        g_fatalTotal += s.fatal;
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

static bool wanted(const char* filter, const char* op) { return !filter || !*filter || strstr(filter, op) != nullptr; }

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

int main(int argc, char** argv) {
    const u64 N = argc > 1 ? strtoull(argv[1], 0, 10) : 200000;
    if (argc > 2) g_rng ^= strtoull(argv[2], 0, 10) * 0x9E3779B97F4A7C15ull;
    const char* filter = argc > 3 ? argv[3] : "";
    g_verbose = getenv("X87T_VERBOSE") ? atoi(getenv("X87T_VERBOSE")) : 0;
    const auto t0 = std::chrono::steady_clock::now();

    if (wanted(filter, "conv")) testConversions(N);
    testBinary(host::ADD, "add", sAdd, N, filter);
    testBinary(host::SUB, "sub", sSub, N, filter);
    testBinary(host::MUL, "mul", sMul, N, filter);
    testBinary(host::DIV, "div", sDiv, N, filter);
    testUnary(host::SQRT, "sqrt", sSqrt, N, filter);

    report();
    printf("\nelapsed %.1f s, fatal float mismatches: %llu\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), (unsigned long long)g_fatalTotal);
    return g_fatalTotal ? 1 : 0;
}
