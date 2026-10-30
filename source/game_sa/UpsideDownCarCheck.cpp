#include "StdInc.h"

#include "UpsideDownCarCheck.h"

void CUpsideDownCarCheck::InjectHooks() {
    RH_ScopedClass(CUpsideDownCarCheck);
    RH_ScopedCategoryGlobal();

    // RH_ScopedOverloadedInstall(IsCarUpsideDown, "handle", 0x0);
    RH_ScopedOverloadedInstall(IsCarUpsideDown, "vehicle", 0x463830, bool(CUpsideDownCarCheck::*)(const CVehicle*));
    RH_ScopedInstall(UpdateTimers, 0x4655E0);
    RH_ScopedInstall(AddCarToCheck, 0x4638D0);
    RH_ScopedInstall(RemoveCarFromCheck, 0x463910);
    RH_ScopedInstall(HasCarBeenUpsideDownForAWhile, 0x463940);
}

// completely inlined, no addr, see 0x468DCB
void CUpsideDownCarCheck::Init() {
    std::ranges::fill(m_aUpsideDownCars, UpsideDownCar());
}

bool CUpsideDownCarCheck::IsCarUpsideDown(int32 carHandle) {
    const auto vehicle = CPools::GetVehicle(carHandle);
    return IsCarUpsideDown(vehicle);
}

// 0x463830
bool CUpsideDownCarCheck::IsCarUpsideDown(const CVehicle* vehicle) {
    const auto GetNumContactWheels = [=]() -> uint32 {
        switch (vehicle->m_nVehicleType) {
        case eVehicleType::VEHICLE_TYPE_BIKE:       return static_cast<const CBike*>(vehicle)->m_nNoOfContactWheels;
        case eVehicleType::VEHICLE_TYPE_AUTOMOBILE: return static_cast<const CAutomobile*>(vehicle)->m_nNumContactWheels;
        default: return 0u;
        }
    };

    // exe 0x463830-0x463895: the shape test first, then the vcall (slot 0xE4 = CanPedStepOutCar(false)) must be TRUE (the port had it inverted: `!CanPedStepOutCar`)
    const auto& up = vehicle->GetUp();
    const bool  isUpsideDown = up.z < 0.3f && GetNumContactWheels() < 4  // Not totally up-side down
                            || up.z < 0.f;                               // Literally up-side down
    return isUpsideDown && vehicle->CanPedStepOutCar(false);
}

// Process
// 0x4655E0
void CUpsideDownCarCheck::UpdateTimers() {
    ZoneScoped;

    for (auto& car : m_aUpsideDownCars) {
        // exe 0x465610: no test of the handle for 0, every slot is looked up (a free slot has handle -1)
        if (const auto vehicle = CPools::GetVehicle(car.m_nHandle)) {
            if (IsCarUpsideDown(vehicle)) {
                car.m_nTime += NOTSA_FTOL(uint32, CTimer::GetTimeStepInMS());
            } else {
                car.m_nTime = 0;
            }
        } else {
            car.m_nHandle = -1; // exe 0x465662: the slot is freed with -1 (the port stored 0, which AddCarToCheck never treats as free)
            car.m_nTime   = 0;  // 0x465669
        }
    }
}

// 0x4638A0
// never used
bool CUpsideDownCarCheck::AreAnyCarsUpsideDown() {
    for (auto& car : m_aUpsideDownCars) {
        if (car.m_nHandle >= 0 && car.m_nTime >= UPSIDE_DOWN_CAR_MIN_TIME) {
            return true;
        }
    }
    return false;
}

// 0x4638D0
void CUpsideDownCarCheck::AddCarToCheck(int32 carHandle) {
    for (auto& car : m_aUpsideDownCars) {
        if (car.m_nHandle < 0) {
            car.m_nHandle = carHandle;
            car.m_nTime = 0;
            return; // exe 0x4638E5: only the first free slot is taken (the port filled all of them)
        }
    }
}

// 0x463910
void CUpsideDownCarCheck::RemoveCarFromCheck(int32 carHandle) {
    for (auto& car : m_aUpsideDownCars) {
        if (car.m_nHandle == carHandle) {
            car.Clear();
        }
    }
}

// 0x463940
bool CUpsideDownCarCheck::HasCarBeenUpsideDownForAWhile(int32 carHandle) {
    for (auto& car : m_aUpsideDownCars) {
        if (car.m_nHandle == carHandle) {
            return car.m_nTime > UPSIDE_DOWN_CAR_MIN_TIME;
        }
    }
    return false;
}
