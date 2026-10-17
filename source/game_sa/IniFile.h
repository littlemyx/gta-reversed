/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

class CIniFile {
public:
    static inline NOTSA_GLOBAL(PedNumberMultiplier, 0x8CDF14, (float), { 1.0f });
    static inline NOTSA_GLOBAL(CarNumberMultiplier, 0x8CDF18, (float), { 1.0f });

public:
    static void InjectHooks();

    static void LoadIniFile();
};
