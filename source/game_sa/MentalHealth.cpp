#include "StdInc.h"
#include "MentalHealth.h"

#include "Fx/FxFtol.h"
#include "Events/EventHealthLow.h"
#include "Events/EventHealthReallyLow.h"
#include "Events/EventLowAngerAtPlayer.h"
#include "Events/EventHighAngerAtPlayer.h"

void CMentalState::InjectHooks() {
    RH_ScopedClass(CMentalState);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Process, 0x6008A0);
}

// NOTE: 0x421050 is `CPedIntelligence::IncrementAngerAtPlayer` (it has a `CMentalState` at +0x174), not a `CMentalState` method.
// The unused `CMentalState::IncrementAnger` stub (which called it with the wrong `this`) was removed.

// 0x6008A0
void CMentalState::Process(CPed& ped) {
    m_pedHealth = (uint8)notsa::detail::Ftol(ped.m_fHealth); // _ftol

    if (ped.IsInVehicle()) {
        m_vehicleHealth = (uint8)notsa::detail::Ftol(ped.m_pVehicle->m_fHealth);
    }

    const auto AddEvent = [&](const CEvent& event) {
        ped.GetIntelligence()->m_eventGroup.Add(const_cast<CEvent*>(&event), false); // 0x4AB420
    };

    if (!ped.bInVehicle) {
        if (m_oldPedHealth >= 50) {
            if (m_pedHealth < 10) {
                AddEvent(CEventHealthReallyLow{});
            } else if (m_pedHealth < 50) {
                AddEvent(CEventHealthLow{});
            }
        } else if (m_oldPedHealth >= 10 && m_pedHealth < 10) {
            AddEvent(CEventHealthReallyLow{});
        }
    } else {
        // BUG: `m_oldVehicleHealth` and `m_vehicleHealth` are bytes, so they can never be >= 300 / 600, hence no event is ever added here.
        if (m_oldVehicleHealth >= 600) {
            if (m_vehicleHealth < 300) {
                AddEvent(CEventHealthReallyLow{});
            } else if (m_vehicleHealth < 600) {
                AddEvent(CEventHealthLow{});
            }
        } else if (m_oldVehicleHealth >= 300 && m_vehicleHealth < 300) {
            AddEvent(CEventHealthReallyLow{});
        }
    }

    if (m_LastAngerAtPlayer <= 3) {
        if (m_AngerAtPlayer > 6) {
            AddEvent(CEventHighAngerAtPlayer{});
        } else if (m_AngerAtPlayer > 3) {
            AddEvent(CEventLowAngerAtPlayer{});
        }
    } else if (m_LastAngerAtPlayer <= 6 && m_AngerAtPlayer > 6) {
        AddEvent(CEventHighAngerAtPlayer{});
    }

    m_LastAngerAtPlayer = m_AngerAtPlayer;
    m_oldPedHealth      = m_pedHealth;
    m_oldVehicleHealth  = m_vehicleHealth;
}
