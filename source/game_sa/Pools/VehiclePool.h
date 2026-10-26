#pragma once

#include "Pool.h"
#include "Heli.h"
#ifdef NOTSA_PORTABLE
#include "Plane.h"
#endif

#ifdef NOTSA_PORTABLE
// STAB1: the widest vehicle is CPlane on LP64 (3136 bytes, CHeli 3128): a plane in a CHeli sized slot overwrote the first 8 bytes (the vtable pointer) of the next slot's vehicle
// (heap-corruption crashes in CWorld::Process / CReplay::RecordThisFrame after planes spawned). Slot stride only matters through index <-> pointer maths inside CPool.
class CVehiclePool : public CPool<CVehicle, CPlane> {
#else
class CVehiclePool : public CPool<CVehicle, CHeli> {
#endif
public:
    static void InjectHooks() {
        RH_ScopedClass(CVehiclePool);
        RH_ScopedCategory("Pools");

        RH_ScopedInstall(New, 0x006E2A50);
    }

public:
    using CPool::CPool;
};
