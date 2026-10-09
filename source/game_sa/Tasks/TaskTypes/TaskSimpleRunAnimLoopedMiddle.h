#pragma once

#include "TaskSimpleAnim.h"
#include "TaskTimer.h"
#include "AnimationEnums.h"

class CAnimBlendHierarchy;

//! Plays an animation, and (until the timer runs out) keeps looping back to a point near its start whenever it passes a point near its end
class NOTSA_EXPORT_VTABLE CTaskSimpleRunAnimLoopedMiddle : public CTaskSimpleAnim {
public:
    static constexpr auto Type = TASK_SIMPLE_ANIM_LOOPED_MIDDLE;

    static void InjectHooks();

    //! Run an anim by group + id (0x61AC90)
    CTaskSimpleRunAnimLoopedMiddle(AssocGroupId animGroup, AnimationId animId, float blendDelta, float loopStartFrac, float loopEndFrac, int32 durationMs, bool bHoldLastFrame);

    //! Run a named anim (0x61AD10)
    CTaskSimpleRunAnimLoopedMiddle(const char* animName, const char* animBlockName, uint32 animFlags, float blendDelta, float loopStartFrac, float loopEndFrac, int32 durationMs, bool bHoldLastFrame);

    ~CTaskSimpleRunAnimLoopedMiddle() override = default; // 0x61BCA0 (scalar deleting dtor: 0x61BC80)

    eTaskType GetTaskType() const override { return (eTaskType)m_TaskType; } // 0x61AD00
    CTask*    Clone() const override; // 0x61B890
    bool      ProcessPed(CPed* ped) override; // 0x61BCB0

    void StartAnim(CPed* ped); // 0x61ADF0

private:
    CTaskSimpleRunAnimLoopedMiddle* Constructor1(AssocGroupId animGroup, AnimationId animId, float blendDelta, float loopStartFrac, float loopEndFrac, int32 durationMs, bool bHoldLastFrame) {
        this->CTaskSimpleRunAnimLoopedMiddle::CTaskSimpleRunAnimLoopedMiddle(animGroup, animId, blendDelta, loopStartFrac, loopEndFrac, durationMs, bHoldLastFrame);
        return this;
    }
    CTaskSimpleRunAnimLoopedMiddle* Constructor2(const char* animName, const char* animBlockName, uint32 animFlags, float blendDelta, float loopStartFrac, float loopEndFrac, int32 durationMs, bool bHoldLastFrame) {
        this->CTaskSimpleRunAnimLoopedMiddle::CTaskSimpleRunAnimLoopedMiddle(animName, animBlockName, animFlags, blendDelta, loopStartFrac, loopEndFrac, durationMs, bHoldLastFrame);
        return this;
    }

private:
    AssocGroupId         m_AnimGroup{};                       // 0x10
    AnimationId          m_AnimId{};                          // 0x14
    uint32               m_AnimFlags{};                       // 0x18
    char                 m_AnimName[24]{};                    // 0x1C
    char                 m_AnimBlockName[16]{};               // 0x34
    CAnimBlendHierarchy* m_AnimHierarchy{};                   // 0x44 - Only set if run by name
    float                m_BlendDelta{};                      // 0x48
    float                m_LoopStartFrac{};                   // 0x4C - Fraction of the anim's total time to jump back to
    float                m_LoopEndFrac{};                     // 0x50 - Fraction of the anim's total time after which to jump back
    float                m_LoopStartTime{};                   // 0x54 - Calculated from `m_LoopStartFrac` in `StartAnim` (In seconds)
    float                m_LoopEndTime{};                     // 0x58 - Calculated from `m_LoopEndFrac` in `StartAnim` (In seconds)
    int32                m_DurationMs{};                      // 0x5C - For how long to loop
    CTaskTimer           m_Timer{};                           // 0x60
    uint16               m_TaskType{ TASK_SIMPLE_ANIM_LOOPED_MIDDLE }; // 0x6C
};
VALIDATE_SIZE(CTaskSimpleRunAnimLoopedMiddle, 0x70);
