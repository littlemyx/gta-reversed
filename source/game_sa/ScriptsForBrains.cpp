#include "StdInc.h"

#include "ScriptsForBrains.h"

#include "Scripts/TheScripts.h"
#include "Pools/Pools.h"

void CScriptsForBrains::InjectHooks() {
    RH_ScopedClass(CScriptsForBrains);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Init, 0x46A8C0);
    RH_ScopedInstall(SwitchAllObjectBrainsWithThisID, 0x46A900);
    RH_ScopedInstall(AddNewScriptBrain, 0x46A930);
    RH_ScopedInstall(AddNewStreamedScriptBrainForCodeUse, 0x46A9C0);
    RH_ScopedInstall(GetIndexOfScriptBrainWithThisName, 0x46AA30);
    RH_ScopedInstall(HasAttractorScriptBrainWithThisNameLoaded, 0x46AB20);
    RH_ScopedInstall(StartNewStreamedScriptBrain, 0x46B270);
    RH_ScopedInstall(StartAttractorScriptBrainWithThisName, 0x46B390);
    RH_ScopedInstall(StartOrRequestNewStreamedScriptBrain, 0x46CD80);
    RH_ScopedInstall(StartOrRequestNewStreamedScriptBrainWithThisName, 0x46CED0);
    RH_ScopedInstall(IsObjectWithinBrainActivationRange, 0x46B3D0);
}


// 0x46A8C0
void CScriptsForBrains::Init() {
    for (auto& script : m_aScriptForBrains) {
        script = tScriptForBrains();
    }
}

// 0x46A900
void CScriptsForBrains::SwitchAllObjectBrainsWithThisID(int8 ID, bool bStatus) {
    if (ID < 0) {
        return;
    }
    for (auto& script : m_aScriptForBrains) {
        if (script.m_ObjectGroupingId == ID) {
            script.m_bBrainActive = bStatus;
        }
    }
}

// 0x46A930
// NOTE: The parameter names are misleading: `attachType` is stored as the type of the brain, `Type` as the object grouping ID
void CScriptsForBrains::AddNewScriptBrain(int16 ImgIndex, int16 Model, uint16 priority, int8 attachType, int8 Type, float Radius) {
    for (auto& script : m_aScriptForBrains) {
        if (script.m_StreamedScriptIndex != -1) {
            continue;
        }
        script.m_StreamedScriptIndex        = ImgIndex;
        script.m_PercentageChance           = priority;
        script.m_ObjectGroupingId           = Type;
        script.m_PedModelOrPedGeneratorIndex = Model;
        script.m_TypeOfBrain                = attachType;
        script.m_bBrainActive               = true;
        script.m_ObjectBrainActivationRadius = Radius > 0.f ? Radius : 5.f;
        return;
    }
}

// 0x46A9C0
void CScriptsForBrains::AddNewStreamedScriptBrainForCodeUse(int16 a2, char* a3, int8 attachtype) {
    for (auto& script : m_aScriptForBrains) {
        if (script.m_StreamedScriptIndex != -1) {
            continue;
        }
        script.m_StreamedScriptIndex = a2;
        strcpy(script.m_ScriptName, a3); // BUG: No bounds check, the original does the same
        script.m_TypeOfBrain                 = attachtype;
        script.m_ObjectGroupingId            = -1;
        script.m_bBrainActive                = true;
        script.m_ObjectBrainActivationRadius = 5.f;
        return;
    }
}

void CScriptsForBrains::CheckIfNewEntityNeedsScript(CEntity* entity, int8 attachType, void* unused) {
    plugin::CallMethod<0x46FF20, CScriptsForBrains*, CEntity*, int8, void*>(this, entity, attachType, unused);
}

void CScriptsForBrains::MarkAttractorScriptBrainWithThisNameAsNoLongerNeeded(const char* name) {
    plugin::CallMethod<0x46AAE0, CScriptsForBrains*, const char*>(this, name);
}

void CScriptsForBrains::RequestAttractorScriptBrainWithThisName(const char* name) {
    plugin::CallMethod<0x46AA80, CScriptsForBrains*, const char*>(this, name);
}

// 0x46B270
void CScriptsForBrains::StartNewStreamedScriptBrain(uint8 index, CEntity* entity, bool bHasAScriptBrain) {
    const auto& brain = m_aScriptForBrains[index];

    auto* const script = CTheScripts::StreamedScripts.StartNewStreamedScript(brain.m_StreamedScriptIndex);
    script->m_ExternalType = brain.m_TypeOfBrain; // NOTE: `script` isn't null-checked in the original

    switch (brain.m_TypeOfBrain) {
    case 0:
    case 3:
    case 5: { // Ped
        auto* const ped = static_cast<CPed*>(entity);

        script->m_LocalVars[0].iParam = CPools::GetPedRef(ped);
        ped->bHasAScriptBrain = true;
        if (brain.m_TypeOfBrain == 5) {
            script->m_LocalVars[1].iParam = bHasAScriptBrain;
        }

        ped->bWaitingForScriptBrainToLoad = false;
        CTheScripts::RemoveFromWaitingForScriptBrainArray(ped, ped->m_StreamedScriptBrainToLoad);
        ped->m_StreamedScriptBrainToLoad = -1;
        break;
    }
    case 1:
    case 4: { // Object
        auto* const obj = static_cast<CObject*>(entity);

        script->m_LocalVars[0].iParam = CPools::GetObjectRef(obj);
        obj->m_nObjectFlags |= 0x300000; // Both of `b0x100000_0x200000`
        break;
    }
    default:
        break;
    }
}

// 0x46CD80
void CScriptsForBrains::StartOrRequestNewStreamedScriptBrain(uint8 index, CEntity* entity, int8 attachType, bool bAddToWaitingArray) {
    if (bAddToWaitingArray) {
        if (!m_aScriptForBrains[index].m_bBrainActive) {
            return;
        }

        switch (attachType) {
        case 0:
        case 3: {
            const auto* const ped = static_cast<CPed*>(entity);
            if (ped->bHasAScriptBrain || ped->bWaitingForScriptBrainToLoad) {
                return;
            }
            break;
        }
        case 1:
        case 4: {
            if (static_cast<CObject*>(entity)->m_nObjectFlags & 0x300000) {
                return;
            }
            break;
        }
        }
    }

    if (attachType == 1) {
        auto* const obj = static_cast<CObject*>(entity);
        if (!bAddToWaitingArray) {
            obj->m_nObjectFlags = (obj->m_nObjectFlags & ~0x300000u) | 0x200000u;
        } else {
            obj->m_nStreamedScriptBrainToLoad = index;
            obj->m_nObjectFlags               = (obj->m_nObjectFlags & ~0x300000u) | 0x100000u;
            CTheScripts::AddToWaitingForScriptBrainArray(obj, index);
        }
    }

    const auto scriptIdx = m_aScriptForBrains[index].m_StreamedScriptIndex;
    if (CStreaming::IsModelLoaded(SCMToModelId(scriptIdx))) {
        StartNewStreamedScriptBrain(index, entity, false);
        return;
    }

    CStreaming::RequestModel(SCMToModelId(scriptIdx), STREAMING_MISSION_REQUIRED);
    // BUG: Attach type 4 is an object type, but is handled as a ped here (original behaviour)
    if ((attachType == 0 || (attachType > 2 && attachType < 5)) && bAddToWaitingArray) {
        auto* const ped = static_cast<CPed*>(entity);
        ped->m_StreamedScriptBrainToLoad  = index;
        ped->bWaitingForScriptBrainToLoad = true;
        CTheScripts::AddToWaitingForScriptBrainArray(ped, index);
    }
}

// 0x46CED0
void CScriptsForBrains::StartOrRequestNewStreamedScriptBrainWithThisName(const char* name, CEntity* entity, int8 attachType) {
    if (const auto idx = GetIndexOfScriptBrainWithThisName(name, attachType); idx >= 0) {
        StartOrRequestNewStreamedScriptBrain(static_cast<uint8>(idx), entity, attachType, true);
    }
}

bool CScriptsForBrains::HasAttractorScriptBrainWithThisNameLoaded(const char* name) {
    if (const auto idx = GetIndexOfScriptBrainWithThisName(name, 5); idx >= 0) {
        return CStreaming::IsModelLoaded(SCMToModelId(m_aScriptForBrains[idx].m_StreamedScriptIndex));
    }
    return false;
}

// 0x46B3D0
bool CScriptsForBrains::IsObjectWithinBrainActivationRange(CObject* entity, const CVector& point) {
    const auto& brain = m_aScriptForBrains[entity->m_nStreamedScriptBrainToLoad];
    if (brain.m_TypeOfBrain != 1) {
        return false;
    }
    return (point - entity->GetPosition()).Magnitude() < brain.m_ObjectBrainActivationRadius;
}

int16 CScriptsForBrains::GetIndexOfScriptBrainWithThisName(const char* name, int8 type) {
    const auto it = rng::find_if(m_aScriptForBrains, [=](tScriptForBrains& script) {
        return script.m_TypeOfBrain == type && !_stricmp(script.m_ScriptName, name);
    });
    return it != m_aScriptForBrains.end()
        ? rng::distance(m_aScriptForBrains.begin(), it)
        : -1;
}

void CScriptsForBrains::StartAttractorScriptBrainWithThisName(const char* name, CPed* ped, bool bHasAScriptBrain) {
    if (!ped->bWaitingForScriptBrainToLoad && !ped->bHasAScriptBrain) {
        if (const auto idx = GetIndexOfScriptBrainWithThisName(name, 5); idx >= 0) {
            StartNewStreamedScriptBrain(static_cast<uint8>(idx), ped, bHasAScriptBrain);
        }
    }
}
