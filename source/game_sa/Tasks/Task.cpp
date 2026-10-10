/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Task.h"

// 0x61A660 (body; scalar deleting dtor is the virtual slot 0)
// The exe's destructors store the vftable (`mov [ecx], 0x86D48C`), so a stale pointer to a deleted task dispatches to the CTask base virtuals.
// MSVC /O2 elides the dead vptr store in an inline `= default` dtor, so this one must stay out of line.
CTask::~CTask() { }

// 0x61A5A0
void* CTask::operator new(size_t size) {
    return GetTaskPool()->New();
}

// 0x61A5B0
void CTask::operator delete(void* object) {
    GetTaskPool()->Delete(static_cast<CTask*>(object));
}

// 0x421180
void CTask::StopTimer(const CEvent* event) {
    // NOP
}

// 0x61A360
bool CTask::IsGoToTask(CTask* task) {
    switch (task->GetTaskType()) {
    case TASK_SIMPLE_GO_TO_POINT:
    case TASK_SIMPLE_GO_TO_POINT_FINE:
        return true;
    default:
        return false;
    }
}

// 0x61A4B0
bool CTask::IsTaskPtr(CTask* task) {
    return task && GetTaskPool()->IsObjectValid(task);
}

void CTask::InjectHooks() {
    RH_ScopedCategory("Tasks");
    RH_ScopedVirtualClass(CTask, 0x86D48C, 6);

    RH_ScopedInstall(IsTaskPtr, 0x61A4B0);
    RH_ScopedInstall(operator new, 0x61A5A0);
    RH_ScopedInstall(operator delete, 0x61A5B0);
    RH_ScopedVMTInstall(StopTimer, 0x421180);
}
