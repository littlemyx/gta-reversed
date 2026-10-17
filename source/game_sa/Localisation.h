/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

class CLocalisation {
public:
    static inline NOTSA_GLOBAL(nastyGame, 0xB9B7EC, (bool), {});
    static inline NOTSA_GLOBAL(germanGame, 0xB9B7ED, (bool), {});
    static inline NOTSA_GLOBAL(frenchGame, 0xB9B7EE, (bool), {});

public:
    static void InjectHooks();

    static void Initialise();
    static void SetNormalGame();
    static void SetFrenchGame();
    static void SetGermanGame();
    static bool GermanGame();
    static bool FrenchGame();
    static bool Metric();
    static bool Blood();
    static bool Porn();
    static bool ScreamsFromKills();
    static bool Prostitutes();
    static bool KickingWhenDown();
    static bool ShootLimbs();
    static bool KnockDownPeds();
    static bool KillFrenzy();
    static bool StealFromDeadPed();
    static bool KillPeds();
    static bool PedsOnFire();
};