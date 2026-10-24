/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Vector.h"
#include "CompressedVector.h"
#include "FileRecord.hpp"
#include "FileRecords/PickupFile.hpp"

class CObject;
class CVehicle;
class CPlayerPed;

enum ePickupType : uint8 {
    PICKUP_NONE = 0,
    PICKUP_IN_SHOP = 1,
    PICKUP_ON_STREET = 2,
    PICKUP_ONCE = 3,
    PICKUP_ONCE_TIMEOUT = 4,
    PICKUP_ONCE_TIMEOUT_SLOW = 5,
    PICKUP_COLLECTABLE1 = 6,
    PICKUP_IN_SHOP_OUT_OF_STOCK = 7,
    PICKUP_MONEY = 8,
    PICKUP_MINE_INACTIVE = 9,
    PICKUP_MINE_ARMED = 10,
    PICKUP_NAUTICAL_MINE_INACTIVE = 11,
    PICKUP_NAUTICAL_MINE_ARMED = 12,
    PICKUP_FLOATINGPACKAGE = 13,
    PICKUP_FLOATINGPACKAGE_FLOATING = 14,
    PICKUP_ON_STREET_SLOW = 15,
    PICKUP_ASSET_REVENUE = 16,
    PICKUP_PROPERTY_LOCKED = 17,
    PICKUP_PROPERTY_FORSALE = 18,
    PICKUP_MONEY_DOESNTDISAPPEAR = 19,
    PICKUP_SNAPSHOT = 20,
    PICKUP_2P = 21,
    PICKUP_ONCE_FOR_MISSION = 22
};

enum ePickupPropertyText : int32 {
    PICKUP_PROPERTY_TEXT_CANCEL   = 0, // "Cancel"
    PICKUP_PROPERTY_TEXT_CAN_BUY  = 1, // "Press TAB to buy ..."
    PICKUP_PROPERTY_TEXT_CANT_BUY = 2, // "You can't buy..."
};

class CPickup {
public:
    float                 m_fRevenueValue;
    CObject*              m_pObject;
    uint32                m_nAmmo;
    uint32                m_nRegenerationTime;
    CompressedLargeVector m_vecPos;
    uint16                m_nMoneyPerDay;
    int16                 m_nModelIndex;
    int16                 m_nReferenceIndex;
    ePickupType           m_nPickupType;
    struct {
        uint8 bDisabled : 1; // waiting for regeneration
        uint8 bEmpty : 1;    // no ammo
        uint8 bHelpMessageDisplayed : 1;
        uint8 bVisible : 1;
        uint8 nPropertyTextIndex : 3; // see enum ePickupPropertyText
    } m_nFlags;

public:
    static void InjectHooks();

    void SetPosn(CVector posn) { m_vecPos = posn; } // 0x454960
    [[nodiscard]] CVector GetPosn() const { return m_vecPos; } // 0x4549A0
    [[nodiscard]] CVector2D GetPosn2D() const { return GetPosn(); };    // NOTSA
    [[nodiscard]] float GetXCoord() const { return m_vecPos.x; } // 0x4549F0
    [[nodiscard]] float GetYCoord() const { return m_vecPos.y; } // 0x454A10
    [[nodiscard]] float GetZCoord() const { return m_vecPos.z; } // 0x454A30
    void SetXCoord(float coord) { m_vecPos.x = coord; }
    void SetYCoord(float coord) { m_vecPos.y = coord; }
    void SetZCoord(float coord) { m_vecPos.z = coord; }

    void ExtractAmmoFromPickup(CPlayerPed* player);
    [[nodiscard]] bool IsVisible();
    void GetRidOfObjects();
    bool PickUpShouldBeInvisible();
    void Remove();
    void GiveUsAPickUpObject(CObject*& obj, int32 slotIndex = -1);
    bool Update(CPlayerPed* player, CVehicle* vehicle, int32 playerId);
    void ProcessGunShot(const CVector& origin, const CVector& target);

    static ePickupPropertyText FindTextIndexForString(const char* message);
    static const char* FindStringForTextIndex(ePickupPropertyText index);
};
VALIDATE_SIZE(CPickup, 0x20); // x86 in-memory (m_pObject widens it to 0x28 on LP64); the on-disk 0x20 is VALIDATE_SIZE_FILE(CPickup_File) in FileRecords/PickupFile.hpp

// P-2e: file layout (0x20) used under NOTSA_PORTABLE by the typed CGenericGameStorage Save/Load (CPickups::Save/Load).
// ToFile: m_pObject is written as 0/1 (the exe wrote the live pointer; the loader only tests it for null). FromFile reproduces the exe loader exactly:
// `if (type != PICKUP_NONE && file.object) { m_pObject = nullptr; bVisible = false; }`; m_pObject is always null after a load (a 32-bit value cannot be a pointer here);
// PICKUP_NONE entries keep bVisible from the file.
template<> struct notsa::FileRecord<CPickup> {
    using Type = CPickup_File;
    static Type ToFile(const CPickup& p) {
        Type f{};
        f.revenueValue     = p.m_fRevenueValue;
        f.object           = p.m_pObject ? 1u : 0u;
        f.ammo             = p.m_nAmmo;
        f.regenerationTime = p.m_nRegenerationTime;
        f.pos[0] = p.m_vecPos.x.Raw(); f.pos[1] = p.m_vecPos.y.Raw(); f.pos[2] = p.m_vecPos.z.Raw();
        f.moneyPerDay      = p.m_nMoneyPerDay;
        f.modelIndex       = p.m_nModelIndex;
        f.referenceIndex   = p.m_nReferenceIndex;
        f.pickupType       = (uint8)p.m_nPickupType;
        f.flags            = uint8((p.m_nFlags.bDisabled ? kCPickupFlagDisabled : 0) | (p.m_nFlags.bEmpty ? kCPickupFlagEmpty : 0)
                                 | (p.m_nFlags.bHelpMessageDisplayed ? kCPickupFlagHelpDisplayed : 0) | (p.m_nFlags.bVisible ? kCPickupFlagVisible : 0)
                                 | ((p.m_nFlags.nPropertyTextIndex & kCPickupPropertyTextMask) << kCPickupPropertyTextShift));
        return f;
    }
    static void FromFile(CPickup& p, const Type& f) {
        p.m_fRevenueValue     = f.revenueValue;
        p.m_pObject           = nullptr;
        p.m_nAmmo             = f.ammo;
        p.m_nRegenerationTime = f.regenerationTime;
        p.m_vecPos            = CompressedLargeVector{ f.pos[0], f.pos[1], f.pos[2] }; // raw (pre-compressed) int16 values
        p.m_nMoneyPerDay      = f.moneyPerDay;
        p.m_nModelIndex       = f.modelIndex;
        p.m_nReferenceIndex   = f.referenceIndex;
        p.m_nPickupType       = (ePickupType)f.pickupType;
        p.m_nFlags.bDisabled              = (f.flags & kCPickupFlagDisabled) != 0;
        p.m_nFlags.bEmpty                 = (f.flags & kCPickupFlagEmpty) != 0;
        p.m_nFlags.bHelpMessageDisplayed  = (f.flags & kCPickupFlagHelpDisplayed) != 0;
        p.m_nFlags.bVisible               = (f.flags & kCPickupFlagVisible) != 0;
        p.m_nFlags.nPropertyTextIndex     = (f.flags >> kCPickupPropertyTextShift) & kCPickupPropertyTextMask;
        if (p.m_nPickupType != PICKUP_NONE && f.object != 0) { // the exe CPickups::Load rule (0x5D35A0)
            p.m_nFlags.bVisible = false;
        }
    }
    // x86: the record IS the raw object (same size, same member offsets) - proves the MSVC raw copy == the record path
    static_assert(sizeof(CompressedLargeVector) == 6 && sizeof(decltype(CPickup::m_nFlags)) == 1);
    static_assert(sizeof(void*) != 4 || (sizeof(CPickup) == sizeof(Type)
        && offsetof(CPickup, m_fRevenueValue) == offsetof(Type, revenueValue) && offsetof(CPickup, m_pObject) == offsetof(Type, object) && offsetof(CPickup, m_nAmmo) == offsetof(Type, ammo)
        && offsetof(CPickup, m_nRegenerationTime) == offsetof(Type, regenerationTime) && offsetof(CPickup, m_vecPos) == offsetof(Type, pos) && offsetof(CPickup, m_nMoneyPerDay) == offsetof(Type, moneyPerDay)
        && offsetof(CPickup, m_nModelIndex) == offsetof(Type, modelIndex) && offsetof(CPickup, m_nReferenceIndex) == offsetof(Type, referenceIndex)
        && offsetof(CPickup, m_nPickupType) == offsetof(Type, pickupType) && offsetof(CPickup, m_nFlags) == offsetof(Type, flags)));
};
