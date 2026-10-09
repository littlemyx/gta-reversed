#include "StdInc.h"
#include "TaskComplexSunbathe.h"

#include "TaskSimplePause.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleRunTimedAnim.h"
#include "InterestingEvents.h"
#include "Population.h"
#include "Weather.h"
#include "Clock.h"
#include "Streaming.h"

// 0x632140 (NOTSA: not a member in the original, plain cdecl without arguments)
bool CanSunbathe() {
    if (CClock::GetGameClockHours() < 10 || CClock::GetGameClockHours() >= 18) {
        return false;
    }
    switch (CWeather::NewWeatherType) {
    case WEATHER_SUNNY_LA:
    case WEATHER_SUNNY_SMOG_LA:
    case WEATHER_SUNNY_COUNTRYSIDE:
    case WEATHER_SUNNY_SF:
    case WEATHER_SUNNY_VEGAS:
    case WEATHER_SUNNY_DESERT:
        return true;
    // 0x62F2D0 (IsExtraSunnyWeather)
    case WEATHER_EXTRASUNNY_LA:
    case WEATHER_EXTRASUNNY_SMOG_LA:
    case WEATHER_EXTRASUNNY_COUNTRYSIDE:
    case WEATHER_EXTRASUNNY_SF:
    case WEATHER_EXTRASUNNY_VEGAS:
    case WEATHER_EXTRASUNNY_DESERT:
        return true;
    default:
        return false;
    }
}

// 0x632190 (NOTSA: not a member in the original, plain cdecl; callers pass a stray `this` in ecx which is never read)
static bool ShouldLoadSunbatheAnims() {
    const auto* const plyr = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
    if (const auto* const veh = plyr->GetVehicleIfInOne()) {
        return veh->m_vecMoveSpeed.SquaredMagnitude() <= 0.04f;
    }
    return true;
}

void CTaskComplexSunbathe::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexSunbathe, 0x86e0ac, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedGlobalInstall(CanSunbathe, 0x632140);
    RH_ScopedGlobalInstall(ShouldLoadSunbatheAnims, 0x632190);

    RH_ScopedInstall(Constructor, 0x631F80);
    RH_ScopedInstall(Destructor, 0x632050);

    RH_ScopedInstall(CreateSubTask, 0x638290);

    RH_ScopedVMTInstall(Clone, 0x6366A0);
    RH_ScopedVMTInstall(GetTaskType, 0x632040);
    RH_ScopedVMTInstall(MakeAbortable, 0x6320F0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x6399F0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x639CB0);
    RH_ScopedVMTInstall(ControlSubTask, 0x6381A0);
}

// 0x631f80
CTaskComplexSunbathe::CTaskComplexSunbathe(CObject* towel, bool bStartStanding) :
    m_pTowel{towel},
    m_bStartStanding{bStartStanding}
{
    if (m_pTowel) {
        CEntity::SafeRegisterRef(m_pTowel);
        m_pTowel->m_nObjectType = OBJECT_TYPE_DECORATION;
    }

    const auto LoadAnim = [](int32& idx, CAnimBlock*& blk, const char* name) {
        idx = CAnimManager::GetAnimationBlockIndex(name);
        blk = CAnimManager::GetAnimationBlock(name);
    };
    LoadAnim(m_BeachAnimBlockIndex, m_pBeachAnimBlock, "beach");
    LoadAnim(m_SunbatheAnimBlockIndex, m_pSunbatheAnimBlock, "sunbathe");

    // Refs added later in another function
}

CTaskComplexSunbathe::CTaskComplexSunbathe(const CTaskComplexSunbathe& o) :
    CTaskComplexSunbathe{ o.m_pTowel, o.m_bStartStanding }
{
}

// 0x632050
CTaskComplexSunbathe::~CTaskComplexSunbathe() {
    if (m_pTowel) {
        m_pTowel->m_nObjectType = OBJECT_TEMPORARY;
        CEntity::SafeCleanUpRef(m_pTowel);
    }

    const auto UnrefAnimBlock = [](bool& refed, int32 blkIdx) {
        if (refed) {
            CAnimManager::RemoveAnimBlockRef(blkIdx);
            refed = false;
        }
    };
    UnrefAnimBlock(m_bBeachAnimsReferenced, m_BeachAnimBlockIndex);
    UnrefAnimBlock(m_bSunbatheAnimsReferenced, m_SunbatheAnimBlockIndex);
}

// 0x6320F0
bool CTaskComplexSunbathe::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (event) {
        if (event->GetEventType() == EVENT_PED_COLLISION_WITH_PED || event->GetEventType() == EVENT_PED_COLLISION_WITH_PLAYER) {
            return false;
        }
    }
    if (!m_pSubTask->MakeAbortable(ped, priority, event)) {
        return false;
    }
    m_bAborted = true;
    return true;
}

// 0x6399F0
CTask* CTaskComplexSunbathe::CreateNextSubTask(CPed* ped) {
    if (ped->m_nPedType == PED_TYPE_PROSTITUTE || ped->m_nPedType == PED_TYPE_CIVFEMALE) {
        g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_2, ped);
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_PAUSE: {
        if (!m_bBathing) {
            if (m_pSunbatheAnimBlock->IsLoaded && m_pBeachAnimBlock->IsLoaded) {
                return CreateSubTask(TASK_SIMPLE_START_SUNBATHING, ped);
            }
        } else if (ShouldLoadSunbatheAnims() && m_pSunbatheAnimBlock->IsLoaded) {
            return CreateSubTask(TASK_SIMPLE_STOP_SUNBATHING, ped);
        }
        break;
    }
    case TASK_SIMPLE_SUNBATHE: {
        if (!m_pSunbatheAnimBlock->IsLoaded) {
            return CreateSubTask(TASK_SIMPLE_PAUSE, ped);
        }
        if (CanSunbathe() && !m_BathingTimer.IsOutOfTime() && (m_SunbatherType == SUNBATHER_MALE_1 || m_SunbatherType == SUNBATHER_FEMALE_1)) {
            return CreateSubTask(TASK_SIMPLE_IDLE_SUNBATHING, ped);
        }
        return CreateSubTask(TASK_SIMPLE_STOP_SUNBATHING, ped);
    }
    case TASK_SIMPLE_START_SUNBATHING: {
        if (m_pBeachAnimBlock->IsLoaded) {
            m_BathingTimer.Start(CGeneral::GetRandomNumberInRange(20000, 100000));
            const auto sub = CreateSubTask(TASK_SIMPLE_SUNBATHE, ped);
            m_bBathing = true;
            return sub;
        }
        break;
    }
    case TASK_SIMPLE_IDLE_SUNBATHING: {
        if (!m_pBeachAnimBlock->IsLoaded) {
            return CreateSubTask(TASK_FINISHED, ped);
        }
        return CreateSubTask(TASK_SIMPLE_SUNBATHE, ped);
    }
    case TASK_SIMPLE_STOP_SUNBATHING: {
        const auto sub = CreateSubTask(TASK_FINISHED, ped);
        m_bBathing = false;
        return sub;
    }
    default:
        return nullptr;
    }
    return CreateSubTask(TASK_FINISHED, ped);
}

// 0x639CB0
CTask* CTaskComplexSunbathe::CreateFirstSubTask(CPed* ped) {
    constexpr float RAND_RECIPROCAL_2POW15 = 1.f / 32768.f; // 0x858B14

    (void)CGeneral::GetRandomNumber(); // NOTSA: result unused, but advances the RNG

    if (ped->m_nPedType == PED_TYPE_CIVFEMALE) {
        // BUG: yields 2..7 (rand & 0xFFFF is in [0, 65535], not 15 bits), i.e. can go past SUNBATHER_FEMALE_3
        m_SunbatherType = (eSunbatherType)(2 - (int32)((float)(CGeneral::GetRandomNumber() & 0xFFFF) * RAND_RECIPROCAL_2POW15 * -3.f));
    } else if (CPopulation::IsSunbather(ped)) {
        m_SunbatherType = (eSunbatherType)(CGeneral::GetRandomNumber() < 0x3FFF);
    } else {
        m_SunbatherType = SUNBATHER_MALE_2;
    }

    if (!m_pBeachAnimBlock->IsLoaded) {
        CStreaming::RequestModel(IFPToModelId(m_BeachAnimBlockIndex), STREAMING_KEEP_IN_MEMORY);
    }
    if (!m_pSunbatheAnimBlock->IsLoaded) {
        CStreaming::RequestModel(IFPToModelId(m_SunbatheAnimBlockIndex), STREAMING_KEEP_IN_MEMORY);
    }

    m_bBathing = false;

    const auto shouldLoadSunbatheAnims = ShouldLoadSunbatheAnims();
    if (m_bStartStanding && shouldLoadSunbatheAnims) {
        if (!m_pTowel && m_pSunbatheAnimBlock->IsLoaded) {
            return CreateSubTask(TASK_SIMPLE_START_SUNBATHING, ped);
        }
        return CreateSubTask(TASK_SIMPLE_PAUSE, ped);
    }

    if (!m_pBeachAnimBlock->IsLoaded) {
        return !m_bStartStanding && shouldLoadSunbatheAnims
            ? CreateSubTask(TASK_SIMPLE_PAUSE, ped)
            : nullptr;
    }

    // 0x639DCC
    const auto r = (float)(CGeneral::GetRandomNumber() & 0xFFFF);
    m_BathingTimer.m_nStartTime = CTimer::GetTimeInMS();
    m_BathingTimer.m_nInterval  = 20000 - (int32)(r * RAND_RECIPROCAL_2POW15 * -80000.f);
    m_BathingTimer.m_bStarted   = true;
    m_bBathing                  = true;
    return CreateSubTask(TASK_SIMPLE_SUNBATHE, ped);
}

// 0x6381A0
CTask* CTaskComplexSunbathe::ControlSubTask(CPed* ped) {
    if (m_bAborted) {
        return nullptr;
    }

    if (!m_bBeachAnimsReferenced) {
        if (m_pBeachAnimBlock->IsLoaded) {
            CAnimManager::AddAnimBlockRef(m_BeachAnimBlockIndex);
            m_bBeachAnimsReferenced = true;
        } else {
            CStreaming::RequestModel(IFPToModelId(m_BeachAnimBlockIndex), STREAMING_KEEP_IN_MEMORY);
        }
    }

    if (!m_bSunbatheAnimsReferenced && ShouldLoadSunbatheAnims()) {
        if (m_pSunbatheAnimBlock->IsLoaded) {
            CAnimManager::AddAnimBlockRef(m_SunbatheAnimBlockIndex);
            m_bSunbatheAnimsReferenced = true;
        } else {
            CStreaming::RequestModel(IFPToModelId(m_SunbatheAnimBlockIndex), STREAMING_KEEP_IN_MEMORY);
        }
    }

    if (m_bBathing) {
        ped->m_pedIK.bSlopePitch = true;
        ped->bFallenDown         = true;
    }

    if (!CanSunbathe()) {
        m_BathingTimer.Pause();
        if (m_pSubTask->GetTaskType() == TASK_SIMPLE_SUNBATHE) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr); // original passes priority 0 (not the default URGENT)
        }
    }

    return m_pSubTask;
}

// 0x638290
CTask* CTaskComplexSunbathe::CreateSubTask(eTaskType taskType, CPed* ped) {
    if (m_pSubTask && m_pSubTask->GetTaskType() == taskType) {
        return m_pSubTask;
    }

    // Anim ids in the SUNBATHE group are laid out as `base + sunbather type`
    switch (taskType) {
    case TASK_SIMPLE_START_SUNBATHING:
        return new CTaskSimpleRunAnim{ ANIM_GROUP_SUNBATHE, (AnimationId)(ANIM_ID_PARKSIT_M_IN + m_SunbatherType), 4.f, TASK_SIMPLE_START_SUNBATHING, "start sunbathing", true };
    case TASK_SIMPLE_SUNBATHE: {
        const auto animId = (AnimationId)(ANIM_ID_PARKSIT_M_LOOP + m_SunbatherType);
        const auto timeLeft = [&]() -> int32 { // 0x62EB80 (CTaskTimer::GetTimeLeft)
            return m_BathingTimer.m_bStarted
                ? m_BathingTimer.m_nInterval - (int32)(CTimer::GetTimeInMS() - m_BathingTimer.m_nStartTime)
                : 0;
        };
        int32 duration;
        if (m_pSunbatheAnimBlock->IsLoaded && (m_SunbatherType == SUNBATHER_MALE_1 || m_SunbatherType == SUNBATHER_FEMALE_1)) {
            duration = CGeneral::GetRandomNumberInRange(3000, std::min(timeLeft() + 1000, 12000));
        } else {
            duration = timeLeft() + 1000;
        }
        return new CTaskSimpleRunTimedAnim{ ANIM_GROUP_BEACH, animId, 4.f, -4.f, (uint32)duration, TASK_SIMPLE_SUNBATHE, "sunbathe", true };
    }
    case TASK_SIMPLE_PAUSE: {
        if ((!m_pSunbatheAnimBlock->IsLoaded && ShouldLoadSunbatheAnims()) || !m_pBeachAnimBlock->IsLoaded) {
            return new CTaskSimplePause{ 10000 };
        }
        return new CTaskSimplePause{ CGeneral::GetRandomNumberInRange(1000, 5000) };
    }
    case TASK_SIMPLE_IDLE_SUNBATHING: {
        int32 animId;
        if (m_SunbatherType == SUNBATHER_MALE_1) {
            animId = CGeneral::GetRandomNumberInRange(0, 3) + ANIM_ID_PARKSIT_M_IDLEA;
        } else if (m_SunbatherType == SUNBATHER_FEMALE_1) {
            animId = CGeneral::GetRandomNumberInRange(0, 3) + ANIM_ID_PARKSIT_W_IDLEA;
        } else {
            animId = 0; // BUG: original reads an uninitialized stack slot here; unreachable as CreateNextSubTask only gets here for the types above
        }
        return new CTaskSimpleRunAnim{ ANIM_GROUP_SUNBATHE, (AnimationId)animId, 4.f, TASK_SIMPLE_IDLE_SUNBATHING, "idle sunbathing", true };
    }
    case TASK_SIMPLE_STOP_SUNBATHING:
        return new CTaskSimpleRunAnim{ ANIM_GROUP_SUNBATHE, (AnimationId)(ANIM_ID_PARKSIT_M_OUT + m_SunbatherType), 4.f, TASK_SIMPLE_STOP_SUNBATHING, "stop sunbathing", false };
    default:
        return nullptr;
    }
}
