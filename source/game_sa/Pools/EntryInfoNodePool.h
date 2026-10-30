#pragma once

#include "Pool.h"
#include "EntryInfoNode.h"

class CEntryInfoNodePool : public CPool<CEntryInfoNode> {
public:
    static void InjectHooks() {
        RH_ScopedClass(CEntryInfoNodePool);
        RH_ScopedCategory("Pools");

        RH_ScopedInstall(New, 0x00536D10); // the exe function is the pool allocator (tail-called by CEntryInfoNode::operator new 0x536DC0), it was hooked as GetAt(idx)
    }

public:
    using CPool::CPool;
};
