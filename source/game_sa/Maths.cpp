#include "StdInc.h"

#include "Maths.h"

#include "Fx/FxFtol.h"

// We generate the lookup table here (static init) instead of in `InitMathsTables`.
// Original table is located at 0xBB3E00 (NOT `0xBB3DFC`: the exe's loop writes `[i*4 + 0xBB3DFC]` after incrementing i) and is of size 256 (so don't change the size below)
//
// The exe (0x59AC90) computes `fsin(fild(i) * 0x3CC90FDB)` entirely on the x87 stack (the product is NOT rounded to float, PC=53 at that time)
// and spills only the sine to float. `std::sin` of a float product differs by up to a few ULP, so this is done with the same x87 sequence.
constexpr size_t SIN_LUT_SIZE = 256;
static_assert(std::bit_cast<float>(0x3CC90FDBu) == 0x1.921fb6p-6f);
static float SinTableEntry(int32 i) {
    static const float step = std::bit_cast<float>(0x3CC90FDBu); // 0x859BBC
    float res;
    __asm {
        fild  i
        fmul  step
        fsin
        fstp  res
    }
    return res;
}

const auto& SIN_LUT = StaticRef<std::array<float, SIN_LUT_SIZE>>(0xBB3E00) = []() {
    std::array<float, SIN_LUT_SIZE> lut{};
    for (int32 i = 0; i < (int32)SIN_LUT_SIZE; ++i) {
        lut[i] = SinTableEntry(i);
    }
    return lut;
}();

// 0x4A1340
float CMaths::GetSinFast(float rad) {
    // exe: `fld rad; fmul 40.7436637878418f (0x85A778); _ftol2; and eax, 0xFF` (the product stays in extended precision)
    return SIN_LUT[(uint32)notsa::detail::Ftol((double)rad * (double)40.7436637878418f) & (SIN_LUT_SIZE - 1)];
}

// 0x4A1360
float CMaths::GetCosFast(float rad) {
    // exe: `fld rad; fmul 40.7436637878418f; fadd 64.0f (0x859A44); _ftol2; and eax, 0xFF`
    return SIN_LUT[(uint32)notsa::detail::Ftol((double)rad * (double)40.7436637878418f + 64.0) & (SIN_LUT_SIZE - 1)];
}

void CMaths::InitMathsTables() {
    ZoneScoped;

    /* No-op: The LUT is already populated by the static initializer above (originally it was
     * populated here at runtime using the MSVC 2004 CRT sin(), which differs from std::sin by
     * ~1-5 ULP per entry). See the SIN_LUT comment above for details. */
}

void CMaths::InjectHooks() {
    RH_ScopedClass(CMaths);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(InitMathsTables, 0x59AC90);
    RH_ScopedInstall(GetSinFast, 0x4A1340); // No callers in the game, inlined in every single instance
    RH_ScopedInstall(GetCosFast, 0x4A1360); // No callers in the game, inlined in every single instance
}
