#pragma once

#include "TaskComplex.h"
#include "Vector.h"

class CPedGroup;
class CPed;

class NOTSA_EXPORT_VTABLE CTaskComplexFollowLeaderInFormation : public CTaskComplex {
    CPedGroup* m_Group;
    CPed*      m_Leader;
    CVector    m_Pos;
    uint32     m_Int;
    float      m_Dist; // Max distance between m_pos and some point, not sure. See `ControlSubTask`. Ignored if < 0

public:
    static constexpr auto Type = TASK_COMPLEX_FOLLOW_LEADER_IN_FORMATION;

    struct Offsets {
        std::array<CVector2D, TOTAL_PED_GROUP_MEMBERS> Offsets;
        std::array<CVector2D, TOTAL_PED_GROUP_MEMBERS> MovingOffsets;
        float                                          Scale;
    };
    static inline NOTSA_GLOBAL(ms_offsets, 0xC196E8, (Offsets), { {{ CVector2D{-1.0f, 1.0f}, CVector2D{1.0f, 1.0f}, CVector2D{0.0f, 1.0f}, CVector2D{-1.0f, 0.0f}, CVector2D{1.0f, 0.0f}, CVector2D{-1.0f, -1.0f}, CVector2D{0.0f, -1.0f}, CVector2D{1.0f, -1.0f} }}, {{ CVector2D{-1.0f, 0.0f}, CVector2D{1.0f, 0.0f}, CVector2D{0.0f, -1.25f}, CVector2D{-1.25f, -1.25f}, CVector2D{1.25f, -1.25f}, CVector2D{0.0f, -2.5f}, CVector2D{-1.0f, -2.5f}, CVector2D{1.0f, -2.5f} }}, 1.0f }); static_assert(sizeof(ms_offsets) == 132); // image bytes of the exe's static initialiser 0x694850

    CTaskComplexFollowLeaderInFormation(CPedGroup* pedGroup, CPed* ped, const CVector& posn, float dist = -1.f);
    CTaskComplexFollowLeaderInFormation(const CTaskComplexFollowLeaderInFormation&);
    ~CTaskComplexFollowLeaderInFormation() override;

    eTaskType GetTaskType() const override { return Type; }
    CTask*    Clone() const override { return new CTaskComplexFollowLeaderInFormation{*this}; }
    CTask*    CreateSubTask(eTaskType taskType, CPed* ped); // 0x6962A0
    CTask*    CreateNextSubTask(CPed* ped) override;
    CTask*    CreateFirstSubTask(CPed* ped) override;
    CTask*    ControlSubTask(CPed* ped) override;

    // NOTSA: Used by `CEventHandler::ComputePedCollisionWithPlayerResponse` (originally read at +0x10)
    CPed* GetLeader() const { return m_Leader; }

private:
    friend void InjectHooksMain();
    static void InjectHooks();

    CTaskComplexFollowLeaderInFormation* Constructor(CPedGroup* pedGroup, CPed* ped, CVector const& posn, float a5) { this->CTaskComplexFollowLeaderInFormation::CTaskComplexFollowLeaderInFormation(pedGroup, ped, posn, a5); return this; }
    CTaskComplexFollowLeaderInFormation* Destructor() { this->CTaskComplexFollowLeaderInFormation::~CTaskComplexFollowLeaderInFormation(); return this; }
};
VALIDATE_SIZE(CTaskComplexFollowLeaderInFormation, 0x28);
