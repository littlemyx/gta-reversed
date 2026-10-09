#include "StdInc.h"

#include "EntitySeekPosCalculatorStandard.h"

void CEntitySeekPosCalculatorStandard::InjectHooks() {
    RH_ScopedVirtualClass(CEntitySeekPosCalculatorStandard, 0x859DC4, 2);
    RH_ScopedCategory("Tasks/TaskTypes/SeekPosCalculators");

    RH_ScopedVMTInstall(ComputeEntitySeekPos, 0x46af20);
}

// NOTSA: 0x46AC10 is the constructor of `CTaskComplexSeekEntity<CEntitySeekPosCalculatorStandard>` (hooked in `CTaskComplexSeekEntityStandard`),
//        the calculator itself has no constructor of its own.

void CEntitySeekPosCalculatorStandard::ComputeEntitySeekPos(const CPed& seeker, const CEntity& target, CVector& outPos) {
    outPos = target.GetPosition();
}
