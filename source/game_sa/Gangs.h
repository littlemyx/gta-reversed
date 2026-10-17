/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include <Base.h>
#include <Enums/ePedType.h>
#include <Enums/eWeaponType.h>
#include <Enums/eModelID.h>

class CPed;
class CGangInfo;

class CGangs {
public:
    static inline NOTSA_GLOBAL(GangAttackWithCops, 0xC091E0, (std::array<bool, 10>), {});
    static NOTSA_GLOBAL_DECL(CGangs, Gang, 0xC091F0, (std::array<CGangInfo, 10>)); // detached: defined at the end of Gangs.cpp (CGangInfo is incomplete here)

public:
    static void InjectHooks();

    static void Initialise();

    static bool     Load();
    static bool     Save();
    static void     SetGangWeapons(int16 gangId, eWeaponType weapId1, eWeaponType weapId2, eWeaponType weapId3);
    static eModelID ChooseGangPedModel(eGangID gangId);

    static bool GetWillAttackPlayerWithCops(ePedType pedType);
    static void SetWillAttackPlayerWithCops(ePedType pedType, bool bAttackPlayerWithCops);
    static void SetGangPedModelOverride(int16 gangId, int8 PedModelOverride);
};
