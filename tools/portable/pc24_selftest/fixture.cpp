// pc24_scan.py self-test fixture (tools/portable/pc24_selftest/run.sh): every function is a case the tool must handle; expectations are asserted in run.sh.
#include <cmath>
#include <cstdint>
#ifdef NOTSA_PORTABLE
typedef float x87d;
#else
typedef double x87d;
#endif
#define X87R(...) ((x87d)(__VA_ARGS__))
#ifdef NOTSA_PORTABLE
#define X87W(...) ((double)(__VA_ARGS__))
#else
#define X87W(...) __VA_ARGS__
#endif
#define ARGM(a, b) ((a) + (b))
#define BODYM (1.5f * 2.5f)
template <class T> uintptr_t AddrOf(T& x) { return (uintptr_t)&x; }
void sink(double&);
void sink_const(const double&);
extern double dglob;

float escape_case(float a, float b) {
    double esc = (double)a * (double)b;       // CASE esc: bound to a non-const reference below -> must stay double
    sink(esc);
    double viaAddr = (double)a + (double)b;   // CASE viaAddr: AddrOf (lifted asm style) -> must stay double
    uintptr_t u = AddrOf(viaAddr);
    double loc = (double)a * (double)b;       // CASE loc: clean local -> x87d
    sink_const(loc);                          // a const reference does not make it dirty
    return (float)(loc) + (float)u + (float)esc;
}

float math_case(float a, float b) {
    return (float)(std::log10((double)a * (double)b)); // CASE log10: overload would change -> X87W
}

float macro_case(float a, float b) {
    return (float)(((double)a + (double)ARGM(a, b)) * 2.0); // CASE macro arg: the cast of a macro expansion is rewritten
}

float exact_case(float a) {
    const double ad = std::fabs(dglob);       // CASE fabs: no rounding op, a 53 bit operand stays double
    return (float)(ad * (double)a);           // CASE dirty: ad is not clean -> X87R
}

float wide_case(int32_t big, float a) {
    return (float)((double)big * (double)a);  // CASE wide: an int that can exceed 2^24 is reported
}

float pc53_case(float a) {
    const double t = (double)a * 3.0; // pc53
    return (float)t;
}
