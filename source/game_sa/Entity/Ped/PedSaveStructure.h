#pragma once

#include "Vector.h"
#include "Weapon.h"
#include "FileRecord.hpp"
#include "FileRecords/PedFile.hpp"
#ifndef NOTSA_P2D_HOSTTEST_NO_PED_H // the host test (tests/portable/p2d_ped_test.cpp) cannot afford the whole CPed; it only needs the two forward declarations below
#include "Ped.h"
#else
enum ePedCreatedBy : uint8;
class CPed;
#endif

class CPedSaveStructure {
public:
    CVector       m_pos;
    float         m_health;
    float         m_armor;
    CWeapon       m_weapons[13];
    ePedCreatedBy m_createdBy;
    int8          m_activeWeaponSlot;
    int8          m_areaCode;
    int32         m_nExitIndex;
    uint8         m_nFightingStyle;
    uint8         m_nAllowedAttackMoves;

    static void InjectHooks();

    void Extract(CPed* ped);
    void Construct(CPed* ped);
};
VALIDATE_SIZE(CPedSaveStructure, 0x18C); // x86 in-memory (CWeapon grows on LP64); the on-disk 0x18C is VALIDATE_SIZE_FILE(CPedSaveStructure_File) in FileRecords/PedFile.hpp

// P-2d: file layout (0x18C). The exe dumps the whole temp object raw behind a uint32 size prefix (CPed::Save/Load); under NOTSA_PORTABLE the typed CGenericGameStorage
// Save/Load go through this record. Padding bytes (0x183, 0x18A..B) were stack garbage in the exe: written as 0.
template<> struct notsa::FileRecord<CPedSaveStructure> {
    using Type = CPedSaveStructure_File;
    static Type ToFile(const CPedSaveStructure& s) {
        Type f{};
        f.pos[0] = s.m_pos.x; f.pos[1] = s.m_pos.y; f.pos[2] = s.m_pos.z;
        f.health = s.m_health;
        f.armor  = s.m_armor;
        for (size_t i = 0; i < 13; i++) {
            f.weapons[i] = FileRecord<CWeapon>::ToFile(s.m_weapons[i]);
        }
        f.createdBy          = (uint8)s.m_createdBy;
        f.activeWeaponSlot   = s.m_activeWeaponSlot;
        f.areaCode           = s.m_areaCode;
        f.exitIndex          = s.m_nExitIndex;
        f.fightingStyle      = s.m_nFightingStyle;
        f.allowedAttackMoves = s.m_nAllowedAttackMoves;
        return f;
    }
    static void FromFile(CPedSaveStructure& s, const Type& f) {
        s.m_pos.x = f.pos[0]; s.m_pos.y = f.pos[1]; s.m_pos.z = f.pos[2];
        s.m_health = f.health;
        s.m_armor  = f.armor;
        for (size_t i = 0; i < 13; i++) {
            FileRecord<CWeapon>::FromFile(s.m_weapons[i], f.weapons[i]);
        }
        s.m_createdBy           = (ePedCreatedBy)f.createdBy;
        s.m_activeWeaponSlot    = f.activeWeaponSlot;
        s.m_areaCode            = f.areaCode;
        s.m_nExitIndex          = f.exitIndex;
        s.m_nFightingStyle      = f.fightingStyle;
        s.m_nAllowedAttackMoves = f.allowedAttackMoves;
    }
    // x86: the record IS the raw object (same size, same member offsets) - proves the MSVC raw copy == the record path
    static_assert(sizeof(void*) != 4 || (sizeof(CPedSaveStructure) == sizeof(Type)
        && offsetof(CPedSaveStructure, m_pos) == offsetof(Type, pos) && offsetof(CPedSaveStructure, m_health) == offsetof(Type, health) && offsetof(CPedSaveStructure, m_armor) == offsetof(Type, armor)
        && offsetof(CPedSaveStructure, m_weapons) == offsetof(Type, weapons) && offsetof(CPedSaveStructure, m_createdBy) == offsetof(Type, createdBy)
        && offsetof(CPedSaveStructure, m_activeWeaponSlot) == offsetof(Type, activeWeaponSlot) && offsetof(CPedSaveStructure, m_areaCode) == offsetof(Type, areaCode)
        && offsetof(CPedSaveStructure, m_nExitIndex) == offsetof(Type, exitIndex) && offsetof(CPedSaveStructure, m_nFightingStyle) == offsetof(Type, fightingStyle)
        && offsetof(CPedSaveStructure, m_nAllowedAttackMoves) == offsetof(Type, allowedAttackMoves)));
};
