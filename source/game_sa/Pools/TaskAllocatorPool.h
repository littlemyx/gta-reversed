#pragma once

#include "Pool.h"
#include "Allocators/TaskAllocator.h"

#ifdef NOTSA_PORTABLE
// STAB2: CTaskAllocatorPlayerCommandAttack / CTaskAllocatorAttack are 48 bytes on LP64 (32 on x86): a 32 byte slot let each of them overwrite 16 bytes of the next slot.
class CTaskAllocatorPool : public CPool<CTaskAllocator, char[sizeof(void*) == 4 ? 32 : 48]> {
#else
class CTaskAllocatorPool : public CPool<CTaskAllocator, char[32]> { // char[32] == CTaskAllocatorPlayerCommandAttack
#endif
public:
    static void InjectHooks() {
        RH_ScopedClass(CTaskAllocatorPool);
        RH_ScopedCategory("Pools");

        RH_ScopedInstall(New, 0x0069D8E0);
    }

public:
    using CPool::CPool;
};
