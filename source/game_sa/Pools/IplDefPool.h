#pragma once

#include "Pool.h"
#include <IplDef.h>

class CIplDefPool : public CPool<IplDef> {
public:
    static void InjectHooks() {
        RH_ScopedClass(CIplDefPool);
        RH_ScopedCategory("Pools");

#ifndef NOTSA_NO_HOOK_REGISTRY // raw rh->, not RH_ScopedInstall: keeps the DLL object identical (source_location column and line)
        rh->InstallStatic(RHCurrentCat.name + "/" + RHCurrentScopeName.name, "New", 0x004059B0, &RHCurrentNS::New);
#endif
    }

public:
    using CPool::CPool;
};
