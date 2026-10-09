#pragma once

#include "TaskSimple.h"
#include "TaskTimer.h"

class CAnimBlendAssociation;
class CEvent;
class CPed;

struct InteriorInfo_t;

class NOTSA_EXPORT_VTABLE CTaskInteriorSitOnChair : public CTaskSimple {
public:
    static constexpr auto Type = eTaskType::TASK_INTERIOR_SIT_ON_CHAIR;

    static void InjectHooks();

    CTaskInteriorSitOnChair(int32 duration, InteriorInfo_t* interiorInfo, bool bDoInstantly);
    ~CTaskInteriorSitOnChair() override;

    static void FinishAnimCB(CAnimBlendAssociation* anim, void* data);

    CTask*    Clone() const override { return new CTaskInteriorSitOnChair{ m_Duration, m_InteriorInfo, m_bDoInstantly }; } // 0x675CF0
    eTaskType GetTaskType() const override { return Type; } // 0x675C80
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override;
    bool      ProcessPed(CPed* ped) override;

private: // Wrappers for hooks
    // 0x675C30
    CTaskInteriorSitOnChair* Constructor(int32 duration, InteriorInfo_t* interiorInfo, uint8 bDoInstantly) {
        this->CTaskInteriorSitOnChair::CTaskInteriorSitOnChair(duration, interiorInfo, bDoInstantly);
        return this;
    }

    // 0x675C90
    CTaskInteriorSitOnChair* Destructor() {
        this->CTaskInteriorSitOnChair::~CTaskInteriorSitOnChair();
        return this;
    }

private:
    int32                  m_Duration;
    InteriorInfo_t*        m_InteriorInfo;
    bool                   m_bDoInstantly;
    CAnimBlendAssociation* m_Anim{};
    AnimationId            m_PrevAnimId{ ANIM_ID_UNDEFINED };
    CTaskTimer             m_TaskTimer{};
    bool                   m_bUpdatePedPos{};
    bool                   m_bTaskFinished{};
    bool                   m_bTaskAborting{};
};
VALIDATE_SIZE(CTaskInteriorSitOnChair, 0x2C);
