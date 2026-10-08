#include "StdInc.h"

#include "ScriptResourceManager.h"

void CScriptResourceManager::InjectHooks() {
    RH_ScopedClass(CScriptResourceManager);
    RH_ScopedCategory("Scripts");

    RH_ScopedInstall(Initialise, 0x470480);
    RH_ScopedInstall(AddToResourceManager, 0x4704B0);
    RH_ScopedInstall(RemoveFromResourceManager, 0x470510);
    RH_ScopedInstall(HasResourceBeenRequested, 0x470620);
    //RH_ScopedInstall(Load, 0x0, { .Reversed = false });
    //RH_ScopedInstall(Save, 0x0, { .Reversed = false });
}

// 0x470480
void CScriptResourceManager::Initialise() {
    m_aScriptResources.fill(tScriptResource());
}

// 0x4704B0
void CScriptResourceManager::AddToResourceManager(int32 modelId, eScriptResourceType type, CRunningScript* script) {
    int32 freeSlot = -1;
    for (int32 i = 0; i < (int32)m_aScriptResources.size(); i++) {
        auto& res = m_aScriptResources[i];

        // Already registered?
        if (res.m_nModelId == modelId && (uint16)res.m_nType == (uint32)type && res.m_pThread == script) {
            return;
        }

        // NOTE: The last free slot is used (an empty slot has the type `RESOURCE_TYPE_DEFAULT`)
        if ((uint16)res.m_nType == 0) {
            freeSlot = i;
        }
    }

    if (freeSlot != -1) {
        auto& res     = m_aScriptResources[freeSlot];
        res.m_nModelId = modelId;
        res.m_nType    = (eScriptResourceType)(uint16)type; // Original code only wrote the lower 16 bits
        res.m_pThread  = script;
    }
}

// 0x470510
bool CScriptResourceManager::RemoveFromResourceManager(int32 modelId, eScriptResourceType type, CRunningScript* script) {
    int32 toRemove    = -1;
    int32 otherUsers  = 0;
    for (int32 i = 0; i < (int32)m_aScriptResources.size(); i++) {
        const auto& res = m_aScriptResources[i];
        if (res.m_nModelId != modelId || (uint16)res.m_nType != (uint32)type) {
            continue;
        }
        if (res.m_pThread == script) {
            toRemove = i; // Last match wins
        } else {
            otherUsers++;
        }
    }

    if (toRemove != -1) {
        auto& res      = m_aScriptResources[toRemove];
        res.m_nModelId = -1;
        res.m_nType    = RESOURCE_TYPE_DEFAULT;
        res.m_pThread  = nullptr;
    }

    // Returns true if no other script is still using the resource
    return otherUsers == 0;
}

// 0x470620
bool CScriptResourceManager::HasResourceBeenRequested(int32 modelId, eScriptResourceType type) {
    for (const auto& res : m_aScriptResources) {
        if (res.m_nModelId == modelId && (uint16)res.m_nType == (uint32)type) {
            return true;
        }
    }
    return false;
}

// 0x0
bool CScriptResourceManager::Load() {
    assert(false);
    return true;
}

// 0x0
bool CScriptResourceManager::Save() {
    assert(false);
    return true;
}
