// (no StdInc.h: also built by the PCH-less unit test tests/standalone/rw_engine_test.cpp; under the game target the PCH is force-included)
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
// Storage for the RW engine globals the game reads (declared in source/fakerw/rwextra.h). Filled by the shim: RwEngineInit (stringFuncs),
// RwCameraBeginUpdate (curCamera, renderFrame), RwEngineStart (dOpenDevice z range).
static RwGlobals s_RwGlobals{};
RwGlobals* RwEngineInstance = &s_RwGlobals;
bool       RwInitialized    = false;
RtDictSchema RpUVAnimDictSchema{};
RwRGBAReal   AmbientSaturated{};
#endif
