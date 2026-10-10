/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include "LoadedCarGroup.h"

constexpr auto SENTINEL_VALUE_OF_UNUSED = (int16)(MODEL_INVALID);

void CLoadedCarGroup::InjectHooks() {
    RH_ScopedClass(CLoadedCarGroup);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Clear, 0x611B90);
    RH_ScopedInstall(AddMember, 0x611BB0);
    RH_ScopedInstall(RemoveMember, 0x611BD0);
    RH_ScopedInstall(GetMember, 0x611C20);
    RH_ScopedInstall(CountMembers, 0x611C30);
    RH_ScopedInstall(PickRandomCar, 0x611C50);
    RH_ScopedInstall(SortBasedOnUsage, 0x611E10);
    RH_ScopedInstall(PickLeastUsedModel, 0x611E90);
}

// 0x611E10
void CLoadedCarGroup::SortBasedOnUsage() {
    // Sort from higher to lower usage
    // The exe (0x611E10) is a plain bubble sort over the leading valid members: swap neighbours while `used(a) < used(b)`.
    // `m_nTimesUsed` (+0x50) is compared as a SIGNED byte (`cmp al, [ebx+0x50]; jge`); equal usages keep their order (stable). std::sort was neither.
    const auto n = (int32)CountMembers();
    if (n - 1 <= 0) {
        return;
    }
    const auto Used = [](int16 modelId) { return (int8)CModelInfo::GetVehicleModelInfo(modelId)->m_nTimesUsed; };
    bool swapped;
    do {
        swapped = false;
        for (int32 i = 0; i < n - 1; i++) {
            const int16 a = m_models[i], b = m_models[i + 1];
            if (Used(a) < Used(b)) {
                m_models[i]     = b;
                m_models[i + 1] = a;
                swapped         = true;
            }
        }
    } while (swapped);
}

// 0x611BD0
void CLoadedCarGroup::RemoveMember(eModelID modelIndex) {
    if (notsa::remove_first(m_models, (int16)(modelIndex))) {
        m_models.back() = SENTINEL_VALUE_OF_UNUSED;
    }
}

// 0x611C50
eModelID CLoadedCarGroup::PickRandomCar(bool bNotTooManyInTheWorld, bool bOnlyPickNormalCars) {
    // Shape of the exe: build the candidate list and the sum of `m_nFrq` (a SIGNED word, +0x52) in one pass; with a zero sum the answer is -1 and
    // no random number is consumed. Then up to 10 tries of: one rand() -> `(rand() & 0xFFFF) * (1/32768) * (float)sum` truncated -> walk the list
    // subtracting frq until `pick <= frq(candidate)`.
    if (Empty()) {
        return MODEL_INVALID;
    }

    std::array<int16, 23> choices;
    size_t                nChoices  = 0;
    int32                 weightSum = 0;
    for (const auto modelId : GetAllModels()) {
        const auto mi = CModelInfo::GetVehicleModelInfo(modelId);
        if (bOnlyPickNormalCars) {
            // movsx byte [+0x4d]: accepts 0..2 (NORMAL, POORFAMILY, RICHFAMILY) and 8 (MOTORBIKE)
            const auto cls = (int8)mi->m_nVehicleClass;
            if (!((cls >= 0 && cls <= 2) || cls == 8)) {
                continue;
            }
        }
        choices[nChoices++] = modelId;
        weightSum += (int16)mi->m_nFrq;
    }
    if (weightSum == 0) {
        return MODEL_INVALID;
    }

    const auto Frq = [&](size_t i) { return (int32)(int16)CModelInfo::GetVehicleModelInfo(choices[i])->m_nFrq; };

    for (auto tr{ 0 }; tr < 10; tr++) { // tr = tries
        // First, pick a model
        const auto pickedModel = [&] {
            auto   pickedWeight = CGeneral::GetRandomNumberInRange(0, weightSum);
            size_t i{};
            // 0x611D65: the exe does not bound this walk (the sum guarantees it ends inside the list)
            while (pickedWeight > Frq(i)) {
                pickedWeight -= Frq(i);
                i++;
                assert(i < nChoices);
            }
            return (eModelID)(choices[i]);
        }();

        // Check if it's suitable
        if (   !CTheScripts::HasCarModelBeenSuppressed(pickedModel)
            && !CTheScripts::HasVehicleModelBeenBlockedByScript(pickedModel)
            && !CStreaming::WeAreTryingToPhaseVehicleOut(pickedModel)
            && (!bNotTooManyInTheWorld || (int16)CModelInfo::GetVehicleModelInfo(pickedModel)->m_nRefCount < 3) // 0x611DD0: signed word compare `< 3`
        ) {
            return pickedModel;
        }
    }

    // 10 tries, but no luck
    return MODEL_INVALID;
}

// 0x611E90
eModelID CLoadedCarGroup::PickLeastUsedModel(int32 maxTimesUsed) {
    // Exe: the best candidate starts at {ref = 999, used = 999} and is replaced when `ref < bestRef` or (`ref == bestRef` and `used < bestUsed`);
    // `m_nRefCount` is read as a signed word, `m_nTimesUsed` as a signed byte, the final test is a signed `bestUsed <= maxTimesUsed`.
    int32    bestRef  = 999;
    int32    bestUsed = 999;
    eModelID best     = MODEL_INVALID;
    for (const auto modelId : GetAllModels()) {
        const auto mi   = CModelInfo::GetVehicleModelInfo(modelId);
        const auto ref  = (int32)(int16)mi->m_nRefCount;
        const auto used = (int32)(int8)mi->m_nTimesUsed;
        if (ref < bestRef || (ref == bestRef && used < bestUsed)) {
            best     = (eModelID)(modelId);
            bestRef  = ref;
            bestUsed = used;
        }
    }
    return bestUsed <= maxTimesUsed
        ? best
        : MODEL_INVALID;
}

// 0x611C20
eModelID CLoadedCarGroup::GetMember(uint32 idx) const {
    assert(idx < CountMembers());
    return (eModelID)(m_models[idx]);
}

// 0x611C30
uint32 CLoadedCarGroup::CountMembers() const {
    return (uint32)(rng::distance(m_models.begin(), rng::find(m_models, SENTINEL_VALUE_OF_UNUSED)));
}

// NOTSA
bool CLoadedCarGroup::Empty() const {
    return m_models.front() == SENTINEL_VALUE_OF_UNUSED;
}

// 0x611B90
void CLoadedCarGroup::Clear() {
    rng::fill(m_models, SENTINEL_VALUE_OF_UNUSED);
}

// 0x611BB0
void CLoadedCarGroup::AddMember(eModelID member) {
    const auto end = rng::find(m_models, SENTINEL_VALUE_OF_UNUSED);
    if (end != m_models.end()) {
        *end = (int16)(member);
    } else {
        NOTSA_LOG_DEBUG("Failed to add model to group [Out of memory]");
    }
}
