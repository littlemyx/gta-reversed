/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Base.h"

#include "Vector.h"
#include "eWeaponType.h"

class CEntity;

class CBulletInfo {
public:
    eWeaponType m_nWeaponType;
    CEntity*    m_pCreator;
    float       m_nDestroyTime;
    bool        m_bExists;
    CVector     m_vecPosition;
    CVector     m_vecVelocity;
    int16       m_nDamage;

    static constexpr auto MAX_BULLET_INFOS{8u};

    static NOTSA_GLOBAL_DECL(CBulletInfo, aBulletInfos, 0xC88740, (std::array<CBulletInfo, 8>)); // detached: defined at the end of BulletInfo.cpp (the element type is incomplete inside the class)
    static inline NOTSA_GLOBAL(PlayerSniperBulletStart, 0xC888A0, (CVector), {});
    static inline NOTSA_GLOBAL(PlayerSniperBulletEnd, 0xC888AC, (CVector), {});

public:
    static void InjectHooks();

    static void Initialise();
    static void Shutdown();
    static void AddBullet(CEntity* creator, eWeaponType weaponType, CVector position, CVector velocity);
    static void Update();

    // NOTSA funcs:
private:
    static CBulletInfo* GetFree();
    bool IsTimeToBeDestroyed() const noexcept;
};

VALIDATE_SIZE(CBulletInfo, 0x2C);
