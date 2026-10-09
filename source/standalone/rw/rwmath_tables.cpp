// 01r: the exe's RW 3.6 approximate sqrt / inverse sqrt (table based, relative error ~1e-4), exported for every shim TU that needs them
// (RwV3dLength / RwV3dNormalize / RwMatrixOrthoNormalize / RwMatrixRotate in math.cpp; rtquat.cpp: RtQuatConvertFromMatrix 0x7EB5C0,
// RtQuatRotate 0x7EB7C0, RtQuatSetupSlerpCache 0x7EC220 use the same helpers). Implementation + provenance: rwmath_exact.h.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include "rwmath_exact.h"

// 0x7EDB30
RwReal _rwSqrt(const RwReal num) { return rwx::Sqrt(num); }
// 0x7EDB90
RwReal _rwInvSqrt(const RwReal num) { return rwx::InvSqrt(num); }
// engine init 0x7EDE90 built the tables; here they are built on first use (same values); this forces the build
void _rwSqrtInit() { (void)rwx::Tables(); }
#endif
