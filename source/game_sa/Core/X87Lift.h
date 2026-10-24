#pragma once
// X87Lift.h - a small x87 FPU register stack + a 32 bit integer register file for the portable (arm64 / x64 / clang) builds.
//
// The exe's x87 sequences that the Win32 build keeps as MSVC inline assembly (CMatrix::SetRotate / Rotate* / ConvertTo/FromEulerAngles, CQuaternion::Slerp & co,
// CCover::FindVectorFromDir, ...) are lifted instruction by instruction (tools/portable/x87lift.py) into straight C++ over this machine:
//   * `Fpu`  - 8-slot stack of notsa::fp::soft::F80 (X87Soft.h), every add / sub / mul / div / sqrt rounded at the precision control `pc` (24 in game mode),
//              fsin / fcos / fpatan / fyl2x return the unrounded 64 bit result on the stack exactly like the instruction; fcom* + fnstsw give the condition bits.
//   * `Cpu`  - eax..edi / esp as uintptr_t (native pointers, so `lea reg, [esp + ...]` and `[ecx + 0x30]` just work), a private 1 KB stack, ZF / PF / CF flags
//              for the few conditional jumps. The stack lives at the VIRTUAL addresses 0x10000..0x10400 (so a stack address stored into a 4 byte slot survives, like on x86),
//              POINTER arguments never go through the 4-byte stack slots either: they are passed as small HANDLES (h * 256, 0 = null). `Cpu::At` maps both back
//              (an address below 0x800 is a handle window, 0x10000..0x10400 the stack, anything else a native address: this / locals / constant tables).
// Nothing here uses the host FPU: the results are bit-identical on every host (the soft x87 is integer-only).
#include "X87Soft.h"
#include <cstring>

namespace notsa::fp::lift {
using namespace notsa::fp::soft;

template <class T> inline uintptr_t AddrOf(T& x) { return reinterpret_cast<uintptr_t>(&x); }   // address of a local / array the lifted asm names

enum class Op { Add, Sub, SubR, Mul, Div, DivR };

class Fpu {
public:
    explicit Fpu(int pc = 24) : pc_(pc) {}

    // ---- loads / constants
    void Ld(const F80& v) { Push(v); }
    void LdF32(float v) { Push(FromFloat(v)); }
    void LdF64(double v) { Push(FromDouble(v)); }
    void LdI32(int32_t v) { Push(FromInt64(v)); }
    void Ld1() { Push(One()); }
    void Ldz() { Push(Zero()); }
    void Ldpi() { Push(kPi); }
    void Ldlg2() { Push(kLg2); }
    void Ldln2() { Push(kLn2); }
    void LdSt(int i) { const F80 v = St(i); Push(v); }                 // fld st(i)

    // ---- stores
    float StF32() const { return ToFloat(St(0)); }
    double StF64() const { return ToDouble(St(0)); }
    float StpF32() { const float r = ToFloat(St(0)); Pop(); return r; }
    double StpF64() { const double r = ToDouble(St(0)); Pop(); return r; }
    void StSt(int i) { St(i) = St(0); }                                // fst st(i)
    void StpSt(int i) { St(i) = St(0); Pop(); }                        // fstp st(i)  (i == 0: just pop)

    void Xch(int i) { const F80 t = St(0); St(0) = St(i); St(i) = t; }
    void Chs() { St(0) = Neg(St(0)); }

    // ---- arithmetic. dst op= src, the result of the op is rounded at the precision control. `pop` = the ...p forms
    void Arith(Op op, int dst, const F80& src, bool pop = false) {
        const F80 d = St(dst);
        F80 r;
        switch (op) {
        case Op::Add:  r = Add(d, src, pc_); break;
        case Op::Sub:  r = Sub(d, src, pc_); break;
        case Op::SubR: r = Sub(src, d, pc_); break;
        case Op::Mul:  r = Mul(d, src, pc_); break;
        case Op::Div:  r = Div(d, src, pc_); break;
        default:       r = Div(src, d, pc_); break;
        }
        St(dst) = r;
        if (pop) Pop();
    }
    void ArithF32(Op op, float v) { Arith(op, 0, FromFloat(v)); }
    void ArithF64(Op op, double v) { Arith(op, 0, FromDouble(v)); }
    void ArithSt(Op op, int dst, int src, bool pop = false) { Arith(op, dst, St(src), pop); }

    void Sqrt() { St(0) = soft::Sqrt(St(0), pc_); }
    void Sin() { bool c2; St(0) = soft::Sin(St(0), &c2); SetC2(c2); }
    void Cos() { bool c2; St(0) = soft::Cos(St(0), &c2); SetC2(c2); }
    void Patan() { St(1) = Atan2(St(1), St(0)); Pop(); }               // st(1) = atan2(st(1), st(0)); pop
    void Yl2x() { St(1) = soft::Yl2x(St(1), St(0)); Pop(); }           // st(1) = st(1) * log2(st(0)); pop

    // ---- compare: fcom / fcomp / fcompp  (ST(0) vs src), condition bits C3 C2 C0 into the status word
    void Com(const F80& src, int pops = 0) {
        const F80 a = St(0);
        sw_ &= uint16_t(~(kC0 | kC2 | kC3));
        const Class ca = Classify(a), cb = Classify(src);
        const bool nan = ca == Class::QNaN || ca == Class::SNaN || ca == Class::Unsupported || cb == Class::QNaN || cb == Class::SNaN || cb == Class::Unsupported;
        if (nan) sw_ |= kC0 | kC2 | kC3;
        else {
            const int c = Cmp(a, src);
            if (c < 0) sw_ |= kC0; else if (c == 0) sw_ |= kC3;
        }
        for (int k = 0; k < pops; ++k) Pop();
    }
    void ComF32(float v, int pops = 0) { Com(FromFloat(v), pops); }
    void ComSt(int i, int pops = 0) { Com(St(i), pops); }
    uint16_t Sw() const { return sw_; }                                // fnstsw (only C0 / C2 / C3 are modelled; TOP reads as 0)

    const F80& Top(int i = 0) const { return St(i); }

private:
    static constexpr uint16_t kC0 = 0x0100, kC2 = 0x0400, kC3 = 0x4000;
    F80& St(int i) { return s_[(top_ + i) & 7]; }
    const F80& St(int i) const { return s_[(top_ + i) & 7]; }
    void Push(const F80& v) { top_ = (top_ - 1) & 7; s_[top_] = v; }
    void Pop() { top_ = (top_ + 1) & 7; }
    void SetC2(bool c2) { if (c2) sw_ |= kC2; else sw_ &= uint16_t(~kC2); }
    // magnitude compare of finite / inf values: -1, 0, 1
    static int Cmp(const F80& a, const F80& b) {
        const bool za = IsZero(a), zb = IsZero(b);
        if (za && zb) return 0;
        const bool sa = za ? false : a.sign, sb = zb ? false : b.sign;
        if (sa != sb) return sa ? -1 : 1;
        int m;   // |a| vs |b|
        const i32 ea = a.exp ? a.exp : 1, eb = b.exp ? b.exp : 1;   // denormals behave as exp 1
        if (za) m = -1; else if (zb) m = 1;
        else if (ea != eb) m = ea < eb ? -1 : 1;
        else if (a.mant != b.mant) m = a.mant < b.mant ? -1 : 1;
        else m = 0;
        return sa ? -m : m;
    }
    F80 s_[8]{};
    int top_ = 0;
    int pc_;
    uint16_t sw_ = 0;
};

// 32 bit register file + private stack + the flags the lifted code branches on
struct Cpu {
    uintptr_t eax = 0, ebx = 0, ecx = 0, edx = 0, esi = 0, edi = 0, ebp = 0, esp = 0;
    bool zf = false, pf = false, cf = false;
    Fpu f;
    uintptr_t handles[8]{};                  // handle h (1..7) -> real pointer
    alignas(16) uint8_t stk[1024]{};

    explicit Cpu(int pc = 24) : f(pc) { esp = kStackBase + sizeof(stk) - 16; }

    // handle <-> pointer: argument pointers are pushed as HANDLES, virtual addresses h * 256 (h = 1..7, 0 = null) that stand for a 256 byte window starting at the real pointer, so
    // `[handle + 4]` works like a pointer offset; Cpu::At maps them back
    uint32_t Handle(void* p) {
        if (!p) return 0;
        for (uint32_t h = 1; h < 8; ++h) if (handles[h] == 0 || handles[h] == reinterpret_cast<uintptr_t>(p)) { handles[h] = reinterpret_cast<uintptr_t>(p); return h << 8; }
        return 0;
    }
    static constexpr uintptr_t kStackBase = 0x10000;
    uint8_t* At(uintptr_t a) {
        if (a < 0x800) return reinterpret_cast<uint8_t*>(handles[a >> 8] + (a & 0xFF));
        if (a - kStackBase < sizeof(stk)) return stk + (a - kStackBase);
        return reinterpret_cast<uint8_t*>(a);
    }

    uint32_t R32(uintptr_t a) { uint32_t v; std::memcpy(&v, At(a), 4); return v; }
    uint64_t R64(uintptr_t a) { uint64_t v; std::memcpy(&v, At(a), 8); return v; }
    uint8_t  R8(uintptr_t a) { return *At(a); }
    void W32(uintptr_t a, uint32_t v) { std::memcpy(At(a), &v, 4); }
    void W16(uintptr_t a, uint16_t v) { std::memcpy(At(a), &v, 2); }
    float    RF32(uintptr_t a) { return std::bit_cast<float>(R32(a)); }
    double   RF64(uintptr_t a) { return std::bit_cast<double>(R64(a)); }
    void WF32(uintptr_t a, float v) { W32(a, std::bit_cast<uint32_t>(v)); }
    void WF64(uintptr_t a, double v) { const uint64_t b = std::bit_cast<uint64_t>(v); std::memcpy(At(a), &b, 8); }

    void Push(uintptr_t v) { esp -= 4; W32(esp, uint32_t(v)); }
    uintptr_t Pop() { const uint32_t v = R32(esp); esp += 4; return v; }

    static bool Parity(uint32_t v) { v &= 0xFF; v ^= v >> 4; v ^= v >> 2; v ^= v >> 1; return !(v & 1); }   // PF = even number of set bits in the low byte
    void SetZs(uintptr_t r) { zf = (r == 0); pf = Parity(uint32_t(r)); }          // full width: a register may hold a native pointer
    void Cmp(uintptr_t a, uintptr_t b) { zf = (a == b); cf = (a < b); pf = Parity(uint32_t(a) - uint32_t(b)); }
    void Test(uintptr_t a, uintptr_t b) { SetZs(a & b); cf = false; }
    void FnstswAx() { eax = (eax & ~uintptr_t(0xFFFF)) | f.Sw(); }
    uint32_t Ah() const { return uint32_t(eax >> 8) & 0xFF; }
};

} // namespace notsa::fp::lift
