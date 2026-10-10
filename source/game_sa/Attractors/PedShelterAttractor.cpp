#include "StdInc.h"

#include <numbers>

#include "PedShelterAttractor.h"
#include "Tasks/TaskTypes/TaskComplexGoToAttractor.h"

// The one in `common.h` is rounded to 5 decimals, the original uses the exact float (0x858CBC)
static constexpr float EXACT_TWO_PI = std::numbers::pi_v<float> * 2.f;

// NOTE: `SArray::erase` is wrong (it passes the *begin* of the destination to `rng::move_backward`, which expects its *end*), so shift the tail down manually (as the original does)
template<typename T>
static void EraseFromSArray(SArray<T>& arr, T* pos) {
    rng::move(pos + 1, arr._Last, pos);
    std::destroy_at(--arr._Last);
}

// 0x5EF420
// NOTE: The original returns a pointer to the element (not a copy)
CVector& CPedShelterAttractor::GetDisplacement(int32 pedId) {
    if (ms_displacements.empty()) {
        ms_displacements.reserve(5); // NOTE: `SArray::emplace_back` can't grow an array which has no storage yet
        // Generate 5 random displacements (inside a circle with radius 2), each at least 1 unit away from all others
        for (int32 n = 0; n < 5;) {
            const float angle  = (float)((double)CGeneral::GetRandomNumber() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)EXACT_TWO_PI);
            const double radius = ((double)CGeneral::GetRandomNumber() * (double)RAND_MAX_FLOAT_RECIPROCAL) * 2.0;
            const CVector disp{
                (float)(x87::cos((double)angle) * radius),
                (float)(x87::sin((double)angle) * radius),
                0.f
            };

            bool tooClose = false;
            for (const CVector& other : ms_displacements) {
                const float dx = other.x - disp.x;
                const float dy = other.y - disp.y;
                const float dz = other.z; // NOTE: Not subtracting `disp.z` (always 0)
                if (dx * dx + dy * dy + dz * dz < 1.f) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) {
                continue;
            }

            ms_displacements.emplace_back(disp);
            n++;
        }
    }
    return ms_displacements.begin()[pedId]; // NOTE: Original doesn't bounds check
}

// 0x5EFC40
void CPedShelterAttractor::ComputeAttractPos(int32 pedId, CVector& outPos) {
    if (m_Fx) {
        const CVector displacement = GetDisplacement(pedId);
        outPos = displacement + m_Pos;
    }
}

// 0x5E9690
void CPedShelterAttractor::ComputeAttractHeading(int32 bQueue, float& heading) {
    heading = CGeneral::GetRandomNumberInRange(0.0f, TWO_PI);
}

// 0x5EF570
bool CPedShelterAttractor::BroadcastDeparture(CPed* ped) {
    const auto it = rng::find(m_ArrivedPeds, ped);
    if (it == m_ArrivedPeds.end()) {
        return false;
    }

    if (const auto pair = rng::find(m_PedTaskPairs, ped, &CPedTaskPair::Ped); pair != m_PedTaskPairs.end()) {
        EraseFromSArray(m_PedTaskPairs, &*pair);
    }
    EraseFromSArray(m_ArrivedPeds, &*it);

    for (auto* const attractedPed : m_AttractPeds) {
        const auto n = (int32)(m_ArrivedPeds.size());
        CVector pos;
        ComputeAttractPos(n, pos);
        float heading;
        ComputeAttractHeading(n, heading);
        SetTaskForPed(attractedPed, new CTaskComplexGoToAttractor{ this, pos, heading, m_AchieveQueueTime, n, PEDMOVE_WALK });
    }
    return true;
}

void CPedShelterAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedShelterAttractor, 0x86C5B4, 6);
    RH_ScopedCategory("Attractors");

    RH_ScopedInstall(GetDisplacement, 0x5EF420);
    RH_ScopedVMTInstall(ComputeAttractPos, 0x5EFC40);
    RH_ScopedVMTInstall(ComputeAttractHeading, 0x5E9690);
    RH_ScopedVMTInstall(BroadcastDeparture, 0x5EF570);
}
