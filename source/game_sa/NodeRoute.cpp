#include "StdInc.h"
#include "NodeRoute.h"

void CNodeRoute::InjectHooks() {
    RH_ScopedClass(CNodeRoute);
    RH_ScopedCategory("Core");

    RH_ScopedInstall(operator new, 0x41B860);
    RH_ScopedInstall(operator delete, 0x41B870);
}

void* CNodeRoute::operator new(uint32 size) {
    return GetNodeRoutePool()->New();
}

void CNodeRoute::operator delete(void* ptr, size_t sz) {
    if (!ptr) { // NOTSA: `delete nullptr` must be a no-op. MSVC calls a class operator delete even for null when the destructor is trivial, the exe's call sites test for null first (e.g. 0x64A5BE in ~CTaskComplexGoToCarDoorAndStandStill) while the pool's Delete(0) would flip a wild slot
        return;
    }
    GetNodeRoutePool()->Delete(reinterpret_cast<CNodeRoute*>(ptr));
}
