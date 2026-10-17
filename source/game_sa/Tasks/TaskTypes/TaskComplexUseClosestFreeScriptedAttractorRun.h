#pragma once

#include "TaskComplexUseClosestFreeScriptedAttractor.h"

class NOTSA_EXPORT_VTABLE CTaskComplexUseClosestFreeScriptedAttractorRun final : public CTaskComplexUseClosestFreeScriptedAttractor {
public:
    static constexpr auto Type = eTaskType::TASK_COMPLEX_USE_CLOSEST_FREE_SCRIPTED_ATTRACTOR_RUN;

    static void InjectHooks();

    CTaskComplexUseClosestFreeScriptedAttractorRun();
    ~CTaskComplexUseClosestFreeScriptedAttractorRun() override = default;

    CTask*    Clone()       const override { return NOTSA_EXE_OR(new CTaskComplexUseClosestFreeScriptedAttractorRun(), new CTaskComplexUseClosestFreeScriptedAttractorRun{ *this }); } // 0x4B87B0; NOTSA-AUDIT: the memberwise copy lost the RUN move state (the base copy constructor resets it to WALK)
    eTaskType GetTaskType() const override { return Type; }

private: // Wrappers for hooks
    // 0x4B8790
    CTaskComplexUseClosestFreeScriptedAttractorRun* Constructor() {
        this->CTaskComplexUseClosestFreeScriptedAttractorRun::CTaskComplexUseClosestFreeScriptedAttractorRun();
        return this;
    }

    // 0x4B8840
    CTaskComplexUseClosestFreeScriptedAttractorRun* Destructor() {
        this->CTaskComplexUseClosestFreeScriptedAttractorRun::~CTaskComplexUseClosestFreeScriptedAttractorRun();
        return this;
    }
};
