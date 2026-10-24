#pragma once

#include "Escalator.h"

static constexpr size_t NUM_ESCALATORS = 32;

class CEscalators {
public:
    static void Init();
    static void Shutdown();
    static void AddOne(const CVector& vecStart, const CVector& vecBottom, const CVector& vecTop, const CVector& vecEnd, bool moveDown, CEntity* entity);
    static void Update();

    // Helpers
    static auto GetAllExists() {
        return aEscalators | rngv::filter([](const auto& e) { return e.m_Exist; });
    }

private:
    static inline NOTSA_GLOBAL(aEscalators, 0xC6E9A8, (std::array<CEscalator, NUM_ESCALATORS>), {}); VALIDATE_SIZE(aEscalators, 10752);

private:
    friend void InjectHooksMain();
    static void InjectHooks();
};
