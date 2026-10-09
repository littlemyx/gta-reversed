#pragma once

class CPed;
class CEventGroup;

class CCollisionEventScanner {
public:
    //! Last time (in ms) a "quiet sound" was generated for a heavy impact (damaging hit) on a ped wearing a balaclava
    static inline auto& ms_LastImpactNoiseTime = StaticRef<uint32>(0xC0B1B0);
    //! Same, but for fast spinning physical entities
    static inline auto& ms_LastSpinningNoiseTime = StaticRef<uint32>(0xC0B1B4);

    bool m_bAlreadyHitByCar;

public:
    static void InjectHooks();

    void ScanForCollisionEvents(CPed* victim, CEventGroup* eventGroup);
};
VALIDATE_SIZE(CCollisionEventScanner, 0x1);
