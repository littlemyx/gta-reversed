#include "StdInc.h"
#include "TaskComplexUseClosestFreeScriptedAttractor.h"
#include "TaskComplexUseEffect.h"
#include "TaskComplexUseEffectRunning.h"
#include "TaskComplexUseEffectSprinting.h"
#include "Scripted2dEffects.h"
#include "PedAttractorManager.h"
#include "InterestingEvents.h"
#include "2dEffect.h"

void CTaskComplexUseClosestFreeScriptedAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexUseClosestFreeScriptedAttractor, 0x86e428, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6346F0);
    RH_ScopedInstall(Destructor, 0x634720);

    RH_ScopedGlobalInstall(ComputeClosestFreeScriptedEffect, 0x634740);

    RH_ScopedVMTInstall(Clone, 0x636F70);
    RH_ScopedVMTInstall(GetTaskType, 0x634710);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x634730);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x639530);
    RH_ScopedVMTInstall(ControlSubTask, 0x634890);
}

CTaskComplexUseClosestFreeScriptedAttractor::CTaskComplexUseClosestFreeScriptedAttractor(eMoveState ms) :
    m_MoveState{ms}
{
}

CTaskComplexUseClosestFreeScriptedAttractor::CTaskComplexUseClosestFreeScriptedAttractor(const CTaskComplexUseClosestFreeScriptedAttractor&) :
    CTaskComplexUseClosestFreeScriptedAttractor{}
{
}

// 0x634740
C2dEffect* CTaskComplexUseClosestFreeScriptedAttractor::ComputeClosestFreeScriptedEffect(CPed const& ped) {
    C2dEffect* closest{};
    float      closestDistSq = FLT_MAX;
    for (auto&& [i, fx] : rngv::enumerate(CScripted2dEffects::ms_effects)) {
        if (!CScripted2dEffects::ms_activated[i]) {
            continue;
        }

        // Check if the ped is allowed to use this effect
        const auto& userList = CScripted2dEffects::ms_userLists[i];
        if (userList.m_bUseList) {
            const auto isAllowed = [&] {
                for (const auto userType : userList.m_UserTypes) {
                    if (ped.m_nModelIndex == userType) {
                        return true;
                    }
                }
                for (auto j = 0; j < 4; j++) { // -2 => by ped type
                    if (userList.m_UserTypes[j] == -2 && userList.m_UserTypesByPedType[j] == (int32)ped.m_nPedType) {
                        return true;
                    }
                }
                return false;
            }();
            if (!isAllowed) {
                continue;
            }
        }

        // x87: the distance stays in extended precision until the comparison
        const auto  pos = ped.GetPosition();
        const double dx = (double)pos.x - (double)fx.m_Pos.x;
        const double dy = (double)pos.y - (double)fx.m_Pos.y;
        const double dz = (double)pos.z - (double)fx.m_Pos.z;
        const double distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < (double)closestDistSq) {
            if (GetPedAttractorManager()->HasEmptySlot(reinterpret_cast<const C2dEffectPedAttractor*>(&fx), nullptr)) {
                closestDistSq = (float)distSq;
                closest       = &fx;
            }
        }
    }
    return closest;
}

// 0x639530
CTask* CTaskComplexUseClosestFreeScriptedAttractor::CreateFirstSubTask(CPed* ped) {
    const auto fx = reinterpret_cast<C2dEffectPedAttractor*>(ComputeClosestFreeScriptedEffect(*ped));
    if (!fx) {
        return nullptr;
    }

    g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_3, ped);

    switch (m_MoveState) {
    case PEDMOVE_RUN:    return new CTaskComplexUseEffectRunning{ fx, nullptr };
    case PEDMOVE_SPRINT: return new CTaskComplexUseEffectSprinting{ fx, nullptr };
    default:             return new CTaskComplexUseEffect{ fx, nullptr };
    }
}
