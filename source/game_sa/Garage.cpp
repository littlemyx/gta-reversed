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
    RH_ScopedInstall(Update, 0x44AA50);
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

// 0x447D80
float CGarage::CalcDistToGarageRectangleSquared(float x, float y) {
    return plugin::CallMethodAndReturn<float, 0x447D80, CGarage*, float, float>(this, x, y);
}

// 0x447720
bool CGarage::RightModTypeForThisGarage(CVehicle* vehicle) {
    return plugin::CallMethodAndReturn<bool, 0x447720, CGarage*, CVehicle*>(this, vehicle);
}

// 0x448330
void CGarage::NeatlyLineUpStoredCars(CStoredCar* car) {
    plugin::CallMethod<0x448330, CGarage*, CStoredCar*>(this, car);
}

// 0x448550
bool CGarage::RestoreCarsForThisHideOut(CStoredCar* car) {
    return plugin::CallMethodAndReturn<bool, 0x448550, CGarage*, CStoredCar*>(this, car);
}

// 0x4485C0
bool CGarage::RestoreCarsForThisImpoundingGarage(CStoredCar* car) {
    return plugin::CallMethodAndReturn<bool, 0x4485C0, CGarage*, CStoredCar*>(this, car);
}

// 0x448E50
bool CGarage::IsPlayerOutsideGarage(float fRadius) {
    return plugin::CallMethodAndReturn<bool, 0x448E50, CGarage*, float>(this, fRadius);
}

// 0x449100
bool CGarage::IsAnyOtherCarTouchingGarage(CVehicle* ignoredVehicle) {
    return plugin::CallMethodAndReturn<bool, 0x449100, CGarage*, CVehicle*>(this, ignoredVehicle);
}

// 0x449220
void CGarage::ThrowCarsNearDoorOutOfGarage(CVehicle* ignoredVehicle) {
    plugin::CallMethod<0x449220, CGarage*, CVehicle*>(this, ignoredVehicle);
}

// 0x4494F0
bool CGarage::IsAnyCarBlockingDoor() {
    return plugin::CallMethodAndReturn<bool, 0x4494F0, CGarage*>(this);
}

// 0x4495F0
int32 CGarage::CountCarsWithCenterPointWithinGarage(CVehicle* ignoredVehicle) {
    return plugin::CallMethodAndReturn<int32, 0x4495F0, CGarage*, CVehicle*>(this, ignoredVehicle);
}

// 0x449A50
void CGarage::StoreAndRemoveCarsForThisImpoundingGarage(CStoredCar* storedCars, int32 iMaxSlot) {
    plugin::CallMethod<0x449A50, CGarage*, CStoredCar*, int32>(this, storedCars, iMaxSlot);
}

// 0x44A660
bool CGarage::SlideDoorOpen() {
    return plugin::CallMethodAndReturn<bool, 0x44A660, CGarage*>(this);
}

// 0x44A750
bool CGarage::SlideDoorClosed() {
    return plugin::CallMethodAndReturn<bool, 0x44A750, CGarage*>(this);
}

// ---------------------------------------------------------------------------------------------------------------------
// CGarage::Update (0x44AA50) - a single function in the original, split here into one helper per garage type.
//
// Type dispatch (jump table @ 0x44C80C: byte index = type - 1, then dword table @ 0x44C7D4):
//   case 0 (0x44B7EE): ONLY_TARGET_VEH                      case 7  (0x44BCAA): SCRIPT_CONTROLLED
//   case 1 (0x44B419): BOMBSHOP_TIMED/ENGINE/REMOTE         case 8  (0x44C1FF): STAY_OPEN_WITH_CAR_INSIDE
//   case 2 (0x44AC16): PAYNSPRAY                            case 9  (0x44C2F3): SCRIPT_OPEN_FREEZE_WHEN_CLOSING
//   case 3 (0x44B9FF): UNKN_CLOSESONTOUCH                   case 10 (0x44C395): IMPOUND_LS/SF/LV
//   case 4 (0x44BA5C): OPEN_FOR_TARGET_FREEZE_PLAYER, CLOSE_WITH_CAR_DONT_OPEN_AGAIN
//   case 5 (0x44BCA4): SCRIPT_ONLY_OPEN                     case 11 (0x44C564): TUNING_*
//   case 6 (0x44BCDE): safehouses + hangars                 case 12 (0x44C70A): BURGLARY
//   case 13 (0x44C7C4): types 6-10, 12, 13, 22 (nothing)
// Each case then switches on `m_nDoorState` (jump tables @ 0x44C83C, 0x44C850, 0x44C864, 0x44C874, 0x44C88C, 0x44C89C, 0x44C8AC).
// ---------------------------------------------------------------------------------------------------------------------
namespace {
// 0x44B3C1
void CallOffChaseNearGarage(const CGarage& g) {
    CWorld::CallOffChaseForArea(g.m_fLeftCoord - 10.0f, g.m_fFrontCoord - 10.0f, g.m_fRightCoord + 10.0f, g.m_fBackCoord + 10.0f); // 10.0f @ 0x85862C
}

// 0x44B4F8 (the Pad part of it is also used on its own, see the callers)
void HoldPlayerInGarage() {
    CPad::GetPad(0)->bPlayerAwaitsInGarage = true;
    FindPlayerWanted()->m_bPoliceBackOffGarage = true;
}

// 0x44BBCC
void ReleasePlayerFromGarage() {
    CPad::GetPad(0)->bPlayerAwaitsInGarage = false;
    FindPlayerWanted()->m_bPoliceBackOffGarage = false;
}

// Squared distance of the player from the garage's center (0x44B802 and the same sequence in other cases).
// The original re-queries the player's position for every operand; the sum is kept at extended precision.
double PlayerDistSqFromGarageCenter(const CGarage& g) {
    const CVector pc = FindPlayerCoors();
    const double  cx = ((double)g.m_fRightCoord + g.m_fLeftCoord) * 0.5;
    const double  cy = ((double)g.m_fFrontCoord + g.m_fBackCoord) * 0.5;
    const float   dyF = (float)(pc.y - cy); // spilled to a float temp
    return (pc.y - cy) * (double)dyF + (pc.x - cx) * (pc.x - cx);
}

// 0x44B9C3 (type specific checks come before): open once the player's vehicle is close enough
void OpenIfPlayerVehicleNear(CGarage& g, float maxDistSq) {
    const auto  pos = FindPlayerVehicle(-1, false)->GetPosition();
    const float d   = g.CalcDistToGarageRectangleSquared(pos.x, pos.y);
    if (!(d < maxDistSq)) {
        return;
    }
    g.m_nDoorState = GARAGE_DOOR_OPENING; // 0x44C6F9
}

// 0x44BCBE
void OpenDoorStep(CGarage& g) {
    if (g.SlideDoorOpen()) {
        g.m_nDoorState = GARAGE_DOOR_OPEN;
    }
}

// 0x44C69D
void CloseDoorStep(CGarage& g) {
    if (g.SlideDoorClosed()) {
        g.m_nDoorState = GARAGE_DOOR_CLOSED;
    }
}

// 0x44C67B
void ThrowOutPlayerVehicleAndCloseDoorStep(CGarage& g) {
    if (FindPlayerVehicle(-1, false)) {
        g.ThrowCarsNearDoorOutOfGarage(FindPlayerVehicle(-1, false));
    }
    CloseDoorStep(g);
}

// 0x44C695
void ThrowOutTargetCarAndCloseDoorStep(CGarage& g) {
    if (g.m_pTargetCar) {
        g.ThrowCarsNearDoorOutOfGarage(g.m_pTargetCar);
    }
    CloseDoorStep(g);
}

// 0x44B79C (shared by the pay'n'spray and the bomb shops): door opening
void ShopOpeningState(CGarage& g) {
    if (g.SlideDoorOpen()) {
        g.m_nDoorState = GARAGE_DOOR_WAITING_PLAYER_TO_EXIT;
    }
    if (!(g.m_fDoorPosition > 0.5f)) {
        return;
    }
    CPad::GetPad(0)->bPlayerAwaitsInGarage = false;
    FindPlayerWanted()->m_bPoliceBackOffGarage = false;
}

// 0x44B40C (shared by the pay'n'spray and the bomb shops): waiting for the player to drive out
void ShopWaitingForPlayerState(CGarage& g) {
    if (g.IsPlayerOutsideGarage(0.0f)) {
        g.m_nDoorState = GARAGE_DOOR_OPEN;
    }
}

// 0x44AC16
void UpdatePaynSpray(CGarage& g, int32 garageId) {
    if (!(FindPlayerCoors().z < 950.0f)) { // 950.0f @ 0x858F4C
        return;
    }
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_CLOSED: { // 0x44AEC0
        if (CGarages::NoResprays) {
            return;
        }
        if (!(CTimer::GetTimeInMS() > g.m_nTimeToOpen)) {
            CallOffChaseNearGarage(g);
            return;
        }
        g.m_nDoorState = GARAGE_DOOR_OPENING;

        bool       bServiced = false; // [esp+0x17]: wanted level cleared or the vehicle was damaged
        const bool bHadWanted = FindPlayerWanted()->m_WantedLevel != eWantedLevel::WANTED_CLEAN; // [esp+0x1c]
        if (bHadWanted) {
            bServiced = true;
            FindPlayerWanted()->ClearWantedLevelAndGoOnParole();
        }

        bool bColourChanged{};
        if (auto* const veh = FindPlayerVehicle(-1, false)) {
            if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE || veh->m_nVehicleType == VEHICLE_TYPE_BIKE) {
                if (veh->m_fHealth < 970.0f) { // 0x859A40
                    bServiced = true;
                }
                veh->m_fHealth = 1000.0f > veh->m_fHealth ? 1000.0f : veh->m_fHealth; // 0x420800: `a > b ? a : b` (NaN => b)
                // BUG: the original writes the automobile's burn timer (+0x8E4) for bikes too; harmless as it stays inside the pool slot
                if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
                    static_cast<CAutomobile*>(veh)->m_fBurnTimer = 0.0f; // +0x8E4
                } else {
                    static_cast<CBike*>(veh)->m_BlowUpTimer = 0.0f; // +0x7BC
                }
                veh->Fix();
                CStats::IncrementStat(STAT_VEHICLE_RESPRAYS, 1.0f);

                // Turn the vehicle back on its wheels if it is upside down
                if (veh->GetMatrix().GetUp().z < 0.0f) {
                    auto& m = veh->GetMatrix();
                    m.GetUp().x    = -m.GetUp().x;
                    m.GetUp().y    = -m.GetUp().y;
                    m.GetUp().z    = -m.GetUp().z;
                    m.GetRight().x = -m.GetRight().x;
                    m.GetRight().y = -m.GetRight().y;
                    m.GetRight().z = -m.GetRight().z;
                }

                // 0x44B0D7: pick new colours
                // BUG: `autoFlags` is read without checking the vehicle type (a bike here)
                if (!static_cast<CAutomobile*>(veh)->autoFlags.bShouldNotChangeColour && veh->GetRemapIndex() < 0) {
                    uint8 prim, sec, tert, quat;
                    veh->GetVehicleModelInfo()->ChooseVehicleColour(prim, sec, tert, quat, 1); // 0x4C8500
                    if (veh->m_nPrimaryColor != prim || veh->m_nSecondaryColor != sec || veh->m_nTertiaryColor != tert || veh->m_nQuaternaryColor != quat) {
                        bColourChanged = true;
                    }
                    veh->m_nPrimaryColor    = prim;
                    veh->m_nSecondaryColor  = sec;
                    veh->m_nTertiaryColor   = tert;
                    veh->m_nQuaternaryColor = quat;
                    veh->SetRemap(-1);
                    if (bColourChanged) {
                        // 0x4AB290; 0x3F19999A, 0x3F333333, 0x3ECCCCCD
                        FxPrtMult_c fxMults{ 1.0f, 0.0f, 0.0f, 0.6f, 0.7f, 1.0f, 0.4f };
                        const auto colour = CVehicleModelInfo::ms_vehicleColourTable[veh->m_nPrimaryColor];
                        fxMults.m_Color.red   = (float)colour.r * (1.0f / 255.0f); // 0x859A3C
                        fxMults.m_Color.green = (float)colour.g * (1.0f / 255.0f);
                        fxMults.m_Color.blue  = (float)colour.b * (1.0f / 255.0f);
                        for (int32 i = 0; i < 10; i++) {
                            CVector pos = veh->GetPosition();
                            CVector vel{};
                            pos.x = CGeneral::GetRandomNumberInRange(-3.0f, 3.0f) + pos.x;
                            pos.y = CGeneral::GetRandomNumberInRange(-3.0f, 3.0f) + pos.y;
                            vel.z = CGeneral::GetRandomNumberInRange(0.0f, 0.05f);
                            g_fx.m_SmokeHuge->AddParticle(pos, vel, 0.0f, fxMults, -1.0f, 1.2f, 0.6f, false); // 0x4AA440
                        }
                    }
                }
                veh->vehicleFlags.bDisableParticles = false; // 0x44B2C0
                veh->m_fDirtLevel                   = 0.0f;
            } else {
                bColourChanged = bHadWanted; // 0x44B2DA
            }
        } else {
            bColourChanged = bHadWanted; // 0x44B2DA
        }

        if (g.m_bRespraysAlwaysFree) { // flags & 0x80
            CGarages::TriggerMessage("GA_22", -1, 4000, -1); // 0x859A34
        } else if (bServiced && !CGarages::RespraysAreFree) {
            auto& money = CWorld::Players[CWorld::PlayerInFocus].m_nMoney;
            if (money > 0) {
                money = std::max(money - 100, 0);
            }
            CStats::IncrementStat(STAT_AUTO_REPAIR_AND_PAINTING_BUDGET, 100.0f);
            CGarages::TriggerMessage(bHadWanted ? "GA_2" : "GA_XX", -1, 4000, -1); // 0x859A2C, 0x859A24
        } else if (bColourChanged) {
            CGarages::TriggerMessage((rand() & 1) ? "GA_15" : "GA_16", -1, 4000, -1); // 0x859A1C, 0x859A14
        }

        g.m_bUsedRespray = true; // flags |= 4
        if (FindPlayerVehicle(-1, false)) {
            FindPlayerVehicle(-1, false)->vehicleFlags.bHasBeenResprayed = true; // +0x42F |= 1
        }
        CallOffChaseNearGarage(g);
        return;
    }
    case GARAGE_DOOR_OPEN: { // 0x44AC50
        if (CGarages::NoResprays) {
            return;
        }
        if (g.IsStaticPlayerCarEntirelyInside()) {
            if (CGarages::IsCarSprayable(FindPlayerVehicle(-1, false))) {
                if (CWorld::Players[CWorld::PlayerInFocus].m_nMoney < 100 && !CGarages::RespraysAreFree) {
                    CGarages::TriggerMessage("GA_3", -1, 4000, -1); // 0x859A58
                    g.m_nDoorState = GARAGE_DOOR_WAITING_PLAYER_TO_EXIT;
                    AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_NO_CASH, 0.0f, 1.0f);
                } else { // 0x44ACC0
                    g.m_nDoorState = GARAGE_DOOR_CLOSING;
                    CPad::GetPad(0)->bPlayerAwaitsInGarage = true;
                    FindPlayerVehicle(-1, false)->m_fDirtLevel = 0.0f;
                }
            } else { // 0x44ACE4
                const auto subType = FindPlayerVehicle(-1, false)->m_nVehicleSubType;
                CGarages::TriggerMessage(subType == VEHICLE_TYPE_BMX ? "GA_1B" : "GA_1", -1, 4000, -1); // 0x859A50, 0x859A48
                g.m_nDoorState = GARAGE_DOOR_WAITING_PLAYER_TO_EXIT;
                AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_IS_HOT, 0.0f, 1.0f);
            }
            // 0x44AD2D
            FindPlayerWanted()->m_bPoliceBackOffGarage = true;
            CGarages::LastGaragePlayerWasIn = garageId;
        } else if (!g.IsPlayerOutsideGarage(0.0f)) { // 0x44AD50
            FindPlayerWanted()->m_bPoliceBackOffGarage = true;
            CGarages::LastGaragePlayerWasIn = garageId;
        } else if (garageId == CGarages::LastGaragePlayerWasIn) { // 0x44AD7F
            FindPlayerWanted()->m_bPoliceBackOffGarage = false;
        }

        // 0x44ADA3
        if (!FindPlayerVehicle(-1, false)) {
            return;
        }
        const auto pos = FindPlayerVehicle(-1, false)->GetPosition();
        if (!(g.CalcDistToGarageRectangleSquared(pos.x, pos.y) < 64.0f)) { // 0x859A44
            return;
        }
        CallOffChaseNearGarage(g);
        return;
    }
    case GARAGE_DOOR_CLOSING: { // 0x44AE0C
        if (FindPlayerVehicle(-1, false)) {
            g.ThrowCarsNearDoorOutOfGarage(FindPlayerVehicle(-1, false));
        }
        if (g.SlideDoorClosed()) {
            g.m_nDoorState = GARAGE_DOOR_CLOSED;
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_RESPRAY, 0.0f, 1.0f);
            g.m_nTimeToOpen = CTimer::GetTimeInMS() + 2000;
            CStats::IncrementStat(STAT_TOTAL_LEGITIMATE_KILLS, CStats::GetStatValue(STAT_KILLS_SINCE_LAST_CHECKPOINT)); // 0xB1, 0xB0 (sic)
            CStats::SetStatValue(STAT_KILLS_SINCE_LAST_CHECKPOINT, 0.0f);
        }
        if (FindPlayerVehicle(-1, false)) {
            // BUG: the original assumes an automobile here (offset 0x8E4), see above
            static_cast<CAutomobile*>(FindPlayerVehicle(-1, false))->m_fBurnTimer = 0.0f;
            FindPlayerVehicle(-1, false)->vehicleFlags.bDisableParticles = true; // +0x42E |= 0x80
        }
        CallOffChaseNearGarage(g);
        return;
    }
    case GARAGE_DOOR_OPENING: // 0x44B79C
        ShopOpeningState(g);
        return;
    case GARAGE_DOOR_WAITING_PLAYER_TO_EXIT: // 0x44B40C
        ShopWaitingForPlayerState(g);
        return;
    default:
        return;
    }
}

// 0x44B419
void UpdateBombShop(CGarage& g) {
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_CLOSED: { // 0x44B5CA
        if (!(CTimer::GetTimeInMS() > g.m_nTimeToOpen)) {
            return;
        }
        if (g.m_nType == BOMBSHOP_REMOTE && !CStreaming::IsModelLoaded(MODEL_BOMB)) { // original: dword compare of the load state @ 0x8E6940
            CStreaming::RequestModel(MODEL_BOMB, STREAMING_GAME_REQUIRED); // 0x44B5AE
            return;
        }
        switch (g.m_nType) {
        case BOMBSHOP_TIMED:  AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_FIT_BOMB_TIMED, 0.0f, 1.0f); break;
        case BOMBSHOP_ENGINE: AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_FIT_BOMB_BOOBY_TRAPPED, 0.0f, 1.0f); break;
        case BOMBSHOP_REMOTE: AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_FIT_BOMB_REMOTE_CONTROLLED, 0.0f, 1.0f); break;
        default: break;
        }
        g.m_nDoorState = GARAGE_DOOR_OPENING;
        if (!CGarages::BombsAreFree) {
            auto& money = CWorld::Players[CWorld::PlayerInFocus].m_nMoney;
            if (money > 0) {
                money = std::max(money - 500, 0);
            }
        }
        if (FindPlayerVehicle(-1, false)) {
            const auto vehType = FindPlayerVehicle(-1, false)->m_nVehicleType;
            if (vehType == VEHICLE_TYPE_AUTOMOBILE || FindPlayerVehicle(-1, false)->m_nVehicleType == VEHICLE_TYPE_BIKE) {
                FindPlayerVehicle(-1, false)->m_nBombOnBoard = g.m_nType - 1;
                auto* const player = FindPlayerPed(-1);
                FindPlayerVehicle(-1, false)->m_pWhoInstalledBombOnMe = player; // NOTE: no reference registered, same as the original
                if (g.m_nType == BOMBSHOP_REMOTE) {
                    CGarages::GivePlayerDetonator(); // 0x448660
                }
                CStats::IncrementStat(STAT_KGS_OF_EXPLOSIVES_USED, 10.0f);
            }
        }

        // 0x44B6E8: help message, depends on the control scheme
        const char* key{};
        switch (g.m_nType) {
        case BOMBSHOP_TIMED:
        case BOMBSHOP_ENGINE: {
            const int32 mode = static_cast<uint16>(CPad::GetPad(0)->Mode); // MOVZX; the original also tests `< 0` (never true)
            if (mode <= 2) {
                key = g.m_nType == BOMBSHOP_TIMED ? "GA_6" : "GA_7"; // 0x8599DC, 0x8599EC
            } else if (mode == 3) {
                key = g.m_nType == BOMBSHOP_TIMED ? "GA_6B" : "GA_7B"; // 0x8599E4, 0x8599F4
            } else {
                return;
            }
            break;
        }
        case BOMBSHOP_REMOTE:
            key = "GA_8"; // 0x8599FC
            break;
        default:
            return;
        }
        CHud::SetHelpMessage(TheText.Get(key), false, false, true);
        return;
    }
    case GARAGE_DOOR_OPEN: { // 0x44B42D
        if (!g.IsStaticPlayerCarEntirelyInside()) {
            return;
        }
        if (!FindPlayerVehicle(-1, false)) {
            return;
        }
        if (FindPlayerVehicle(-1, false)->m_nVehicleSubType == VEHICLE_TYPE_BIKE) {
            return;
        }
        if (FindPlayerVehicle(-1, false)->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
            return;
        }
        if (FindPlayerVehicle(-1, false)->m_nBombOnBoard != 0) { // 0x44B52B
            CGarages::TriggerMessage("GA_5", -1, 4000, -1); // 0x859A04
            g.m_nDoorState = GARAGE_DOOR_WAITING_PLAYER_TO_EXIT;
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_ALREADY_RIGGED, 0.0f, 1.0f);
            return;
        }
        if (!CGarages::BombsAreFree && CWorld::Players[CWorld::PlayerInFocus].m_nMoney < 500) {
            CGarages::TriggerMessage("GA_4", -1, 4000, -1); // 0x859A0C
            g.m_nDoorState = GARAGE_DOOR_WAITING_PLAYER_TO_EXIT;
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_NO_CASH, 0.0f, 1.0f);
            return;
        }
        g.m_nDoorState = GARAGE_DOOR_CLOSING; // 0x44B4F8
        HoldPlayerInGarage();
        return;
    }
    case GARAGE_DOOR_CLOSING: { // 0x44B564
        if (FindPlayerVehicle(-1, false)) {
            g.ThrowCarsNearDoorOutOfGarage(FindPlayerVehicle(-1, false));
        }
        if (g.SlideDoorClosed()) {
            g.m_nDoorState  = GARAGE_DOOR_CLOSED;
            g.m_nTimeToOpen = CTimer::GetTimeInMS() + 2000;
        }
        if (g.m_nType == BOMBSHOP_REMOTE) {
            CStreaming::RequestModel(MODEL_BOMB, STREAMING_GAME_REQUIRED); // 0x44B5AE
        }
        return;
    }
    case GARAGE_DOOR_OPENING: // 0x44B79C
        ShopOpeningState(g);
        return;
    case GARAGE_DOOR_WAITING_PLAYER_TO_EXIT: // 0x44B40C
        ShopWaitingForPlayerState(g);
        return;
    default:
        return;
    }
}

// 0x44B7EE
void UpdateOnlyTargetVeh(CGarage& g) {
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_CLOSED: { // 0x44B9AB
        auto* const target = g.m_pTargetCar;
        if (FindPlayerVehicle(-1, false) != target) {
            return;
        }
        if (!target) {
            return;
        }
        OpenIfPlayerVehicleNear(g, 64.0f); // 0x859A44
        return;
    }
    case GARAGE_DOOR_OPEN: { // 0x44B802
        if (PlayerDistSqFromGarageCenter(g) > 900.0f) { // 900.0f @ 0x858978
            // Far away: close the door (once the target car is out of the way)
            if ((CTimer::GetFrameCounter() & 0x1F) != 0) {
                return;
            }
            if (g.m_pTargetCar && g.IsEntityTouching3D(g.m_pTargetCar)) {
                return;
            }
            g.m_b0x1       = true; // 0x44C662
            g.m_nDoorState = GARAGE_DOOR_CLOSING;
            return;
        }
        // 0x44B8BE
        auto* const target = g.m_pTargetCar;
        if (FindPlayerVehicle(-1, false) == target) {
            return;
        }
        if (!target) {
            return;
        }
        if (!g.IsEntityEntirelyInside3D(target, 0.0f)) {
            return;
        }
        CEntity* const playerEntity = FindPlayerVehicle(-1, false) ? (CEntity*)FindPlayerVehicle(-1, false) : (CEntity*)FindPlayerPed(-1);
        if (!g.IsEntityEntirelyOutside(playerEntity, 2.0f)) {
            return;
        }
        HoldPlayerInGarage();
        g.m_b0x1       = false;
        g.m_nDoorState = GARAGE_DOOR_CLOSING;
        return;
    }
    case GARAGE_DOOR_CLOSING: { // 0x44B963
        if (g.m_pTargetCar) {
            g.ThrowCarsNearDoorOutOfGarage(g.m_pTargetCar);
        }
        if (!g.SlideDoorClosed()) {
            return;
        }
        if (g.m_b0x1) { // 0x44C6AC
            g.m_nDoorState = GARAGE_DOOR_CLOSED;
            return;
        }
        if (g.m_pTargetCar) {
            g.m_nDoorState = GARAGE_DOOR_CLOSED_DROPPED_CAR;
            g.m_pTargetCar->DestroyVehicleAndDriverAndPassengers(g.m_pTargetCar); // NOTE: declared as a member of CVehicle, the original is a cdecl function (0x6D2250)
            g.m_pTargetCar = nullptr;
        } else {
            g.m_nDoorState = GARAGE_DOOR_CLOSED; // 0x44BBC8
        }
        ReleasePlayerFromGarage(); // 0x44BBCC
        return;
    }
    case GARAGE_DOOR_OPENING: // 0x44BCBE
        OpenDoorStep(g);
        return;
    default:
        return;
    }
}

// 0x44B9FF
void UpdateClosesOnTouch(CGarage& g) {
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_OPEN: // 0x44BA3C
        if (g.IsGarageEmpty()) {
            g.m_nDoorState = GARAGE_DOOR_CLOSING;
        }
        return;
    case GARAGE_DOOR_CLOSING: // 0x44BA0D
        if (g.SlideDoorClosed()) {
            g.m_nDoorState = GARAGE_DOOR_CLOSED;
        }
        if (!g.IsGarageEmpty()) {
            g.m_nDoorState = GARAGE_DOOR_OPENING;
        }
        return;
    case GARAGE_DOOR_OPENING: // 0x44BCB7
        OpenDoorStep(g);
        return;
    default:
        return;
    }
}

// 0x44BA5C
void UpdateOpenForTargetFreezePlayer(CGarage& g) { // also CLOSE_WITH_CAR_DONT_OPEN_AGAIN
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_CLOSED: { // 0x44BBFB
        auto* const target = g.m_pTargetCar;
        if (FindPlayerVehicle(-1, false) != target) {
            return;
        }
        if (!target) {
            return;
        }
        OpenIfPlayerVehicleNear(g, 289.0f); // 0x8599D8
        return;
    }
    case GARAGE_DOOR_OPEN: { // 0x44BA70
        if (PlayerDistSqFromGarageCenter(g) > 900.0f || !g.m_pTargetCar) { // 0x44C662
            g.m_b0x1       = true;
            g.m_nDoorState = GARAGE_DOOR_CLOSING;
            return;
        }
        if (g.m_pTargetCar != FindPlayerVehicle(-1, false)) {
            return;
        }
        if (!g.IsStaticPlayerCarEntirelyInside()) {
            return;
        }
        if (g.IsAnyCarBlockingDoor()) {
            return;
        }
        HoldPlayerInGarage();
        g.m_b0x1       = false;
        g.m_nDoorState = GARAGE_DOOR_CLOSING;
        return;
    }
    case GARAGE_DOOR_CLOSING: { // 0x44BB83
        if (g.m_pTargetCar) {
            g.ThrowCarsNearDoorOutOfGarage(g.m_pTargetCar);
        }
        if (!g.SlideDoorClosed()) {
            return;
        }
        if (g.m_b0x1) { // 0x44C6AC
            g.m_nDoorState = GARAGE_DOOR_CLOSED;
            return;
        }
        if (g.m_pTargetCar) {
            g.m_nDoorState  = GARAGE_DOOR_CLOSED_DROPPED_CAR;
            g.m_nTimeToOpen = CTimer::GetTimeInMS() + 2000;
            g.m_pTargetCar  = nullptr;
        } else {
            g.m_nDoorState = GARAGE_DOOR_CLOSED; // 0x44BBC8
        }
        ReleasePlayerFromGarage(); // 0x44BBCC
        return;
    }
    case GARAGE_DOOR_OPENING: // 0x44BCBE
        OpenDoorStep(g);
        return;
    case GARAGE_DOOR_CLOSED_DROPPED_CAR: // 0x44BC7B
        if (g.m_nType != OPEN_FOR_TARGET_FREEZE_PLAYER) {
            return;
        }
        if (CTimer::GetTimeInMS() > g.m_nTimeToOpen) {
            g.m_nDoorState = GARAGE_DOOR_OPENING;
        }
        return;
    default:
        return;
    }
}

// 0x44BCA4 (SCRIPT_ONLY_OPEN)
void UpdateScriptOnlyOpen(CGarage& g) {
    if (g.m_nDoorState == GARAGE_DOOR_OPENING) {
        OpenDoorStep(g);
    }
}

// 0x44BCAA (SCRIPT_CONTROLLED)
void UpdateScriptControlled(CGarage& g) {
    if (g.m_nDoorState == GARAGE_DOOR_CLOSING) {
        CloseDoorStep(g); // 0x44C69D
    } else if (g.m_nDoorState == GARAGE_DOOR_OPENING) {
        OpenDoorStep(g);
    }
}

// 0x44BCDE (safehouses and hangars)
void UpdateHideOut(CGarage& g) {
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_CLOSED: { // 0x44BE56
        if (!(FindPlayerCoors().z < 950.0f)) { // 0x858F4C
            return;
        }
        const auto pc = FindPlayerCoors();
        const float d = g.CalcDistToGarageRectangleSquared(pc.x, pc.y);
        if (!(d < 12.25f)) { // 0x8599C8
            if (!(d < 100.0f)) { // 0x858628
                return;
            }
            if (!FindPlayerVehicle(-1, false)) {
                return;
            }
            if (FindPlayerVehicle(-1, false)->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
                return;
            }
        }

        // 0x44BEF9
        if (FindPlayerVehicle(-1, false) && g.m_nType != HANGAR_AT400) {
            const int32 required = g.m_nType != SAFEHOUSE_GANTON ? 4 : 2;
            if (CGarages::CountCarsInHideoutGarage(g.m_nType) >= required) {
                // The player's vehicle is close to a door => show the help message
                CObject *first, *second;
                g.FindDoorsWithGarage(&first, &second);
                const auto DoorDistSq = [](const CObject* door) {
                    const auto& dp = door->GetPosition();
                    const auto& vp = FindPlayerVehicle(-1, false)->GetPosition();
                    const double dy = (double)dp.y - vp.y;
                    const double dx = (double)dp.x - vp.x;
                    return dy * dy + dx * dx;
                };
                if (!(first && DoorDistSq(first) < 25.0f)) { // 0x858FE8
                    if (!second) {
                        return;
                    }
                    if (!(DoorDistSq(second) < 25.0f)) {
                        return;
                    }
                }
                // 0x44C13E
                if (!(CTimer::GetTimeInMS() - CGarages::LastTimeHelpMessage > 18000)) { // unsigned compare, 0x4650
                    return;
                }
                if (FindPlayerVehicle(-1, false)->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI) {
                    return;
                }
                if (FindPlayerVehicle(-1, false)->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE) {
                    return;
                }
                CHud::SetHelpMessage(TheText.Get("GA_21"), false, false, true); // 0x8599C0
                CGarages::LastTimeHelpMessage = CTimer::GetTimeInMS();
                return;
            }
        }

        // 0x44C1C3
        if (g.m_nType != HANGAR_AT400) {
            if (!g.RestoreCarsForThisHideOut(CGarages::GetStoredCarsInSafehouse(CGarages::FindSafeHouseIndexForGarageType(g.m_nType)))) {
                return;
            }
        }
        g.m_nDoorState = GARAGE_DOOR_OPENING; // 0x44C1EE
        return;
    }
    case GARAGE_DOOR_OPEN: { // 0x44BCF2
        const auto  pc = FindPlayerCoors();
        const float d  = g.CalcDistToGarageRectangleSquared(pc.x, pc.y);

        bool bTryClose; // goto 0x44BD6A (true) or 0x44BD79 (false)
        if (d > 225.0f) { // 0x8599D4
            bTryClose = true;
        } else if (!(d > 16.0f)) { // 0x8599D0; (<= or unordered)
            bTryClose = false;
        } else if (!FindPlayerVehicle(-1, false)) {
            bTryClose = true;
        } else {
            bTryClose = FindPlayerVehicle(-1, false)->m_nVehicleSubType == VEHICLE_TYPE_BMX;
        }
        if (bTryClose && !g.IsAnyCarBlockingDoor()) { // 0x44BD6A
            g.m_nDoorState = GARAGE_DOOR_CLOSING; // 0x44C7C0
            return;
        }

        // 0x44BD79
        if (FindPlayerVehicle(-1, false)) {
            const int32 required = g.m_nType != SAFEHOUSE_GANTON ? 4 : 2;
            if (g.CountCarsWithCenterPointWithinGarage(FindPlayerVehicle(-1, false)) >= required && g.IsPlayerOutsideGarage(0.25f)) {
                g.m_nDoorState = GARAGE_DOOR_CLOSING; // 0x44C7C0
                return;
            }
        }
        if (!(d > 4900.0f)) { // 0x8599CC
            return;
        }
        g.m_nDoorState = GARAGE_DOOR_CLOSING;
        g.RemoveCarsBlockingDoorNotInside();
        return;
    }
    case GARAGE_DOOR_CLOSING: { // 0x44BDF1
        g.SlideDoorClosed(); // NOTE: result unused
        if (!g.IsPlayerOutsideGarage(0.0f)) {
            g.m_nDoorState = GARAGE_DOOR_OPENING; // 0x44C6F9
            return;
        }
        if (g.m_fDoorPosition != 0.0f) { // FCOMP + `test ah, 0x44` / JP: continue only when equal (NaN returns)
            return;
        }
        g.m_nDoorState = GARAGE_DOOR_CLOSED;
        if (g.m_nType == HANGAR_AT400) {
            return;
        }
        g.StoreAndRemoveCarsForThisHideOut(CGarages::GetStoredCarsInSafehouse(CGarages::FindSafeHouseIndexForGarageType(g.m_nType)), 4);
        return;
    }
    case GARAGE_DOOR_OPENING: // 0x44BCBE
        OpenDoorStep(g);
        return;
    default:
        return;
    }
}

// 0x44C1FF (STAY_OPEN_WITH_CAR_INSIDE)
void UpdateStayOpenWithCarInside(CGarage& g) {
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_CLOSED: { // 0x44B9AB
        auto* const target = g.m_pTargetCar;
        if (FindPlayerVehicle(-1, false) != target) {
            return;
        }
        if (!target) {
            return;
        }
        OpenIfPlayerVehicleNear(g, 64.0f); // 0x859A44
        return;
    }
    case GARAGE_DOOR_OPEN: // 0x44C213
        if (!(PlayerDistSqFromGarageCenter(g) > 900.0f)) { // 0x858978
            return;
        }
        if (!g.m_pTargetCar) {
            return;
        }
        if (!g.IsEntityEntirelyOutside(g.m_pTargetCar, 0.0f)) {
            return;
        }
        g.m_b0x1       = true;
        g.m_nDoorState = GARAGE_DOOR_CLOSING;
        return;
    case GARAGE_DOOR_CLOSING: // 0x44C2E3
        ThrowOutTargetCarAndCloseDoorStep(g);
        return;
    case GARAGE_DOOR_OPENING: // 0x44BCBE
        OpenDoorStep(g);
        return;
    default:
        return;
    }
}

// 0x44C2F3 (SCRIPT_OPEN_FREEZE_WHEN_CLOSING)
void UpdateScriptOpenFreezeWhenClosing(CGarage& g) {
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_OPEN: { // 0x44C331
        auto* const target = g.m_pTargetCar;
        if (!target) {
            return;
        }
        if (!g.IsEntityEntirelyInside3D(target, 0.0f)) {
            return;
        }
        if (g.IsAnyCarBlockingDoor()) {
            return;
        }
        if (!g.IsPlayerOutsideGarage(0.0f)) {
            return;
        }
        CPad::GetPad(0)->bPlayerAwaitsInGarage = true;
        g.m_b0x1       = false;
        g.m_nDoorState = GARAGE_DOOR_CLOSING;
        return;
    }
    case GARAGE_DOOR_CLOSING: // 0x44C301
        if (!g.SlideDoorClosed()) {
            return;
        }
        g.m_nDoorState = GARAGE_DOOR_CLOSED;
        CPad::GetPad(0)->bPlayerAwaitsInGarage = false;
        return;
    case GARAGE_DOOR_OPENING: // 0x44BCB7
        OpenDoorStep(g);
        return;
    default:
        return;
    }
}

// 0x44C395 (IMPOUND_LS/SF/LV)
void UpdateImpound(CGarage& g) {
    // The player is in the garage's height range
    const auto IsPlayerInHeightRange = [&g] {
        bool bRes = false;
        if ((double)g.m_fTopZ - 2.0 > FindPlayerCoors().z) { // 2.0f @ 0x858CA0
            if (FindPlayerCoors().z > g.m_vPosn.z) {
                bRes = true;
            }
        }
        return bRes;
    };
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_CLOSED: { // 0x44C477
        const auto  pc = FindPlayerCoors();
        const float d  = g.CalcDistToGarageRectangleSquared(pc.x, pc.y);
        const bool  bInRange = IsPlayerInHeightRange();
        if (!(d < 3600.0f)) { // 0x8599B8
            return;
        }
        if (!bInRange) {
            return;
        }
        g.NeatlyLineUpStoredCars(CGarages::GetStoredCarsInSafehouse(CGarages::FindSafeHouseIndexForGarageType(g.m_nType)));
        if (!g.RestoreCarsForThisImpoundingGarage(CGarages::GetStoredCarsInSafehouse(CGarages::FindSafeHouseIndexForGarageType(g.m_nType)))) {
            return;
        }
        g.m_nDoorState = GARAGE_DOOR_OPEN; // 0x44C554
        return;
    }
    case GARAGE_DOOR_OPEN:
    case GARAGE_DOOR_CLOSING: { // 0x44C3AE
        const auto  pc = FindPlayerCoors();
        const float d  = g.CalcDistToGarageRectangleSquared(pc.x, pc.y);
        const bool  bInRange = IsPlayerInHeightRange();
        if (!(d > 4225.0f) && bInRange && g.m_nDoorState != GARAGE_DOOR_CLOSING) { // 0x8599BC; 0x44C43D
            return;
        }
        g.m_nDoorState = GARAGE_DOOR_CLOSED; // 0x44C447
        g.StoreAndRemoveCarsForThisImpoundingGarage(CGarages::GetStoredCarsInSafehouse(CGarages::FindSafeHouseIndexForGarageType(g.m_nType)), 3);
        return;
    }
    default:
        return;
    }
}

// 0x44C564 (TUNING_*)
void UpdateTuning(CGarage& g) {
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_CLOSED: // 0x44C6BD
        if (!g.RightModTypeForThisGarage(FindPlayerVehicle(-1, false))) {
            return;
        }
        OpenIfPlayerVehicleNear(g, 64.0f); // 0x859A44
        return;
    case GARAGE_DOOR_OPEN: // 0x44C578
        if (!(PlayerDistSqFromGarageCenter(g) > 900.0f)) { // 0x858978
            return;
        }
        if ((CTimer::GetFrameCounter() & 0x1F) != 0) {
            return;
        }
        if (g.RightModTypeForThisGarage(FindPlayerVehicle(-1, false))) {
            if (g.IsEntityTouching3D(FindPlayerVehicle(-1, false))) {
                return;
            }
        }
        if (g.IsAnyOtherCarTouchingGarage(nullptr)) { // 0x44C652
            return;
        }
        g.m_b0x1       = true; // 0x44C662
        g.m_nDoorState = GARAGE_DOOR_CLOSING;
        return;
    case GARAGE_DOOR_CLOSING: // 0x44C67B
        ThrowOutPlayerVehicleAndCloseDoorStep(g);
        return;
    case GARAGE_DOOR_OPENING: // 0x44BCBE
        OpenDoorStep(g);
        return;
    default:
        return;
    }
}

// 0x44C70A (BURGLARY)
void UpdateBurglary(CGarage& g) {
    switch (g.m_nDoorState) {
    case GARAGE_DOOR_OPEN: // 0x44C71D
        if (!(PlayerDistSqFromGarageCenter(g) > 900.0f)) { // 0x858978
            return;
        }
        if (g.IsAnyOtherCarTouchingGarage(nullptr)) {
            return;
        }
        g.m_nDoorState = GARAGE_DOOR_CLOSING; // 0x44C7C0
        return;
    case GARAGE_DOOR_CLOSING: // 0x44C67B
        ThrowOutPlayerVehicleAndCloseDoorStep(g);
        return;
    case GARAGE_DOOR_OPENING: // 0x44BCB7
        OpenDoorStep(g);
        return;
    default:
        return;
    }
}
} // namespace

// 0x44AA50
void CGarage::Update(int32 garageId) {
    // 0x44AA50: tell the camera / the garage system whether the player (or his vehicle) is in this garage
    if (static_cast<uint8>(m_nType) != 13 && m_nDoorState <= GARAGE_DOOR_CLOSED_DROPPED_CAR && FindPlayerPed(-1) && !m_bCameraFollowsPlayer) {
        auto* const playerVeh = FindPlayerVehicle(-1, false);
        CEntity*    entity    = FindPlayerPed(-1);
        {
            auto* const ped = FindPlayerPed(-1);
            if (ped->bInVehicle && ped->m_pVehicle && ped->m_pVehicle->m_nModelIndex == MODEL_KART) { // 0x23B
                entity = FindPlayerPed(-1)->m_pVehicle;
            }
        }
        if (IsEntityEntirelyInside3D(entity, 0.25f)) { // 0x3E800000
            CGarages::bCamShouldBeOutside = true;
            TheCamera.m_pToGarageWeAreIn  = this;
        }
        if (playerVeh) {
            if (!IsEntityEntirelyOutside(playerVeh, 0.0f)) {
                TheCamera.m_pToGarageWeAreInForHackAvoidFirstPerson = this;
            }
            if (playerVeh->m_nModelIndex == MODEL_MRWHOOP) { // 0x1A7
                const auto& pos = playerVeh->GetPosition();
                if ((double)m_fLeftCoord - 0.5 < pos.x && pos.x < (double)m_fRightCoord + 0.5 && (double)m_fFrontCoord - 0.5 < pos.y && pos.y < (double)m_fBackCoord + 0.5) { // 0.5f @ 0x858B8C
                    CGarages::bCamShouldBeOutside = true;
                    TheCamera.m_pToGarageWeAreIn  = this;
                }
            }
        }
    }

    // 0x44ABB6
    if (m_bInactive && m_nDoorState == GARAGE_DOOR_CLOSED) {
        return;
    }
    if (m_bDoorOpensUp) {
        // 0.4f @ 0x858EE8
        if ((m_nDoorState == GARAGE_DOOR_OPENING && m_fDoorPosition > 0.4f) || m_nDoorState == GARAGE_DOOR_OPEN) {
            m_bDoorClosed = false;
        } else {
            m_bDoorClosed = true;
        }
    }

    switch (m_nType) {
    case ONLY_TARGET_VEH:
        UpdateOnlyTargetVeh(*this);
        break;
    case BOMBSHOP_TIMED:
    case BOMBSHOP_ENGINE:
    case BOMBSHOP_REMOTE:
        UpdateBombShop(*this);
        break;
    case PAYNSPRAY:
        UpdatePaynSpray(*this, garageId);
        break;
    case UNKN_CLOSESONTOUCH:
        UpdateClosesOnTouch(*this);
        break;
    case OPEN_FOR_TARGET_FREEZE_PLAYER:
    case CLOSE_WITH_CAR_DONT_OPEN_AGAIN:
        UpdateOpenForTargetFreezePlayer(*this);
        break;
    case SCRIPT_ONLY_OPEN:
        UpdateScriptOnlyOpen(*this);
        break;
    case SAFEHOUSE_GANTON:
    case SAFEHOUSE_SANTAMARIA:
    case SAGEHOUSE_ROCKSHORE:
    case SAFEHOUSE_FORTCARSON:
    case SAFEHOUSE_VERDANTMEADOWS:
    case SAFEHOUSE_DILLIMORE:
    case SAFEHOUSE_PRICKLEPINE:
    case SAFEHOUSE_WHITEWOOD:
    case SAFEHOUSE_PALOMINOCREEK:
    case SAFEHOUSE_REDSANDSWEST:
    case SAFEHOUSE_ELCORONA:
    case SAFEHOUSE_MULHOLLAND:
    case SAFEHOUSE_CALTONHEIGHTS:
    case SAFEHOUSE_PARADISO:
    case SAFEHOUSE_DOHERTY:
    case SAFEHOUSE_HASHBURY:
    case HANGAR_AT400:
    case HANGAR_ABANDONED_AIRPORT:
        UpdateHideOut(*this);
        break;
    case SCRIPT_CONTROLLED:
        UpdateScriptControlled(*this);
        break;
    case STAY_OPEN_WITH_CAR_INSIDE:
        UpdateStayOpenWithCarInside(*this);
        break;
    case SCRIPT_OPEN_FREEZE_WHEN_CLOSING:
        UpdateScriptOpenFreezeWhenClosing(*this);
        break;
    case IMPOUND_LS:
    case IMPOUND_SF:
    case IMPOUND_LV:
        UpdateImpound(*this);
        break;
    case TUNING_LOCO_LOW_CO:
    case TUNING_WHEEL_ARCH_ANGELS:
    case TUNING_TRANSFENDER:
        UpdateTuning(*this);
        break;
    case BURGLARY:
        UpdateBurglary(*this);
        break;
    default: // 6-10, 12, 13, 22 (and > 45) do nothing
        break;
    }
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
