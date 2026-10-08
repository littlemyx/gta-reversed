#include "StdInc.h"

#include "Garage.h"
#include "Garages.h"

void CGarage::InjectHooks() {
    RH_ScopedClass(CGarage);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(BuildRotatedDoorMatrix, 0x4479F0);
    RH_ScopedInstall(TidyUpGarageClose, 0x449D10);
    RH_ScopedInstall(TidyUpGarage, 0x449C50);
    RH_ScopedInstall(StoreAndRemoveCarsForThisHideOut, 0x449900);
    // RH_ScopedInstall(EntityHasASphereWayOutsideGarage, 0x449050);
    RH_ScopedInstall(RemoveCarsBlockingDoorNotInside, 0x449690);
    // RH_ScopedInstall(IsEntityTouching3D, 0x448EE0);
    // RH_ScopedInstall(IsEntityEntirelyOutside, 0x448D30);
    // RH_ScopedInstall(IsStaticPlayerCarEntirelyInside, 0x44A830);
    // RH_ScopedInstall(IsEntityEntirelyInside3D, 0x448BE0);
    // RH_ScopedInstall(IsPointInsideGarage, 0x448740);
    // RH_ScopedInstall(PlayerArrestedOrDied, 0x4486C0);
    RH_ScopedInstall(OpenThisGarage, 0x447D50);
    RH_ScopedInstall(CloseThisGarage, 0x447D70);
    RH_ScopedInstall(InitDoorsAtStart, 0x447600);
    // RH_ScopedInstall(IsPointInsideGarage, 0x4487D0);
    // RH_ScopedInstall(Update, 0x44AA50);
    RH_ScopedInstall(FindDoorsWithGarage, 0x449FF0);

    {
        RH_ScopedClass(CStoredCar);
        RH_ScopedCategoryGlobal();

        RH_ScopedInstall(StoreCar, 0x449760);
    }
}

// 0x4479F0
void CGarage::BuildRotatedDoorMatrix(CEntity* entity, float fDoorPosition) {
    const auto fAngle = fDoorPosition * -HALF_PI;
    const auto fSin = sin(fAngle);
    const auto fCos = cos(fAngle);
    CMatrix& matrix = entity->GetMatrix();

    const auto& vecForward = matrix.GetForward();
    matrix.GetUp() = CVector(-fSin * vecForward.y, fSin * vecForward.x, fCos);
    matrix.GetRight() = CrossProduct(vecForward, matrix.GetUp());
}

// 0x59C890 - the original evaluation order; the sum stays in the FPU registers (extended precision), stored as float
static CVector TransformPointExt(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x) + p.x),
        (float)((((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y) + p.y),
        (float)((((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y) + p.z)
    };
}

// 0x449D10
void CGarage::TidyUpGarageClose() {
    auto* const pool = GetVehiclePool();
    for (int32 i = pool->GetSize() - 1; i != 0; i--) { // NOTE: slot 0 is never visited (original loop ends at 0 exclusive)
        auto* const veh = pool->GetAt(i);
        if (!veh) {
            continue;
        }
        if (veh->m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE && veh->m_nVehicleType != VEHICLE_TYPE_BIKE) {
            continue;
        }
        if (veh->GetStatus() != STATUS_WRECKED || !IsEntityTouching3D(veh)) {
            continue;
        }

        bool bRemove{};
        if (m_nDoorState == GARAGE_DOOR_CLOSED) {
            bRemove = true;
        } else {
            // Remove if any of the col spheres is (partly) outside of the garage
            const auto* const colData = veh->GetColModel()->m_pColData;
            for (int32 s = 0; s < colData->m_nNumSpheres; s++) {
                const auto& sphere = colData->m_pSpheres[s];
                if (!IsPointInsideGarage(TransformPointExt(*veh->m_matrix, sphere.m_vecCenter), sphere.m_fRadius)) { // 0x449DFC
                    bRemove = true;
                }
            }
        }
        if (bRemove) {
            CWorld::Remove(veh);
            delete veh;
        }
    }
}

// 0x449C50
void CGarage::TidyUpGarage() {
    auto* const pool = GetVehiclePool();
    for (int32 i = pool->GetSize() - 1; i != 0; i--) { // NOTE: slot 0 is never visited (original loop ends at 0 exclusive)
        auto* const veh = pool->GetAt(i);
        if (!veh) {
            continue;
        }
        if (veh->m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE && veh->m_nVehicleType != VEHICLE_TYPE_BIKE) {
            continue;
        }
        if (!IsPointInsideGarage(veh->GetPosition())) {
            continue;
        }
        // 0.5f @ 0x858B8C; note: matrix is dereferenced unchecked in the original too
        if (veh->GetStatus() == STATUS_WRECKED || veh->GetUp().z < 0.5f) {
            CWorld::Remove(veh);
            delete veh;
        }
    }
}

// 0x449900
void CGarage::StoreAndRemoveCarsForThisHideOut(CStoredCar* storedCars, int32 maxSlot) {
    maxSlot = std::min<int32>(maxSlot, NUM_GARAGE_STORED_CARS);

    for (auto i = 0; i < NUM_GARAGE_STORED_CARS; i++)
        storedCars[i].Clear();

    auto pool = GetVehiclePool();
    auto storedCarIdx{0u};
    for (auto i = pool->GetSize(); i; i--) {
        if (auto vehicle = pool->GetAt(i - 1)) {
            if (IsPointInsideGarage(vehicle->GetPosition()) && vehicle->GetCreatedBy() != MISSION_VEHICLE) {
                if (storedCarIdx < static_cast<uint32>(maxSlot) && !EntityHasASphereWayOutsideGarage(vehicle, 1.0f)) {
                    storedCars[storedCarIdx++].StoreCar(vehicle);
                }

                FindPlayerInfo().CancelPlayerEnteringCars(vehicle);
                CWorld::Remove(vehicle);
                delete vehicle;
            }
        }
    }

    // Clear slots with no vehicles in it
    for (auto i = storedCarIdx; i < NUM_GARAGE_STORED_CARS; i++)
        storedCars[i].Clear();
}

// 0x449050
bool CGarage::EntityHasASphereWayOutsideGarage(CEntity* entity, float fRadius) {
    return plugin::CallMethodAndReturn<bool, 0x449050, CGarage*, CEntity*, float>(this, entity, fRadius);
}

// 0x449690
void CGarage::RemoveCarsBlockingDoorNotInside() {
    auto* const pool = GetVehiclePool();
    for (int32 i = pool->GetSize(); i-- > 0;) {
        auto* const veh = pool->GetAt(i);
        if (!veh) {
            continue;
        }
        if (!IsEntityTouching3D(veh)) {
            continue;
        }
        if (IsPointInsideGarage(veh->GetPosition()) || veh->vehicleFlags.bIsLocked || !veh->CanBeDeleted()) {
            continue;
        }
        CWorld::Remove(veh);
        delete veh;
        return; // Only one car is removed per call
    }
}

// 0x448EE0
bool CGarage::IsEntityTouching3D(CEntity* entity) {
    return plugin::CallMethodAndReturn<bool, 0x448EE0, CGarage*, CEntity*>(this, entity);
}

// 0x448D30
bool CGarage::IsEntityEntirelyOutside(CEntity* entity, float radius) {
    return plugin::CallMethodAndReturn<bool, 0x448D30, CGarage*, CEntity*, float>(this, entity, radius);
}

// 0x44A830
bool CGarage::IsStaticPlayerCarEntirelyInside() {
    return plugin::CallMethodAndReturn<bool, 0x44A830, CGarage*>(this);
}

// 0x448BE0
bool CGarage::IsEntityEntirelyInside3D(CEntity* entity, float radius) {
    return plugin::CallMethodAndReturn<bool, 0x448BE0, CGarage*, CEntity*, float>(this, entity, radius);
}

// 0x448740
bool CGarage::IsPointInsideGarage(CVector point) {
    return plugin::CallMethodAndReturn<bool, 0x448740, CGarage*, CVector>(this, point);
}

// 0x4486C0
eGarageDoorState CGarage::PlayerArrestedOrDied() {
    return plugin::CallMethodAndReturn<eGarageDoorState, 0x4486C0, CGarage*>(this);
}

// 0x447D50
void CGarage::OpenThisGarage() {
  if ( m_nDoorState == GARAGE_DOOR_CLOSED
    || m_nDoorState == GARAGE_DOOR_CLOSING
    || m_nDoorState == GARAGE_DOOR_CLOSED_DROPPED_CAR)
  {
    m_nDoorState = GARAGE_DOOR_OPENING;
  }
}

// 0x447D70
void CGarage::CloseThisGarage() {
    if (m_nDoorState == GARAGE_DOOR_OPEN || m_nDoorState == GARAGE_DOOR_OPENING)
        m_nDoorState = GARAGE_DOOR_CLOSING;
}

// 0x447600
void CGarage::InitDoorsAtStart() {
    m_nFlags      = (m_nFlags & 0x39) | 0x40; // keep bits 0,3,4,5; clear bits 1,2,7; set `m_bDoorClosed`
    m_nDoorState  = GARAGE_DOOR_CLOSED;
    m_nTimeToOpen = 0;

    switch (m_nType) {
    case 1:
    case 6: case 7: case 8: case 9: case 10: case 11: case 12:
    case 14: case 15: case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23: case 24: case 25:
    case 26: case 27: case 28: case 29: case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
    case 38: case 39: case 40: case 41: case 42: case 43: case 44: case 45:
        m_nDoorState    = GARAGE_DOOR_CLOSED;
        m_fDoorPosition = 0.0f;
        break;
    case BOMBSHOP_TIMED:
    case BOMBSHOP_ENGINE:
    case BOMBSHOP_REMOTE:
    case PAYNSPRAY: // 2..5
        m_nDoorState    = GARAGE_DOOR_OPEN;
        m_fDoorPosition = 1.0f;
        break;
    default:
        break;
    }
}

// 0x4487D0
bool CGarage::IsPointInsideGarage(CVector point, float radius) {
    return plugin::CallMethodAndReturn<bool, 0x4487D0, CGarage*, CVector, float>(this, point, radius);
}

// 0x44AA50
void CGarage::Update(int32 garageId) {
    plugin::CallMethod<0x44AA50, CGarage*>(this, garageId);
}

bool CGarage::IsHideOut() const {
    switch (m_nType) {
    case eGarageType::SAFEHOUSE_GANTON:
    case eGarageType::SAFEHOUSE_SANTAMARIA:
    case eGarageType::SAGEHOUSE_ROCKSHORE:
    case eGarageType::SAFEHOUSE_FORTCARSON:
    case eGarageType::SAFEHOUSE_VERDANTMEADOWS:
    case eGarageType::SAFEHOUSE_DILLIMORE:
    case eGarageType::SAFEHOUSE_PRICKLEPINE:
    case eGarageType::SAFEHOUSE_WHITEWOOD:
    case eGarageType::SAFEHOUSE_PALOMINOCREEK:
    case eGarageType::SAFEHOUSE_REDSANDSWEST:
    case eGarageType::SAFEHOUSE_ELCORONA:
    case eGarageType::SAFEHOUSE_MULHOLLAND:
    case eGarageType::SAFEHOUSE_CALTONHEIGHTS:
    case eGarageType::SAFEHOUSE_PARADISO:
    case eGarageType::SAFEHOUSE_DOHERTY:
    case eGarageType::SAFEHOUSE_HASHBURY:
    case eGarageType::HANGAR_ABANDONED_AIRPORT:
        return true;
    default:
        return false;
    }
}

// 0x44A9C0
bool CGarage::IsGarageEmpty() {
    return plugin::CallMethodAndReturn<bool, 0x44A9C0, CGarage*>(this);

    CVector cornerA = { m_fLeftCoord, m_fFrontCoord, m_vPosn.z };
    CVector cornerB = { m_fRightCoord, m_fBackCoord, m_fTopZ   };

    int16 outCount[2];
    CEntity* outEntities[16];
    CWorld::FindObjectsIntersectingCube(cornerA, cornerB, outCount, static_cast<int16>(std::size(outEntities)), outEntities, false, true, true, false, false);
    if (outCount[0] <= 0)
        return true;

    int16 entityIndex = 0;

    while (!IsEntityTouching3D(outEntities[entityIndex])) {
        if (++entityIndex >= outCount[0])
            return true;
    }
    return false;
}

/*
void CGarage::CenterCarInGarage(CEntity* entity) {
    auto vehicle = FindPlayerVehicle();
    if (IsAnyOtherCarTouchingGarage(vehicle))
        return;

    auto player = FindPlayerPed();
    if (IsAnyOtherPedTouchingGarage(player))
        return;

    auto pos = entity->GetPosition();

    const auto halfX = (m_fRightCoord + m_fLeftCoord) * 0.5f;
    const auto halfY = (m_fBackCoord + m_fFrontCoord) * 0.5f;
    CVector p1{
        halfX - pos.x,
        halfY - pos.y,
        pos.z - pos.z
    };

    auto dist = p1.Magnitude();
    if (dist >= 0.4f) {
        auto x = halfX - pos.x * 0.4f / dist + pos.x;
        auto y = 0.4f / dist * halfY - pos.y + pos.y;
    } else {
        auto x = halfX;
        auto y = halfY;
    }

    if (!IsEntityEntirelyInside3D(entity, 0.3f))
        entity->SetPosn(entity->GetPosition());
}
*/

// 0x5D3020
void CSaveGarage::CopyGarageIntoSaveGarage(Const CGarage& g) {
    m_nType         = g.m_nType;
    m_nDoorState    = g.m_nDoorState;
    m_nFlags        = g.m_nFlags;
    m_vPosn         = g.m_vPosn;
    m_vDirectionA   = g.m_vDirectionA;
    m_vDirectionB   = g.m_vDirectionB;
    m_fTopZ         = g.m_fTopZ;
    m_fWidth        = g.m_fWidth;
    m_fHeight       = g.m_fHeight;
    m_fLeftCoord    = g.m_fLeftCoord;
    m_fRightCoord   = g.m_fRightCoord;
    m_fFrontCoord   = g.m_fFrontCoord;
    m_fBackCoord    = g.m_fBackCoord;
    m_fDoorPosition = g.m_fDoorPosition;
    m_nTimeToOpen   = g.m_nTimeToOpen;
    m_nOriginalType = g.m_nOriginalType;
    strcpy_s(m_anName, g.m_anName);
}

// 0x5D30C0
void CSaveGarage::CopyGarageOutOfSaveGarage(CGarage& g) const {
    g.m_nType         = m_nType;
    g.m_nDoorState    = m_nDoorState;
    g.m_nFlags        = m_nFlags;
    g.m_vPosn         = m_vPosn;
    g.m_vDirectionA   = m_vDirectionA;
    g.m_vDirectionB   = m_vDirectionB;
    g.m_fTopZ         = m_fTopZ;
    g.m_fWidth        = m_fWidth;
    g.m_fHeight       = m_fHeight;
    g.m_fLeftCoord    = m_fLeftCoord;
    g.m_fRightCoord   = m_fRightCoord;
    g.m_fFrontCoord   = m_fFrontCoord;
    g.m_fBackCoord    = m_fBackCoord;
    g.m_fDoorPosition = m_fDoorPosition;
    g.m_nTimeToOpen   = m_nTimeToOpen;
    g.m_nOriginalType = m_nOriginalType;
    g.m_pTargetCar    = nullptr;
    strcpy_s(g.m_anName, m_anName);
}

// todo move
// 0x449760
void CStoredCar::StoreCar(CVehicle* vehicle) {
    m_wModelIndex = vehicle->m_nModelIndex;
    m_vPosn       = vehicle->GetPosition();

    // Forward vector packed as signed bytes (x100); `_ftol` truncates, the product is kept at extended precision
    const auto& fwd  = vehicle->m_matrix->GetForward();
    m_nPackedForwardX = static_cast<uint8>(static_cast<int32>(double(fwd.x) * 100.0));
    m_nPackedForwardY = static_cast<uint8>(static_cast<int32>(double(fwd.y) * 100.0));
    m_nPackedForwardZ = static_cast<uint8>(static_cast<int32>(double(fwd.z) * 100.0));

    m_nPrimaryColor    = vehicle->m_nPrimaryColor;
    m_nSecondaryColor  = vehicle->m_nSecondaryColor;
    m_nTertiaryColor   = vehicle->m_nTertiaryColor;
    m_nQuaternaryColor = vehicle->m_nQuaternaryColor;
    m_nRadioStation    = static_cast<uint8>(vehicle->m_vehicleAudio.m_AuSettings.RadioStation);
    m_nHandlingFlags   = static_cast<uint32>(vehicle->m_nHandlingFlagsIntValue);
    m_anCompsToUse[0]  = vehicle->m_anExtras[0];
    m_anCompsToUse[1]  = vehicle->m_anExtras[1];

    m_nStoredCarFlags = 0;
    _pad0             = 0; // The original clears a 16 bit word here
    if (vehicle->physicalFlags.bBulletProof) {
        m_nStoredCarFlags |= 0x01;
    }
    if (vehicle->physicalFlags.bFireProof) {
        m_nStoredCarFlags |= 0x02;
    }
    if (vehicle->physicalFlags.bExplosionProof) {
        m_nStoredCarFlags |= 0x04;
    }
    if (vehicle->physicalFlags.bCollisionProof) {
        m_nStoredCarFlags |= 0x08;
    }
    if (vehicle->physicalFlags.bMeleeProof) {
        m_nStoredCarFlags |= 0x10;
    }
    if (vehicle->vehicleFlags.bUpgradedStereo) {
        m_nStoredCarFlags |= 0x20;
    }
    if (vehicle->handlingFlags.bHydraulicInst) {
        m_nStoredCarFlags |= 0x40;
    }
    if (vehicle->handlingFlags.bNosInst) {
        m_nStoredCarFlags |= 0x80;
    }

    // NOTE: not reset for other vehicle types (stale value stays)
    if (vehicle->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE || vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        m_nBombType = vehicle->m_nBombOnBoard;
    }

    std::ranges::copy(vehicle->m_anUpgrades, m_awCarMods);
    m_nPaintJob     = static_cast<uint8>(vehicle->GetRemapIndex());
    m_nNitroBoosts  = vehicle->m_nNitroBoosts;
}

// 0x447E40
CVehicle* CStoredCar::RestoreCar() {
    return plugin::CallMethodAndReturn<CVehicle*, 0x447E40, CStoredCar*>(this);
}

// 0x449FF0
void CGarage::FindDoorsWithGarage(CObject** ppFirstDoor, CObject** ppSecondDoor) {
    *ppSecondDoor = nullptr;
    *ppFirstDoor  = nullptr;

    // Center of the garage's door area (intermediates are rounded to float where the original spills them)
    const auto garageIdx = static_cast<int8>(this - CGarages::aGarages);
    const float halfW    = m_fWidth * 0.5f;
    const float t1       = (float)((double)m_vDirectionA.x * halfW);
    const float x1       = t1 + m_vPosn.x;
    const double y1      = (double)m_vDirectionA.y * halfW + m_vPosn.y;
    const float halfH    = m_fHeight * 0.5f;
    const float t3       = (float)((double)m_vDirectionB.x * halfH);
    const float cx       = t3 + x1;
    const float cy       = (float)((double)m_vDirectionB.y * halfH + y1);

    float dist1  = 99999.9f; // 0x85999C
    float dist2  = 99999.9f;
    auto& pool   = *GetObjectPool();
    for (auto i = pool.GetSize(); i-- > 0;) {
        if (pool.IsFreeSlotAtIndex(i)) {
            continue;
        }
        auto* const obj = pool.GetAt(i);
        if (!obj || obj->m_nGarageDoorGarageIndex != garageIdx) {
            continue;
        }

        const auto& pos = obj->GetPosition();
        const double dx = (double)cx - pos.x;
        const double dy = (double)cy - pos.y;
        const float dist = (float)std::sqrt(dx * dx + dy * dy);

        if (!*ppFirstDoor) {
            *ppFirstDoor = obj;
            dist1        = dist;
            continue;
        }
        if (!(dist < dist1)) { // (FCOM + JP): >= or unordered
            if (!*ppSecondDoor || dist < dist2) {
                *ppSecondDoor = obj;
                dist2         = dist;
            }
        } else {
            *ppSecondDoor = *ppFirstDoor;
            dist2         = dist1;
            *ppFirstDoor  = obj;
            dist1         = dist;
        }
    }
}
