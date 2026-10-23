#include "StdInc.h"

#include "Dummy.h"

void CDummy::InjectHooks() {
    RH_ScopedVirtualClass(CDummy, 0x8638C0, 22);
    RH_ScopedCategory("Entity/Dummy");

    RH_ScopedGlobalInstall(IsDummyPointerValid, 0x532730);
}

// 0x5326E0
void* CDummy::operator new(size_t size) {
    return GetDummyPool()->New();
}

// 0x5326F0
void CDummy::operator delete(void* obj) {
    GetDummyPool()->Delete(static_cast<CDummy*>(obj));
}

// 0x532540
CDummy::CDummy() : CEntity() {
    SetTypeDummy();
}

// The exe's check reads the pool's storage base (CPool::m_Storage is private): explicit-instantiation access to it, no change to Pool.h (which every TU includes).
namespace {
template<class Tag, typename Tag::type M> struct DummyPoolRob { friend typename Tag::type DummyPoolRobGet(Tag) { return M; } };
struct DummyPoolStorageTag { using type = byte (*CPool<CDummy>::*)[sizeof(CDummy)]; };
byte (*CPool<CDummy>::* DummyPoolRobGet(DummyPoolStorageTag))[sizeof(CDummy)];
template struct DummyPoolRob<DummyPoolStorageTag, &CPool<CDummy>::m_Storage>; // inside the unnamed namespace: clang requires the instantiation in the template's own namespace
}

// 0x532730: null -> false; idx = (int)(dummy - storage) / sizeof(CDummy) (truncating); valid if 0 <= idx <= size (`jle`: one past the end counts) and the slot state is NOT looked at
bool IsDummyPointerValid(CDummy* dummy) {
    if (!dummy) {
        return false;
    }
    auto* const pool = GetDummyPool();
    const auto* const storage = (pool->*DummyPoolRobGet(DummyPoolStorageTag{}));
    const int32 idx = (int32)((int32)(uintptr_t)dummy - (int32)(uintptr_t)storage) / (int32)sizeof(CDummy);
    return idx >= 0 && idx <= (int32)pool->GetSize();
}
