#include "StdInc.h"

#include "FxInfo.h"
#include "FxManager.h"

void* FxInfo_c::operator new(size_t size) {
    return g_fxMan.GetMemPool().GetMem((int32)size, 4);
}

FxInfo_c::~FxInfo_c() {
    // NOP
}
