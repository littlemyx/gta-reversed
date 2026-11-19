#pragma once

#include "PedAttractor.h"

class NOTSA_EXPORT_VTABLE CPedPizzaAttractor final : public CPedAttractor {
public:
    static void InjectHooks();
    static constexpr auto Type = PED_ATTRACTOR_PIZZA;

    // 0x5EE6E0
    CPedPizzaAttractor(C2dEffectPedAttractor* effect, CEntity* entity, eMoveState moveState) :
        CPedAttractor(
            effect,
            entity,
            moveState,
            5,      // exe 0x5EE71D: push 5 (max peds; port: 1)
            1.0f,
            30000.0f,
            3000.0f,
            0.2f,   // exe 0x86C458 = 0.2 (arrive range; port: 0.125)
            0.1f,
            0.1f,
            0.1f
        )
    {
    }

    // 0x5EE740
    ePedAttractorType GetType() const override { return Type; }

    // 0x5EE750
    virtual float GetHeadOfQueueWaitTime() const { return 2000.0f; } // new virtual in the exe vtable (slot 6)
};
