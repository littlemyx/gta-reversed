#include "StdInc.h"

#include "TimeInfo.h"

void CTimeInfo::InjectHooks() {
    RH_ScopedClass(CTimeInfo);
    RH_ScopedCategory("Models");

    RH_ScopedInstall(FindOtherTimeModel, 0x4C47E0);
    RH_ScopedInstall(GetOtherTimeModel, 0x4C4A30);
    RH_ScopedInstall(GetTimeOff, 0x407330);
    RH_ScopedInstall(GetTimeOn, 0x407320);
    RH_ScopedInstall(SetOtherTimeModel, 0x5B3440);
    RH_ScopedInstall(SetTimes, 0x5B3430);
}

// 0x4C47E0
CTimeInfo* CTimeInfo::FindOtherTimeModel(const char* modelName) {
    char timeSwitchModelName[24];
    strcpy_s(timeSwitchModelName, modelName);

    char* nightSuffix = strstr(timeSwitchModelName, "_nt");

    if (nightSuffix) {
        strncpy_s(nightSuffix, 4, "_dy", 4);
    }
    else {
        char* daySuffix = strstr(timeSwitchModelName, "_dy");

        if (!daySuffix)
            return nullptr;

        strncpy_s(daySuffix, 4, "_nt", 4);
    }

    // 0x4C47E0: the exe walks all model infos and takes the first one that HAS a CTimeInfo (GetTimeInfo() != null) and the key; a non-time model with the same key does not stop the search
    const auto key = CKeyGen::GetUppercaseKey(timeSwitchModelName);
    for (int32 i = 0; i < CModelInfo::NUM_MODEL_INFOS; i++) {
        auto* const mi = CModelInfo::GetModelInfo(i);
        if (!mi) {
            continue;
        }
        if (auto* const timeInfo = mi->GetTimeInfo(); timeInfo && mi->m_nKey == key) {
            m_nOtherTimeModel = (int16)i;
            return timeInfo;
        }
    }
    return nullptr;
}
