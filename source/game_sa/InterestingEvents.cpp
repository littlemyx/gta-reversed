#include "StdInc.h"

#include "InterestingEvents.h"

auto& g_InterestingEvents = StaticRef<CInterestingEvents>(0xC0B058);

/*
 * Commented hooks aren't tested.
 * */
void CInterestingEvents::InjectHooks() {
    RH_ScopedClass(CInterestingEvents);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x6023A0);
    // RH_ScopedInstall(Destructor, 0x856880, { .Reversed = false }); <-- original is an atexit-style cleanup of the global (takes no `this`), can't be hooked as a member
    RH_ScopedInstall(Add, 0x602590);
    RH_ScopedInstall(ScanForNearbyEntities, 0x605A30);
    RH_ScopedInstall(GetInterestingEvent, 0x6028A0);
    RH_ScopedInstall(InvalidateEvent, 0x602960);
    RH_ScopedInstall(InvalidateNonVisibleEvents, 0x6029C0);
}

// 0x6023A0
CInterestingEvents::CInterestingEvents()
{
    m_nFlags = 0;
    m_b2 = true;
    m_b4 = true;
    m_b8 = true;

    /* Everything missing from here is initialized in the header */

    const auto SetOptions = [=](auto index, auto priority, auto delay, uint32 end = 0) {
        m_nPriorities[index] = priority;
        m_nDelays[index]     = delay;
        m_nEndsOfTime[index] = end;
    };

    SetOptions(INTERESTING_EVENT_0,     5,  2000);
    SetOptions(PEDS_CHATTING,           1,  5000);
    SetOptions(INTERESTING_EVENT_2,     1,  5000);
    SetOptions(INTERESTING_EVENT_3,     1,  5000);
    SetOptions(INTERESTING_EVENT_4,     2,  3000);
    SetOptions(INTERESTING_EVENT_5,     2,  3000);
    SetOptions(INTERESTING_EVENT_6,     2,  3000);
    SetOptions(INTERESTING_EVENT_7,     2,  3000);
    SetOptions(INTERESTING_EVENT_8,     4,  3000);
    SetOptions(INTERESTING_EVENT_9,     4,  3000);
    SetOptions(INTERESTING_EVENT_10,    5,  6000);
    SetOptions(INTERESTING_EVENT_11,    6,  6000);
    SetOptions(INTERESTING_EVENT_12,    6,  8000);
    SetOptions(INTERESTING_EVENT_13,    6,  5000);
    SetOptions(INTERESTING_EVENT_14,    5,  6000);
    SetOptions(INTERESTING_EVENT_15,    9,  6000);
    SetOptions(INTERESTING_EVENT_16,    9,  6000);
    SetOptions(VEHICLE_DAMAGE,          8,  6000);
    SetOptions(INTERESTING_EVENT_18,    7,  6000);
    SetOptions(INTERESTING_EVENT_19,    6,  5000);
    SetOptions(INTERESTING_EVENT_20,    7,  6000);
    SetOptions(INTERESTING_EVENT_21,    8,  8000);
    SetOptions(INTERESTING_EVENT_22,    9,  5000);
    SetOptions(GANG_ATTACKING_PED,      9,  6000);
    SetOptions(GANG_FIGHT,              9,  6000);
    SetOptions(INTERESTING_EVENT_25,    9,  6000);
    SetOptions(ZELDICK_OCCUPATION,      9,  8000);
    SetOptions(EVENT_ATTRACTOR,         10, 4000);
    SetOptions(INTERESTING_EVENT_28,    10, 4000);
}

CInterestingEvents* CInterestingEvents::Constructor() {
    this->CInterestingEvents::CInterestingEvents();
    return this;
}

// 0x856880
CInterestingEvents::~CInterestingEvents() {
    for (auto& event : m_Events) {
        CEntity::ClearReference(event.entity);
    }
}

CInterestingEvents* CInterestingEvents::Destructor() {
    this->CInterestingEvents::~CInterestingEvents();
    return this;
}

// Common part of `Add` and `ScanForNearbyEntities`: Updates the (per-frame cached) scan area center
static void UpdateScanCenter(CInterestingEvents& ie, const CVector& camPos) {
    CPlayerPed* const player = FindPlayerPed();
    const CVector     playerPos = player->GetPosition();

    ie.vec148   = playerPos - camPos;
    ie.vec148.z = 0.f;
    if (ie.vec148.NormaliseAndMag() == 0.f) {
        ie.vec148 = player->GetMatrix().GetForward();
    }
    ie.m_vecCenter = ie.vec148 * ie.m_fRadius + playerPos;
}

// 0x602590
void CInterestingEvents::Add(CInterestingEvents::EType type, CEntity* entity) {
    if (!m_b1 || !entity) {
        return;
    }

    // NOTE: Original code made a copy of the camera position
    const CVector camPos = CCamera::GetActiveCamera().m_vecSource;

    if (m_CurrentFrameCounter != CTimer::GetFrameCounter()) {
        m_CurrentFrameCounter = CTimer::GetFrameCounter();
        UpdateScanCenter(*this, camPos);
    }

    // Is the entity inside of the scan area? (2D)
    const CVector entityPos = entity->GetPosition();
    {
        const float dx = m_vecCenter.x - entityPos.x;
        const float dy = m_vecCenter.y - entityPos.y;
        if (dy * dy + dx * dx > m_fRadius * m_fRadius) {
            return;
        }
    }

    // Is the entity in front of the camera?
    if (m_b2) {
        const float entityDot = (vec148.z * entityPos.z + vec148.y * entityPos.y) + vec148.x * entityPos.x;
        const float camDot    = (camPos.y * vec148.y + camPos.z * vec148.z) + camPos.x * vec148.x;
        if (entityDot + -camDot < 0.f) {
            return;
        }
    }

    if (!CWorld::GetIsLineOfSightClear(camPos, entityPos, true, false, false, false, false, true, false)) {
        return;
    }

    const auto now          = CTimer::GetTimeInMS();
    const auto newPriority  = m_nPriorities[type];
    const bool endTimeOver  = now > m_nEndsOfTime[type];
    for (int32 i = 0; i < MAX_INTERESTING_EVENTS; i++) {
        auto& event = m_Events[i];

        if (!event.entity) {
            event.type = 0;
        } else if (event.type != 0) { // NOTE: Events of type 0 can always be replaced
            const bool isExpired = now > (uint32)m_nDelays[event.type] + event.time;
            if (newPriority < m_nPriorities[event.type] && !isExpired) {
                continue;
            }
            if (!endTimeOver || m_nInterestingEvent == i) {
                continue;
            }
        }

        CEntity::SafeCleanUpRef(event.entity);
        event.type   = type;
        event.entity = entity;
        event.time   = now;
        entity->RegisterReference(&event.entity);

        m_nEndsOfTime[type] = m_b8
            ? now + (m_nDelays[type] >> 1)
            : now;
        return;
    }
}

// 0x605A30
void CInterestingEvents::ScanForNearbyEntities() {
    ZoneScoped;

    if (!m_b1) {
        return;
    }

    if (CTimer::GetTimeInMS() - m_nLastScanTimeUpdate < 500u) {
        return;
    }
    m_nLastScanTimeUpdate = CTimer::GetTimeInMS();

    CPlayerPed* const player = FindPlayerPed();
    if (m_CurrentFrameCounter != CTimer::GetFrameCounter()) {
        m_CurrentFrameCounter = CTimer::GetFrameCounter();
        UpdateScanCenter(*this, CCamera::GetActiveCamera().m_vecSource);
    }

    // NOTE: Not using `CWorld::GetSectorX/Y` here, as the original code clamps the values to the area
    const auto GetSector = [](float coord) {
        return (int32)std::floor(coord * 0.02f + 60.0f);
    };
    const int32 startSectorX = std::max(GetSector(m_vecCenter.x - m_fRadius), 0);
    const int32 startSectorY = std::max(GetSector(m_vecCenter.y - m_fRadius), 0);
    const int32 endSectorX   = std::min(GetSector(m_fRadius + m_vecCenter.x), 119);
    const int32 endSectorY   = std::min(GetSector(m_fRadius + m_vecCenter.y), 119);

    CWorld::AdvanceCurrentScanCode();
    player->SetCurrentScanCode();

    for (int32 sectorY = startSectorY; sectorY <= endSectorY; ++sectorY) {
        for (int32 sectorX = startSectorX; sectorX <= endSectorX; ++sectorX) {
            auto& rs = CWorld::GetRepeatSector(sectorX, sectorY);

            for (CPed* const ped : rs.Peds) {
                if (ped->IsScanCodeCurrent()) {
                    continue;
                }
                ped->SetCurrentScanCode();

                if (ped->m_nPedState == PEDSTATE_DEAD) {
                    continue;
                }

                CEntity* const entity = ped->bInVehicle && ped->m_pVehicle
                    ? (CEntity*)ped->m_pVehicle
                    : ped;

                switch (ped->m_nPedType) {
                case PED_TYPE_COP:       Add(INTERESTING_EVENT_5, entity); break;
                case PED_TYPE_CRIMINAL:  Add(INTERESTING_EVENT_6, entity); break;
                case PED_TYPE_PROSTITUTE:Add(INTERESTING_EVENT_4, entity); break;
                default:
                    if (IsPedTypeGang(ped->m_nPedType)) {
                        Add(INTERESTING_EVENT_7, entity);
                    }
                    break;
                }
            }

            for (CVehicle* const vehicle : rs.Vehicles) {
                if (vehicle->IsScanCodeCurrent()) {
                    continue;
                }
                vehicle->SetCurrentScanCode();

                if (vehicle->physicalFlags.bRenderScorched) {
                    continue;
                }
                if (!vehicle->m_pDriver) {
                    continue;
                }

                const auto style = vehicle->m_autoPilot.m_nCarDrivingStyle;
                if (style == DRIVING_STYLE_STOP_FOR_CARS || style == DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS) {
                    continue;
                }

                Add(INTERESTING_EVENT_14, vehicle);
            }
        }
    }
}

// 0x6028A0
TInterestingEvent* CInterestingEvents::GetInterestingEvent() {
    const auto now = CTimer::GetTimeInMS();

    // Is the current event still valid?
    if (m_b4 && m_nInterestingEvent != -1) {
        auto* const cur = &m_Events[m_nInterestingEvent];
        if (cur->entity && now < (uint32)m_nDelays[cur->type] + cur->time) {
            return cur;
        }
    }

    // Find a new one
    int32 bestPriority = 0;
    int32 best         = -1;
    for (int32 i = 0; i < MAX_INTERESTING_EVENTS; i++) {
        const auto& event = m_Events[i];
        if (!event.entity) {
            continue;
        }
        if (now >= (uint32)m_nDelays[event.type] + event.time) { // Expired
            continue;
        }
        if ((int32)m_nPriorities[event.type] > bestPriority || (rand() & 0xFFFF) < 0x80) {
            bestPriority = m_nPriorities[event.type];
            best         = i;
        }
    }
    m_nInterestingEvent = (int8)best;

    return best == -1 ? nullptr : &m_Events[best];
}

// 0x602960
void CInterestingEvents::InvalidateEvent(const TInterestingEvent* event) {
    for (auto index = 0; index < MAX_INTERESTING_EVENTS; index++) {
        TInterestingEvent* tevent = &m_Events[index];
        if (tevent != event)
            continue;

        tevent->time = 0;
        CEntity::ClearReference(tevent->entity);
        if (m_nInterestingEvent == index) {
            m_nInterestingEvent = -1;
        }
    }
}

// 0x6029C0
void CInterestingEvents::InvalidateNonVisibleEvents() {
    const auto& camPos = CCamera::GetActiveCamera().m_vecSource;
    for (auto i = 0; i < MAX_INTERESTING_EVENTS; i++) {
        TInterestingEvent& event = m_Events[i];
        if (!event.entity)
            continue;

        CVector pos = event.entity->GetPosition();
        if (CWorld::GetIsLineOfSightClear(camPos, pos, true, false, false, false, false, true, false))
            continue;

        event.time = 0;
        CEntity::SafeCleanUpRef(event.entity);
        if (m_nInterestingEvent == i) {
            m_nInterestingEvent = -1;
        }
    }
}
