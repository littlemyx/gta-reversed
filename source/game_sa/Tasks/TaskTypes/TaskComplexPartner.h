#pragma once

#include "TaskComplex.h"

class CTaskComplexSequence;

enum ePartnerState : int8 {
    PARTNER_STATE_UNK_1 = 1,
    PARTNER_STATE_UNK_2,
    PARTNER_STATE_UNK_3,
    PARTNER_STATE_UNK_4,
    PARTNER_STATE_UNK_5,
    PARTNER_STATE_UNK_6,
};

class NOTSA_EXPORT_VTABLE CTaskComplexPartner : public CTaskComplex {
public:
    int32         field_C;
    int32         field_10;
    char          m_commandName[32];
    int32         m_taskId;
    CPed*         m_partner;
    float         m_distanceMultiplier;
    CVector       m_point;
    CVector       m_targetPoint;
    bool          m_leadSpeaker;
    ePartnerState m_partnerState;
    int8          m_firstToTargetFlag;
    int8          m_updateDirectionCount;
    bool          m_taskCompleted;
    bool          m_makePedAlwaysFacePartner;
    char          m_animBlockName[16];
    bool          m_requiredAnimsStreamedIn;

public:
    static constexpr auto Type = TASK_COMPLEX_PARTNER;

    CTaskComplexPartner(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, bool makePedAlwaysFacePartner, int8 updateDirectionCount, CVector point);
    ~CTaskComplexPartner() override;

    eTaskType GetTaskType() const override { return Type; }
    CTask*       CreateNextSubTask(CPed* ped) override;
    CTask*       CreateFirstSubTask(CPed* ped) override;
    CTask*       ControlSubTask(CPed* ped) override;
    virtual void StreamRequiredAnims();
    virtual void RemoveStreamedAnims();
    virtual CTaskComplexSequence* GetPartnerSequence() = 0; // vtable slot 13 is `_purecall` in the exe

protected:
    // NOTSA: the base class code uses a 16-bit counter at +0x70, which is `field_70` of the derived classes (the base is 0x70 bytes)
    int16& GetGoToPointFrameCounter() { return *reinterpret_cast<int16*>(reinterpret_cast<uint8*>(this) + 0x70); }

private:
    friend void InjectHooksMain();
    static void InjectHooks();

    CTaskComplexPartner* Constructor(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, bool makePedAlwaysFacePartner, int8 updateDirectionCount, CVector point);
};

VALIDATE_SIZE(CTaskComplexPartner, 0x70);
