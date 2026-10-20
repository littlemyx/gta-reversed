#include "StdInc.h"

#include "CustomCarEnvMapPipeline.h"

// 0x5D8D30
void* CCustomCarEnvMapPipeline::pluginEnvAtmConstructorCB(void* object, int32 offsetInObject, int32 sizeInObject) {
    *RWPLUGINOFFSET(CustomEnvMapPipeAtomicData*, object, offsetInObject) = nullptr; // exe 0x5D8D30: the offset ARGUMENT (not ms_envMapAtmPluginOffset)
    return object;
}

// 0x5D9730
void* CCustomCarEnvMapPipeline::pluginEnvAtmDestructorCB(void* object, int32 offsetInObject, int32 sizeInObject) {
    // exe 0x5D9730: the slot is addressed with the offset ARGUMENT (not ms_envMapAtmPluginOffset)
    if (auto* const data = std::exchange(*RWPLUGINOFFSET(CustomEnvMapPipeAtomicData*, object, offsetInObject), nullptr)) {
        m_gEnvMapPipeAtmDataPool->Delete(data);
    }
    return object;
}

// 0x5D9780
void* CCustomCarEnvMapPipeline::pluginEnvAtmCopyConstructorCB(void* dstObject, const void* srcObject, int32 offsetInObject, int32 sizeInObject) {
    if (auto* const src = EnvMapAtmPlGetData((RpAtomic*)(srcObject))) {
        if (auto* const dst = AllocEnvMapPipeAtomicData((RpAtomic*)(dstObject))) {
            *dst = *src;
        }
    }
    return dstObject;
}

// 0x5D96F0
CustomEnvMapPipeAtomicData* CCustomCarEnvMapPipeline::AllocEnvMapPipeAtomicData(RpAtomic* atomic) {
    auto*& data = EnvMapAtmPlGetData(atomic);
    if (!data) {
        if (data = m_gEnvMapPipeAtmDataPool->New()) {
            SetCustomEnvMapPipeAtomicDataDefaults(data);
        }
    }
    return data;
}

void CCustomCarEnvMapPipeline::SetCustomEnvMapPipeAtomicDataDefaults(CustomEnvMapPipeAtomicData* data) {
    data->OffsetU = 0.0f;
    data->PrevPosY = 0.0f;
    data->PrevPosX = 0.0f;
}
