#include "StdInc.h"

#include "TaskComplexAvoidEntity.h"
#include "TaskSimpleGoToPoint.h"
#include "PedGeometryAnalyser.h"
#include "Ragdoll/IKChainManager.h"
#include "ColSphere.h"
#include "Pad.h"

namespace {
//! 0x59C910 - `CVector::Normalise` as the original evaluates it: the sum of squares and the reciprocal root stay in the FPU
//! (extended precision), every component is stored as float. A length of 0 (or less) yields (1, y, z) - only x is written.
void NormaliseExt(CVector& v) {
    const double sq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sq <= 0.0) { // FCOM + JP: NaN takes the sqrt path
        v.x = 1.0f;
        return;
    }
    const double inv = 1.0 / std::sqrt(sq);
    v.x = (float)(v.x * inv);
    v.y = (float)(v.y * inv);
    v.z = (float)(v.z * inv);
}

//! 0x406DA0
double SqMagnitudeExt(const CVector& v) {
    return ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
}

constexpr auto LOOK_AT_PURPOSE = "TaskAvoidEntity"; // 0x86FF2C
}; // namespace

void CTaskComplexAvoidEntity::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexAvoidEntity, 0x86FF00, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x66AA20);
    RH_ScopedInstall(LookAtTarget, 0x66AB40);
    RH_ScopedInstall(StopLooking, 0x66AD10);
    RH_ScopedInstall(ComputeRoute, 0x66FD30);
    RH_ScopedInstall(ComputeBoundingSphere, 0x66F6C0);
    RH_ScopedVMTDestructorInstall(0x66F6A0);
    RH_ScopedVMTInstall(Clone, 0x66D0C0);
    RH_ScopedVMTInstall(GetTaskType, 0x66AAD0);
    RH_ScopedVMTInstall(MakeAbortable, 0x66AD40);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x66AD70);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x672530);
    RH_ScopedVMTInstall(ControlSubTask, 0x66ADE0);
}

// 0x66AA20
CTaskComplexAvoidEntity::CTaskComplexAvoidEntity(eMoveState moveState, CEntity* entity, const CVector& targetPos) :
    m_Entity{ entity },
    m_MoveState{ moveState },
    m_TargetPos{ targetPos },
    m_RoutePoint2{ targetPos }
{
}

// 0x66AD40
bool CTaskComplexAvoidEntity::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    StopLooking(ped);
    return true;
}

// 0x66AD70
CTask* CTaskComplexAvoidEntity::CreateNextSubTask(CPed* ped) {
    StopLooking(ped);
    return nullptr;
}

// 0x672530
CTask* CTaskComplexAvoidEntity::CreateFirstSubTask(CPed* ped) {
    m_PedPos = ped->GetPosition();

    if (!ComputeRoute(ped)) {
        StopLooking(ped);
        return nullptr;
    }
    return new CTaskSimpleGoToPoint{ m_MoveState, m_RoutePoint2, 0.5f, false, false }; // 0x86FC78
}

// 0x66ADE0
CTask* CTaskComplexAvoidEntity::ControlSubTask(CPed* ped) {
    if (!m_Entity) { // Entity is gone
        StopLooking(ped);
        return nullptr;
    }

    LookAtTarget(ped);

    if (ped->GetIntelligence()->m_AnotherStaticCounter > 30) { // 0x86C938
        StopLooking(ped);
        return nullptr;
    }

    // Finish if we're too far from the entity (> 15 units)
    const auto& entityPos = m_Entity->GetPosition();
    const auto& pedPos    = ped->GetPosition();
    const double dx = (double)pedPos.x - (double)entityPos.x;
    const double dy = (double)pedPos.y - (double)entityPos.y;
    const double dz = (double)pedPos.z - (double)entityPos.z;
    const double distSq = (dz * dz + dx * dx) + dy * dy;
    // FCOMPP: ST(0)=7.5*7.5*4 (0x86C8FC, 0x858B90), ST(1)=distSq; JP taken (=> keep going) if 225 > distSq, 225 == distSq or unordered.
    // Not taken (=> finish) only if 225 < distSq.
    if ((double)7.5f * (double)7.5f * (double)4.f < distSq) {
        StopLooking(ped);
        return nullptr;
    }
    return m_pSubTask;
}

// 0x66AB40
void CTaskComplexAvoidEntity::LookAtTarget(CPed* ped) {
    const auto pad = CPad::GetPad(0); // 0x53FB70
    if (ped == FindPlayerPed(-1) && !pad->DisablePlayerControls) { // 0x56E210
        return;
    }
    if (!ped->GetIsOnScreen()) { // 0x534540
        return;
    }
    if (m_IsLookingAt) {
        return;
    }
    if (g_ikChainMan.GetLookAtEntity(ped)) { // 0x6181D0
        return;
    }
    if (ped->GetIntelligence()->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_IK)) { // 0x681810
        return;
    }
    if (const auto parent = m_Parent) { // Don't look if the parent is avoiding someone else already
        if (parent->GetTaskType() == TASK_COMPLEX_AVOID_OTHER_PED_WHILE_WANDERING) {
            return;
        }
        if (parent->GetTaskType() == TASK_COMPLEX_AVOID_ENTITY) {
            return;
        }
    }

    const auto& pedPos = ped->GetPosition();
    CVector     dir    = m_TargetPos - pedPos; // 0x40FE60
    if (!(SqMagnitudeExt(dir) > 9.0)) { // 0x406DA0, 0x85EED4 (Only look if the target is far enough away)
        return;
    }
    NormaliseExt(dir); // 0x59C910

    // Don't bother if the target is (more or less) in front of the ped
    const auto&  fwd = ped->GetMatrix().GetForward();
    const double dot = ((double)dir.z * fwd.z + (double)dir.y * fwd.y) + (double)dir.x * fwd.x;
    if (!(dot < (double)CTaskSimpleGoTo::ms_fLookAtThresholdDotProduct)) { // 0xC18D48
        return;
    }

    // Look at a point 2 units behind the target
    CVector lookAtPos{
        (float)((double)m_TargetPos.x + (double)dir.x * 2.0),
        (float)((double)m_TargetPos.y + (double)dir.y * 2.0),
        (float)(((double)dir.z * 2.0 + (double)pedPos.z) + (double)0.61f) // 0x86FD40
    };
    g_ikChainMan.LookAt(LOOK_AT_PURPOSE, ped, nullptr, 5000, BONE_UNKNOWN, &lookAtPos, false, 0.25f, 500, 3, false); // 0x618970
    m_IsLookingAt = true;
}

// 0x66AD10
void CTaskComplexAvoidEntity::StopLooking(CPed* ped) {
    if (m_IsLookingAt) {
        if (g_ikChainMan.IsLooking(ped)) { // 0x6181A0
            g_ikChainMan.AbortLookAt(ped, 250); // 0x618280
        }
    }
}

// 0x66FD30
bool CTaskComplexAvoidEntity::ComputeRoute(CPed* ped) {
    CEntity* entities[16]{};
    entities[0] = m_Entity;

    CColSphere sphere{};
    ComputeBoundingSphere(sphere, entities); // 0x66F6C0

    sphere.m_vecCenter.z = ped->GetPosition().z;
    return CPedGeometryAnalyser::ComputeRouteRoundSphere(*ped, sphere, m_PedPos, m_TargetPos, m_RoutePoint1, m_RoutePoint2); // 0x5F1890
}

// 0x66F6C0
void CTaskComplexAvoidEntity::ComputeBoundingSphere(CColSphere& out, CEntity* const* entities) {
    // Centre = average position of all entities (x, y only)
    CVector sum{};
    int32   count = 0;
    for (auto i = 0; i < 16; i++) {
        if (const auto e = entities[i]) {
            const auto& pos = e->GetPosition();
            count++;
            sum.x = (float)((double)sum.x + pos.x);
            sum.y = (float)((double)sum.y + pos.y);
            sum.z = (float)((double)sum.z + pos.z);
        }
    }
    const double inv = (double)1.f / (double)count; // 0x858624 (FDIVR)
    CVector centre{
        (float)((double)sum.x * inv),
        (float)((double)sum.y * inv),
        0.f
    };

    // Radius = max of (Distance^2 + BoundRadius^2) of all entities
    double maxSq = 0.0; // 0x858B50
    for (auto i = 0; i < 16; i++) {
        if (const auto e = entities[i]) {
            const auto& pos = e->GetPosition();
            const double dx = (double)pos.x - (double)centre.x;
            const double dy = (double)pos.y - (double)centre.y;
            const double r  = e->GetModelInfo()->GetColModel()->GetBoundRadius();
            const double v  = (dy * dy + dx * dx) + r * r;
            if (v > maxSq) {
                maxSq = (double)(float)v; // Stored to a float temp, and reloaded
            }
        }
    }
    out.Set((float)(std::sqrt(maxSq) + (double)0.7f), centre, SURFACE_DEFAULT, 0, tColLighting{ 0xFF }); // 0x86FCC8, 0x40FD10
}
