#include "StdInc.h"

#include "FxInfoRotSpeed.h"
#include "MovementInfo.h"

void FxInfoRotSpeed_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInfoRotSpeed_c, 0x85A8A8, 3);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A7470);
    RH_ScopedVMTInstall(Load, 0x4A5D20);
    RH_ScopedVMTInstall(GetValue, 0x4A5D40);
}

// 0x4A5CB0
FxInfoRotSpeed_c::FxInfoRotSpeed_c() : FxInfo_c() {
    m_nType = FX_INFO_ROTSPEED_DATA;
    m_InterpInfo.Allocate(4);
}

// 0x4A5D20
void FxInfoRotSpeed_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 0x4A5D40
void FxInfoRotSpeed_c::GetValue(float currentTime, float mult, float totalTime, float len, bool useConst, void* info) {
    if (!m_bTimeModeParticle) {
        mult = currentTime / len;
    }

    float values[4];
    m_InterpInfo.GetVal(values, mult);

    auto& movement = *static_cast<MovementInfo_t*>(info);
    movement.m_Rot[0] = values[0];
    movement.m_Rot[1] = values[1];
    movement.m_Rot[2] = values[2];
    movement.m_Rot[3] = values[3];
}
