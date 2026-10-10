#pragma once

//! The exe's `x / N` for a literal N is `x * (float 1/N)`, the reciprocal constant (1/N rounded to float) sitting in .rdata; an inline
//! `x * (1.f / N)` is folded by MSVC (x87) into an extended-precision constant and `x / N` is compiled as a real division: both differ from the
//! exe in the last bit. This yields the exact float constant at compile time (consteval: never evaluated at runtime, whatever the optimisation level).
consteval float ExeRecip(float n) { return 1.0f / n; }
