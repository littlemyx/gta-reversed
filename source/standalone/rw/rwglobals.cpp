#include "StdInc.h"

#ifdef NOTSA_RW_LIBRW
// Storage for the RW engine globals the game reads (declared in source/fakerw/rwextra.h). Filled by the shim: RwEngineInit (stringFuncs),
// RwCameraBeginUpdate (curCamera, renderFrame), RwEngineStart (dOpenDevice z range).
static RwGlobals s_RwGlobals{};
RwGlobals* RwEngineInstance = &s_RwGlobals;
bool       RwInitialized    = false;
RtDictSchema RpUVAnimDictSchema{};
RwRGBAReal   AmbientSaturated{};
#endif
