#include "StdInc.h"

#include "PedShelterAttractor.h"
#include "Tasks/TaskTypes/TaskComplexGoToAttractor.h"

// 0x5EF420
// NOTE: The original returns a pointer to the element (not a copy)
CVector& CPedShelterAttractor::GetDisplacement(int32 pedId) {
    if (ms_displacements.empty()) {
        // Generate 5 random displacements (inside a circle with radius 2), each at least 1 unit away from all others
        for (int32 n = 0; n < 5;) {
            const float angle  = (float)((double)CGeneral::GetRandomNumber() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)TWO_PI);
            const double radius = ((double)CGeneral::GetRandomNumber() * (double)RAND_MAX_FLOAT_RECIPROCAL) * 2.0;
            const CVector disp{
                (float)(std::cos((double)angle) * radius),
                (float)(std::sin((double)angle) * radius),
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
        m_PedTaskPairs.erase(pair);
    }
    m_ArrivedPeds.erase(it);

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
