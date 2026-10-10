/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Pickups.h"
#include "Garages.h"
#include "tPickupMessage.h"
#include "Radar.h"
#include "Shadows.h"
#include "Coronas.h"
#include "TaskSimpleJetPack.h"
#include "Clock.h"
#include "Sprite.h"
#include "PostEffects.h"
#include "Fx/FxFtol.h"

using namespace ModelIndices;

static uint32 GenerateNewOneRaw(CVector coors, uint32 modelId, ePickupType pickupType, uint32 ammo, uint32 moneyPerDay, bool isEmpty, char* message);

void CPickups::InjectHooks() {
    RH_ScopedClass(CPickups);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x454A70);
    RH_ScopedInstall(ReInit, 0x456E60);
    RH_ScopedInstall(AddToCollectedPickupsArray, 0x455240);
    RH_ScopedOverloadedInstall(CreatePickupCoorsCloseToCoors, "", 0x458A80, void(*)(float, float, float, float&, float&, float&));
    RH_ScopedInstall(CreateSomeMoney, 0x458970);
    RH_ScopedInstall(DetonateMinesHitByGunShot, 0x4590C0);
    RH_ScopedInstall(DoCollectableEffects, 0x455E20);
    RH_ScopedInstall(DoMineEffects, 0x4560E0);
    RH_ScopedInstall(DoMoneyEffects, 0x454E80);
    RH_ScopedInstall(DoPickUpEffects, 0x455720);
    RH_ScopedInstall(FindPickUpForThisObject, 0x4551C0);

    // The return value is a plain 32 bit integer, but `tPickupReference` (non trivial) is returned through a hidden pointer, so the hook is installed on a free function with the original ABI
    RH_ScopedGlobalInstall(GenerateNewOneRaw, 0x456F20);

    RH_ScopedInstall(GenerateNewOne_WeaponType, 0x457380);
    RH_ScopedInstall(GetActualPickupIndex, 0x4552A0);
    RH_ScopedInstall(GetNewUniquePickupIndex, 0x456A30);
    RH_ScopedInstall(GetUniquePickupIndex, 0x455280);
    RH_ScopedInstall(GivePlayerGoodiesWithPickUpMI, 0x4564F0);
    RH_ScopedInstall(IsPickUpPickedUp, 0x454B40);
    RH_ScopedInstall(ModelForWeapon, 0x454AC0);
    RH_ScopedInstall(PassTime, 0x455200);
    RH_ScopedInstall(PickedUpHorseShoe, 0x455390);
    RH_ScopedInstall(PickedUpOyster, 0x4552D0);
    RH_ScopedInstall(PictureTaken, 0x456A70);
    RH_ScopedInstall(PlayerCanPickUpThisWeaponTypeAtThisMoment, 0x4554C0);
    RH_ScopedInstall(RemoveMissionPickUps, 0x456DE0);
    RH_ScopedInstall(RemovePickUp, 0x4573D0);
    RH_ScopedInstall(RemovePickUpsInArea, 0x456D30);
    RH_ScopedInstall(RemovePickupObjects, 0x455470);
    RH_ScopedInstall(RemoveUnnecessaryPickups, 0x4563A0);
    RH_ScopedInstall(RenderPickUpText, 0x455000);
    RH_ScopedInstall(TestForPickupsInBubble, 0x456450);
    RH_ScopedInstall(TryToMerge_WeaponType, 0x4555A0);
    RH_ScopedInstall(Update, 0x458DE0);
    RH_ScopedInstall(UpdateMoneyPerDay, 0x455680);
    RH_ScopedInstall(WeaponForModel, 0x454AE0);
    RH_ScopedInstall(Load, 0x5D35A0);
    RH_ScopedInstall(Save, 0x5D3540);

    RH_ScopedGlobalInstall(ModifyStringLabelForControlSetting, 0x454B70);
}

// 0x454A70
void CPickups::Init() {
    ZoneScoped;

    NumMessages = 0;
    for (auto& pickup : aPickUps) {
        pickup.m_nPickupType = PICKUP_NONE;
        pickup.m_nReferenceIndex = 1;
        pickup.m_pObject = nullptr;
    }
    aPickUpsCollected.fill(0);
    CollectedPickUpIndex = 0u;
    DisplayHelpMessage = 10u;
}

// 0x456E60
void CPickups::ReInit() {
    rng::for_each(GetAllActivePickups(), [](auto& pickup) { pickup.Remove(); });
    Init();
}

// 0x455240
void CPickups::AddToCollectedPickupsArray(int32 pickupIndex) {
    aPickUpsCollected[CollectedPickUpIndex++] = GetUniquePickupIndex(pickupIndex).num;
    CollectedPickUpIndex %= std::size(aPickUpsCollected);
}

/*!
 * @addr 0x458A80
 * @brief Created a pickup close to pos (inX, inY, inZ)
 *
 * @param [out] outX, outY, outZ Created pickup's position
 */
void CPickups::CreatePickupCoorsCloseToCoors(float inX, float inY, float inZ, float& outX, float& outY, float& outZ) {
    for (int32 i = 0; i < 32; i++) {
        const auto angle = (float)(CGeneral::GetRandomNumber() & 0xFF) * (TWO_PI / 256.0f); // 0x859BBC
        CVector    cand{
            (float)(x87::sin(angle) * 1.5f + inX),
            (float)(x87::cos(angle) * 1.5f + inY),
            0.0f
        };

        bool foundGround{};
        cand.z = CWorld::FindGroundZFor3DCoord(CVector{ cand.x, cand.y, inZ }, &foundGround, nullptr) + 0.5f;
        if (!foundGround) {
            continue;
        }

        const CVector start{ inX, inY, inZ + 0.3f };
        const CVector delta = cand - start;
        const auto    len   = delta.Magnitude();
        const auto    scale = (len + 0.4f) / len;
        const CVector end   = start + delta * scale;

        const auto playerPos = FindPlayerCoors(-1);
        if (std::sqrt((cand.x - playerPos.x) * (cand.x - playerPos.x) + (cand.y - playerPos.y) * (cand.y - playerPos.y)) > 2.0f) {
            if (i <= 16 && TestForPickupsInBubble(cand, 1.3f)) {
                continue;
            }
        } else if (i <= 16) {
            continue;
        }

        if (!CWorld::GetIsLineOfSightClear(end, start, true, i < 16, false, i < 16, false, false, false)) {
            continue;
        }

        if (i <= 16 && CWorld::TestSphereAgainstWorld(cand, 1.2f, nullptr, false, true, false, false, false, false)) {
            continue;
        }

        outX = cand.x;
        outY = cand.y;
        // BUG: the original writes the candidate's Y coordinate into outZ
        outZ = notsa::IsFixBugs() ? cand.z : cand.y;
        return;
    }

    outX = inX;
    outY = inY;
    outZ = inZ + 0.4f;
}

/*!
 * @addr 0x458970
 * @brief Creates wads of money that is worth amount of money at the position coors.
 */
void CPickups::CreateSomeMoney(CVector coors, int32 amount) {
    const auto wads = std::min(amount / 20 + 1, 7);
    const auto perWad = amount / wads;

    for (auto i = 0; i < wads; i++) {
        bool result;
        coors.x += x87::sin(CGeneral::GetRandomNumberInRange(0.f, TWO_PI)) * 1.5f;
        coors.y += x87::cos(CGeneral::GetRandomNumberInRange(0.f, TWO_PI)) * 1.5f;
        coors.z = CWorld::FindGroundZFor3DCoord(coors, &result, nullptr) + 0.5f;

        if (result) {
            GenerateNewOne(coors, MI_MONEY, PICKUP_MONEY, perWad + (CGeneral::GetRandomNumber() % 4));
        }
    }
}

// 0x4590C0
void CPickups::DetonateMinesHitByGunShot(const CVector& shotOrigin, const CVector& shotTarget) {
    for (auto& pickup : aPickUps) {
        if (pickup.m_nPickupType == PICKUP_NAUTICAL_MINE_ARMED) {
            pickup.ProcessGunShot(shotOrigin, shotTarget);
        }
    }
}

// 0x455E20
void CPickups::DoCollectableEffects(CEntity* entity) {
    const auto& entityPos = entity->GetPosition();

    if (const auto d = DistanceBetweenPoints(TheCamera.GetPosition(), entityPos); d < 14.0f) {
        // shade of gray
        const auto t = (uint8)((x87::sin((float)(((uint16)std::bit_cast<uintptr_t>(entity) + (uint16)CTimer::GetTimeInMS()) % 2048) * 0.0030664064f) + 1.0f) / 2.0f * ((14.0f - d) * ExeRecip(14.0f)) * 255.0f);

        CShadows::StoreStaticShadow(
            (uint32)entity,
            SHADOW_ADDITIVE,
            gpShadowExplosionTex,
            entityPos,
            2.0f,
            0.0f,
            0.0f,
            -2.0f,
            0,
            t,
            t,
            t,
            4.0f,
            1.0f,
            40.0f,
            false,
            0.0f
        );
        CCoronas::RegisterCorona(
            (uint32)entity,
            nullptr,
            t,
            t,
            t,
            255,
            entityPos,
            0.6f,
            40.0f,
            CORONATYPE_TORUS,
            FLARETYPE_NONE,
            eCoronaReflType::CORREFL_NONE,
            eCoronaLOSCheck::LOSCHECK_OFF,
            eCoronaTrail::TRAIL_OFF,
            0.0f,
            false,
            1.5f,
            0,
            15.0f,
            false,
            false
        );
    }

    entity->GetMatrix().SetRotateZOnly(static_cast<float>(CTimer::GetTimeInMS() % 4096) * 0.0015283204f);
}

// 0x4560E0
void CPickups::DoMineEffects(CEntity* entity) {
    const auto& entityPos = entity->GetPosition();

    if (const auto d = DistanceBetweenPoints(TheCamera.GetPosition(), entityPos); d < 20.0f) {
        // shade of red
        const auto t = (uint8)((x87::sin((float)(((uint16)std::bit_cast<uintptr_t>(entity) + (uint16)CTimer::GetTimeInMS()) % 512) * 0.012265625f) + 1.0f) / 2.0f * ((20.0f - d) * ExeRecip(20.0f)) * 64.0f);

        CShadows::StoreStaticShadow(
            (uint32)entity,
            SHADOW_ADDITIVE,
            gpShadowExplosionTex,
            entityPos,
            2.0f,
            0.0f,
            0.0f,
            -2.0f,
            0,
            t,
            0,
            0,
            4.0f,
            1.0f,
            40.0f,
            false,
            0.0f
        );
        CCoronas::RegisterCorona(
            (uint32)entity,
            nullptr,
            t,
            0,
            0,
            255,
            entityPos,
            0.6f,
            40.0f,
            CORONATYPE_TORUS,
            FLARETYPE_NONE,
            eCoronaReflType::CORREFL_NONE,
            eCoronaLOSCheck::LOSCHECK_OFF,
            eCoronaTrail::TRAIL_OFF,
            0.0f,
            false,
            1.5f,
            0,
            15.0f,
            false,
            false
        );
    }

    entity->GetMatrix().SetRotateZOnly(static_cast<float>(CTimer::GetTimeInMS() % 1024) * 0.0061132815f);
}

// 0x454E80
void CPickups::DoMoneyEffects(CEntity* entity) {
    entity->GetMatrix().SetRotateZOnly(static_cast<float>(CTimer::GetTimeInMS() % 2048) * 0.0030566407f);
}

// 0x455720
void CPickups::DoPickUpEffects(CEntity* entity) {
    auto* const obj    = entity->AsObject();
    auto* const pickup = FindPickUpForThisObject(obj);

    if (obj->m_nModelIndex == MI_PICKUP_CAMERA) {
        if (TheCamera.GetActiveCam().m_nMode == MODE_CAMERA) {
            obj->objectFlags.bDoNotRender = false;
        } else {
            obj->objectFlags.bDoNotRender = true;
            if (CClock::GetGameClockHours() < 5 || CPostEffects::IsVisionFXActive()) {
                // 0x859B60 = -50.0f
                const auto alphaRoll = 100 - (int32)((float)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.0f / 32768.0f) * -50.0f);
                const auto size      = (x87::sin((float)(CTimer::GetTimeInMS() & 0x1FFF) * 0.00076660159f) + 1.7f) * 3.7f;
                CCoronas::RegisterCorona(
                    (uint32)(uintptr_t)obj + 1, // NOTSA: original passed the address of a stack slot of this function here
                    nullptr,
                    (uint8)alphaRoll,
                    (uint8)(int32)((float)alphaRoll * 0.7f),
                    (uint8)(int32)((float)alphaRoll * 0.7f),
                    255,
                    obj->GetPosition(),
                    size,
                    100.0f,
                    CORONATYPE_HEADLIGHT,
                    FLARETYPE_NONE,
                    eCoronaReflType::CORREFL_NONE,
                    eCoronaLOSCheck::LOSCHECK_OFF,
                    eCoronaTrail::TRAIL_OFF,
                    0.0f,
                    false,
                    1.5f,
                    0,
                    15.0f,
                    false,
                    false
                );
            }
        }
    } else {
        obj->objectFlags.bDoNotRender = pickup->PickUpShouldBeInvisible();
    }

    if (obj->objectFlags.bDoNotRender) {
        return;
    }

    // BUG: for some models (bribe, info, killfrenzy, property, savegame) the original never assigned `weaponType`,
    // so the stack slot of the function's argument (= the object pointer) was used as an index below
    int32 weaponType = notsa::IsFixBugs() ? 0 : (int32)(int16)(uintptr_t)obj;
    {
        const auto model = obj->m_nModelIndex;
        if (model == MI_PICKUP_ADRENALINE) {
            weaponType = 0x2F;
        } else if (model == MI_PICKUP_BODYARMOUR) {
            weaponType = 0x30;
        } else if (model == MI_PICKUP_BRIBE || model == MI_PICKUP_INFO || model == MI_PICKUP_KILLFRENZY) {
            // keeps `weaponType`
        } else if (model == MI_PICKUP_HEALTH || model == MI_PICKUP_BONUS) {
            weaponType = 0x2F;
        } else if (model == MI_PICKUP_PROPERTY) {
            // keeps `weaponType`
        } else if (model == MI_PICKUP_PROPERTY_FORSALE) {
            weaponType = 0x2F;
        } else if (model == MI_PICKUP_REVENUE) {
            weaponType = 0x35;
        } else if (model == MI_PICKUP_SAVEGAME) {
            // keeps `weaponType`
        } else if (model == MI_PICKUP_CLOTHES) {
            weaponType = 0x2F;
        } else {
            weaponType = WeaponForModel(model);
        }
    }

    if (obj->objectFlags.bPickupPropertyForSale || obj->objectFlags.bPickupInShopOutOfStock || obj->m_nBonusValue != 0 || obj->m_wCostValue != 0) {
        const auto& camPos = TheCamera.GetPosition();
        const auto& objPos = obj->GetPosition();
        const auto  dist   = std::sqrt((camPos.y - objPos.y) * (camPos.y - objPos.y) + (camPos.x - objPos.x) * (camPos.x - objPos.x));

        if (dist < 14.0f && NumMessages < MAX_PICKUP_MESSAGES) {
            const CVector worldPos{ objPos.x, objPos.y, objPos.z + 0.7f };
            CVector       screenPos;
            float         w, h;
            if (CSprite::CalcScreenCoors(worldPos, &screenPos, &w, &h, true, true)) {
                auto& msg = aMessages[NumMessages];

                msg.pos.x  = screenPos.x;
                msg.pos.y  = screenPos.y;
                msg.width  = w;
                msg.height = h;
                msg.pos.z  = std::bit_cast<float>((int32)WeaponForModel(obj->m_nModelIndex)); // NOTE: yes, the original stores it in `pos.z`

                // 0x8A5FB0: 8-byte entries { uint8 r, g, b, pad; float 1.0f }, indexed by weapon type
                struct tColorEntry { uint8 r, g, b, pad; float unused; };
                const auto& color = reinterpret_cast<const tColorEntry*>(0x8A5FB0)[weaponType];
                msg.color.r = color.r;
                msg.color.g = color.g;
                msg.color.b = color.b;
                msg.color.a = (uint8)(int32)((1.0f - dist * ExeRecip(14.0f)) * 255.0f);

                if (obj->objectFlags.bPickupInShopOutOfStock) {
                    msg.flags |= 1;
                } else {
                    msg.flags &= ~1;
                }
                msg.field_19 = obj->m_nBonusValue;
                msg.price    = obj->m_wCostValue * 5u;

                if (obj->m_nModelIndex == MI_PICKUP_PROPERTY) {
                    msg.text = const_cast<GxtChar*>(TheText.Get(CPickup::FindStringForTextIndex((ePickupPropertyText)FindPickUpForThisObject(obj)->m_nFlags.nPropertyTextIndex)));
                    msg.flags &= ~2;
                } else if (obj->m_nModelIndex == MI_PICKUP_PROPERTY_FORSALE) {
                    msg.text = const_cast<GxtChar*>(TheText.Get(CPickup::FindStringForTextIndex((ePickupPropertyText)FindPickUpForThisObject(obj)->m_nFlags.nPropertyTextIndex)));
                    msg.flags |= 2;
                } else {
                    msg.text = nullptr;
                    msg.flags &= ~2;
                }
                NumMessages++;
            }
        }
    }

    // Scale the model so that it fits into ~1.2 units
    const auto& bb   = CModelInfo::GetModelInfo(obj->m_nModelIndex)->GetColModel()->GetBoundingBox();
    const auto  dimX = bb.m_vecMax.x - bb.m_vecMin.x;
    const auto  dimY = bb.m_vecMax.y - bb.m_vecMin.y;
    const auto  dimZ = bb.m_vecMax.z - bb.m_vecMin.z;
    const auto  maxYZ = dimY > dimZ ? dimY : dimZ;
    const auto  maxDim = dimX > maxYZ ? dimX : maxYZ;

    auto invScale = 1.2f / maxDim;
    if (invScale < 1.0f) {
        invScale = 1.0f;
    }
    auto scale = (invScale - 1.0f) * 0.6f + 1.0f;
    if (obj->m_nModelIndex == 0x16A) {
        scale = 1.2f;
    }

    const auto angle = (float)(CTimer::GetTimeInMS() & 0x7FF) * 0.0030566407f;
    const float cs    = x87::cos(angle) * scale;
    const float sn    = x87::sin(angle) * scale;

    auto& mat = obj->GetMatrix();
    mat.GetRight() = CVector{ cs, sn, 0.0f };
    mat.GetUp()    = CVector{ -sn, cs, 0.0f };
    mat.GetForward() = CVector{ 0.0f, 0.0f, scale };
}

// 0x4551C0
CPickup* CPickups::FindPickUpForThisObject(CObject* object) {
    for (auto& pickup : GetAllActivePickups()) {
        if (pickup.m_pObject == object) {
            return &pickup;
        }
    }
    return aPickUps.data();
}

// 0x456F20 - NOTSA: the original returns the handle as a plain 32 bit integer in `eax` (cdecl), which `tPickupReference` (hidden return pointer) can't express, so this is what's hooked
static uint32 GenerateNewOneRaw(CVector coors, uint32 modelId, ePickupType pickupType, uint32 ammo, uint32 moneyPerDay, bool isEmpty, char* message) {
    using namespace notsa::detail;
    auto& pickups = CPickups::aPickUps;

    // Reuses the pickup object slot of an old pickup
    const auto FreeObjectOf = [](CPickup& pickup) {
        if (pickup.m_pObject) {
            CWorld::Remove(pickup.m_pObject);
            delete pickup.m_pObject;
            pickup.m_pObject = nullptr;
        }
    };
    const auto FindFirst = [&](auto&& pred) {
        for (auto i = 0u; i < pickups.size(); i++) {
            if (pred(pickups[i].m_nPickupType)) {
                return (int32)i;
            }
        }
        return (int32)pickups.size();
    };

    int32 slot = -1;
    bool  reuse{}; // Whenever an used slot has to be cleaned up
    if (pickupType == PICKUP_FLOATINGPACKAGE || pickupType == PICKUP_NAUTICAL_MINE_INACTIVE || isEmpty) {
        // Search for a free slot, starting from the back
        for (auto i = (int32)pickups.size() - 1; i >= 0; i--) {
            if (pickups[i].m_nPickupType == PICKUP_NONE) {
                slot = i;
                break;
            }
        }
    }
    if (slot < 0) {
        slot = FindFirst([](ePickupType t) { return t == PICKUP_NONE; });
        if (slot >= (int32)pickups.size()) {
            slot = FindFirst([](ePickupType t) { return t == PICKUP_MONEY; });
            if (slot >= (int32)pickups.size()) {
                slot = FindFirst([](ePickupType t) { return t == PICKUP_ONCE_TIMEOUT || t == PICKUP_ONCE_TIMEOUT_SLOW; });
                if (slot >= (int32)pickups.size()) {
                    return (uint32)-1;
                }
            }
            reuse = true;
        }
    }
    auto& pickup = pickups[slot];
    if (reuse) {
        FreeObjectOf(pickup);
    }

    pickup.m_nAmmo        = ammo;
    pickup.m_nMoneyPerDay = (uint16)moneyPerDay;
    pickup.m_nFlags.bDisabled              = false;
    pickup.m_nFlags.bEmpty                 = isEmpty;
    pickup.m_nFlags.bHelpMessageDisplayed  = false;
    pickup.m_nPickupType         = pickupType;
    pickup.m_fRevenueValue       = 0.0f;
    pickup.m_nRegenerationTime   = CTimer::m_snTimeInMilliseconds;

    const auto now = CTimer::m_snTimeInMilliseconds;
    if (pickupType == PICKUP_ONCE_TIMEOUT) {
        pickup.m_nRegenerationTime = now + 20'000;
    } else if (pickupType == PICKUP_ONCE_TIMEOUT_SLOW) {
        pickup.m_nRegenerationTime = now + 120'000;
    } else if (pickupType == PICKUP_MONEY) {
        pickup.m_nRegenerationTime = now + 30'000;
    } else {
        if (pickupType == PICKUP_MINE_INACTIVE || pickupType == PICKUP_MINE_ARMED) {
            pickup.m_nPickupType       = PICKUP_MINE_INACTIVE;
            pickup.m_nRegenerationTime = now + 1500;
        }
        if (pickupType == PICKUP_NAUTICAL_MINE_INACTIVE || pickupType == PICKUP_NAUTICAL_MINE_ARMED) {
            pickup.m_nPickupType       = PICKUP_NAUTICAL_MINE_INACTIVE;
            pickup.m_nRegenerationTime = now + 1500;
        }
    }

    pickup.m_nModelIndex = (int16)modelId;
    pickup.m_nFlags.nPropertyTextIndex = (uint8)CPickup::FindTextIndexForString(message); // 0x455500

    // x87: `x * 8.0f`, truncated (_ftol), the low 16 bits are used
    pickup.m_vecPos = CompressedLargeVector{
        (int16)Ftol((double)coors.x * 8.0),
        (int16)Ftol((double)coors.y * 8.0),
        (int16)Ftol((double)coors.z * 8.0)
    };

    pickup.m_nFlags.bVisible = pickup.IsVisible(); // 0x454C70
    pickup.m_pObject         = nullptr;
    if (pickup.m_nFlags.bVisible) {
        pickup.GiveUsAPickUpObject(pickup.m_pObject, -1); // 0x4567E0
        if (pickup.m_pObject) {
            CWorld::Add(pickup.m_pObject);
        }
    }

    if ((uint16)pickup.m_nReferenceIndex < 0xFFFE) {
        pickup.m_nReferenceIndex++;
    } else {
        pickup.m_nReferenceIndex = 1;
    }
    return ((uint32)(uint16)pickup.m_nReferenceIndex << 16) | (uint32)slot;
}

// returns pickup handle
// 0x456F20
tPickupReference CPickups::GenerateNewOne(CVector coors, uint32 modelId, ePickupType pickupType, uint32 ammo, uint32 moneyPerDay, bool isEmpty, char* message) {
    return tPickupReference((int32)GenerateNewOneRaw(coors, modelId, pickupType, ammo, moneyPerDay, isEmpty, message));
}

/*!
 *
 * @param coors Position of new pickup
 * @param weaponType Weapon type
 * @param pickupType Pickup type
 * @param ammo
 * @param isEmpty
 * @param message
 * @return Pickup handle
 * @addr 0x457380
 */
tPickupReference CPickups::GenerateNewOne_WeaponType(CVector coors, eWeaponType weaponType, ePickupType pickupType, uint32 ammo, bool isEmpty, char* message) {
    return GenerateNewOne(coors, CWeaponInfo::GetWeaponInfo(weaponType)->m_nModelId1, pickupType, ammo, 0u, isEmpty, message);
}

/*!
 * @param pickupIndex Index of pickup
 * @return -1 if this index is not actual
 * @addr 0x4552A0
 */
int32 CPickups::GetActualPickupIndex(tPickupReference pickupRef) {
    if (pickupRef.num == -1)
        return -1;

    if (pickupRef.refIndex != aPickUps.at(pickupRef.index).m_nReferenceIndex)
        return -1;

    return pickupRef.index;
}

// 0x456A30
tPickupReference CPickups::GetNewUniquePickupIndex(int32 pickupIndex) {
    auto& refIdx = aPickUps[pickupIndex].m_nReferenceIndex;
    refIdx = (refIdx == -1) ? 1 : refIdx + 1;

    return GetUniquePickupIndex(pickupIndex);
}

// returns pickup handle
// 0x455280
tPickupReference CPickups::GetUniquePickupIndex(int32 pickupIndex) {
    return tPickupReference(pickupIndex, aPickUps.at(pickupIndex).m_nReferenceIndex);
}

// returns TRUE if player got goodies
// 0x4564F0
bool CPickups::GivePlayerGoodiesWithPickUpMI(uint16 modelId, int32 playerId) {
    auto* ped = FindPlayerPed(playerId);

    if (modelId == MI_PICKUP_ADRENALINE) {
        ped->GetPlayerData()->m_bAdrenaline = true;
        ped->GetPlayerData()->m_nAdrenalineEndTime = CTimer::GetTimeInMS() + 20'000;
        ped->ResetSprintEnergy();
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_ADRENALINE);
        return true;
    }

    if (modelId == MI_PICKUP_BODYARMOUR) {
        ped->m_fArmour = (float)FindPlayerInfo(playerId).m_nMaxArmour;
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_BODY_ARMOUR);
        return true;
    }

    if (modelId == MI_PICKUP_INFO) {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_INFO);
        return true;
    }

    if (modelId == MI_PICKUP_HEALTH) {
        auto maxHealth = FindPlayerInfo(playerId).m_nMaxHealth;
        CStats::UpdateStatsAddToHealth((uint32)((float)maxHealth - ped->m_fHealth));
        ped->m_fHealth = (float)maxHealth;
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_HEALTH);
        return true;
    }

    if (modelId == MI_PICKUP_BONUS) {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_INFO);
        return true;
    }

    if (modelId == MI_PICKUP_BRIBE) {
        auto wantedLevel = std::max(+eWantedLevel::WANTED_CLEAN, +FindPlayerPed()->GetWantedLevel() - +eWantedLevel::WANTED_LEVEL_1);
        FindPlayerPed(0)->SetWantedLevel((eWantedLevel)wantedLevel);
        CStats::IncrementStat(STAT_NUMBER_OF_POLICE_BRIBES, 1.0f);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_INFO);
        return true;
    }

    if (modelId == MI_PICKUP_KILLFRENZY) {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_INFO);
        return true;
    }

    if (modelId == MODEL_JETPACK) {
        auto* task = new CTaskSimpleJetPack(nullptr, 10.0f, 0, nullptr);
        CEventScriptCommand event(3, task, 0);
        ped->GetEventGroup().Add(&event, false);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_INFO);
        return true;
    }

    if (modelId == MI_OYSTER) {
        PickedUpOyster();
        return true;
    }

    if (modelId == MI_HORSESHOE) {
        PickedUpHorseShoe();
        return true;
    }

    return false;
}

/*!
 * @brief Check if pickup was picked up, and then mark it as not picked up.
 * @addr 0x454B40
 */
bool CPickups::IsPickUpPickedUp(tPickupReference pickupRef) {
    if (const auto it = rng::find(aPickUpsCollected, pickupRef.num); it != aPickUpsCollected.end()) {
        *it = 0; // Reset
        return true;
    }
    return false;
}

/*!
 * @addr 0x454AC0
 * @returns The `nModelId1` of the given weapon type.
 */
int32 CPickups::ModelForWeapon(eWeaponType weaponType) {
    return CWeaponInfo::GetWeaponInfo(weaponType)->m_nModelId1;
}

/*!
 * @brief Update each pickup's (except if of type NONE or ASSET_REVENUE) `nRegenerationTime` field.
 * @addr 0x455200
 */
void CPickups::PassTime(uint32 time) {
    for (auto& pickup : aPickUps) {
        switch (pickup.m_nPickupType) {
        case PICKUP_NONE:
        case PICKUP_ASSET_REVENUE:
            continue;
        }

        if (pickup.m_nRegenerationTime <= time) {
            pickup.m_nRegenerationTime = 0;
        } else {
            pickup.m_nRegenerationTime -= time;
        }
    }
}

// 0x455390
void CPickups::PickedUpHorseShoe() {
    AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_COLLECTABLE1);

    CStats::IncrementStat(STAT_HORSESHOES_COLLECTED);
    CStats::IncrementStat(STAT_LUCK, 1000.f / CStats::GetStatValue(STAT_TOTAL_HORSESHOES)); // TODO: Is this some inlined stuff? (The division part)

    FindPlayerInfo().m_nMoney += 100; // originally rewarded to the player 1.

    const auto collected = CStats::GetStatValue(STAT_HORSESHOES_COLLECTED);
    const auto total = CStats::GetStatValue(STAT_TOTAL_HORSESHOES);
    if (collected == total) {
        CGarages::TriggerMessage("HO_ALL");
        FindPlayerInfo().m_nMoney += 100'000; // originally rewarded to the player 1.
    } else {
        CGarages::TriggerMessage("HO_ONE", static_cast<int16>(collected), 5000u, static_cast<int16>(total));
    }
}

// 0x4552D0
void CPickups::PickedUpOyster() {
    AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_COLLECTABLE1);

    CStats::IncrementStat(STAT_OYSTERS_COLLECTED);
    FindPlayerInfo().m_nMoney += 100; // originally rewarded to the player 1.

    const auto collected = CStats::GetStatValue(STAT_OYSTERS_COLLECTED);
    const auto total = CStats::GetStatValue(STAT_TOTAL_OYSTERS);
    if (collected == total) {
        CGarages::TriggerMessage("OY_ALL");
        FindPlayerInfo().m_nMoney += 100'000; // originally rewarded to the player 1.
    } else {
        CGarages::TriggerMessage("OY_ONE", static_cast<int16>(collected), 5000u, static_cast<int16>(total));
    }
}

// 0x456A70
void CPickups::PictureTaken() {
    std::optional<size_t> capturedPickup{};
    auto lastFoundDist = 999'999.88f; // maybe FLT_MAX

    for (auto&& [i, pickup] : rngv::enumerate(aPickUps)) {
        if (pickup.m_nPickupType != PICKUP_SNAPSHOT)
            continue;

        const auto pupPos = pickup.GetPosn();
        const auto dist = DistanceBetweenPoints(TheCamera.GetPosition(), pupPos);

        if (90.0f / TheCamera.FindCamFOV() * 20.0f > dist && dist < lastFoundDist) {
            CVector origin = pupPos;
            if (TheCamera.IsSphereVisible(pupPos, 0.2f) || TheCamera.IsSphereVisibleInMirror(pupPos, 0.2f)) {
                capturedPickup = i;
                lastFoundDist = dist;
            }
        }
    }

    if (!capturedPickup.has_value())
        return;

    aPickUps[*capturedPickup].Remove();

    FindPlayerInfo().m_nMoney += 100'000; // originally rewarded to the player 1.

    AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_COLLECTABLE1);
    CStats::IncrementStat(STAT_SNAPSHOTS_TAKEN, 1.0f);

    const auto taken = CStats::GetStatValue(STAT_SNAPSHOTS_TAKEN);
    const auto total = CStats::GetStatValue(STAT_TOTAL_SNAPSHOTS);
    if (taken == total) {
        CGarages::TriggerMessage("SN_ALL");
        FindPlayerInfo().m_nMoney += 100'000;
    } else {
        CGarages::TriggerMessage("SN_ONE", static_cast<int16>(taken), 5000u, static_cast<int16>(total));
    }
}

// 0x4554C0
bool CPickups::PlayerCanPickUpThisWeaponTypeAtThisMoment(eWeaponType weaponType) {
    if (!CWeaponInfo::GetWeaponInfo(weaponType)->flags.bAimWithArm) {
        if (FindPlayerPed()->GetIntelligence()->GetTaskJetPack()) {
            return false;
        }
    }
    return true;
}

// 0x456DE0
void CPickups::RemoveMissionPickUps() {
    for (auto&& [i, pickup] : rngv::enumerate(aPickUps)) {
        switch (pickup.m_nPickupType) {
        case PICKUP_ONCE_FOR_MISSION: {
            CRadar::ClearBlipForEntity(BLIP_PICKUP, GetUniquePickupIndex(i).num);
            pickup.GetRidOfObjects();

            pickup.m_nFlags.bDisabled = true;
            pickup.m_nPickupType = PICKUP_NONE;
            break;
        }
        }
    }
}

// 0x4573D0
void CPickups::RemovePickUp(tPickupReference pickupRef) {
    if (const auto i = GetActualPickupIndex(pickupRef); i != -1) {
        aPickUps[i].Remove();
    }
}

// 0x456D30
void CPickups::RemovePickUpsInArea(float minX, float maxX, float minY, float maxY, float minZ, float maxZ) {
    CBoundingBox bb{ { minX, minY, minZ }, { maxX, maxY, maxZ } }; // They didn't use a bounding box, but it's nicer to do so.

    for (auto& pickup : GetAllActivePickups()) {
        if (bb.IsPointWithin(pickup.GetPosn())) {
            pickup.Remove();
        }
    }
}

// 0x455470
void CPickups::RemovePickupObjects() {
    for (auto& pickup : GetAllActivePickups()) {
        if (pickup.m_pObject) {
            pickup.GetRidOfObjects();
            pickup.m_nFlags.bVisible = false;
        }
    }
}

// remove pickups with types PICKUP_ONCE_TIMEOUT and PICKUP_MONEY in area
// 0x4563A0
void CPickups::RemoveUnnecessaryPickups(const CVector& posn, float radius) {
    for (auto& pickup : aPickUps) {
        switch (pickup.m_nPickupType) {
        case PICKUP_ONCE_TIMEOUT:
        case PICKUP_MONEY: {
            if (IsPointInSphere(pickup.GetPosn(), posn, radius)) {
                pickup.Remove();
            }
            break;
        }
        }
    }
}

// 0x455000
void CPickups::RenderPickUpText() {
    GxtChar msgText[352]{};
    for (const auto& message : std::span{ aMessages.data(), NumMessages }) {
        if (message.price == 0u) {
            if (!message.text)
                continue;

            if (message.flags & 2) {
                CMessages::InsertNumberInString(message.text, 0, 0, 0, 0, 0, 0, msgText);
            }
        } else {
            AsciiToGxtChar(std::format("${:d}", message.price).c_str(), msgText);
        }

        // TODO: scaled wrong in windowed mode, but it's fine in fullscreen.
        auto scaleX = std::min(SCREEN_STRETCH_X(1.0f), message.width * ExeRecip(30.0f));
        auto scaleY = std::min(SCREEN_STRETCH_X(1.0f), message.height * ExeRecip(30.0f));

        CFont::SetProportional(true);
        CFont::SetBackground(false, false);
        CFont::SetScale(scaleX, scaleY);
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetCentreSize(SCREEN_WIDTH);
        CFont::SetColor(message.color);
        CFont::SetFontStyle(eFontStyle::FONT_PRICEDOWN);
        CFont::PrintString(message.pos.x, message.pos.y, msgText);
    }
    NumMessages = 0;
}

// check for pickups in area
// 0x456450
bool CPickups::TestForPickupsInBubble(const CVector posn, float radius) {
    // NOTE: (Possible bug) - They dont check if the pickup is active (eg: type != NONE)
    return rng::any_of(aPickUps, [sp = CSphere{ posn, radius }](const CPickup& p) {
        return sp.IsPointWithin(p.GetPosn()); // They obviously didn't use `CSphere` here, but it's nicer.
    });
}

// search for pickup in area (radius = 5.5 units) with this weapon model and pickup type and add ammo to this pickup; returns TRUE if merged
// 0x4555A0
bool CPickups::TryToMerge_WeaponType(CVector posn, eWeaponType weaponType, ePickupType pickupType, uint32 ammo, bool arg4) {
    const auto mi = CWeaponInfo::GetWeaponInfo(weaponType)->m_nModelId1;

    for (auto& pickup : aPickUps) {
        if (mi == pickup.m_nModelIndex && pickup.m_nPickupType == pickupType && IsPointInSphere(pickup.GetPosn(), posn, 5.0f)) {
            pickup.m_nAmmo += ammo;

            return true;
        }
    }
    return false;
}

// 0x458DE0
void CPickups::Update() {
    ZoneScoped;

    if (CReplay::Mode == MODE_PLAYBACK)
        return;

    auto start = 620 * (CTimer::GetFrameCounter() % 32) / 32;
    auto end   = 620 * (CTimer::GetFrameCounter() % 32 + 1) / 32;
    for (auto i = start; i < end; i++) {
        auto& pickup = aPickUps[i];
        if (pickup.m_nPickupType == PICKUP_NONE)
            continue;

        if (pickup.m_nFlags.bVisible = pickup.IsVisible()) {
            if (!pickup.m_nFlags.bDisabled && !pickup.m_pObject) {
                pickup.GiveUsAPickUpObject(pickup.m_pObject);

                if (auto& obj = pickup.m_pObject; obj) {
                    CWorld::Add(obj);
                }
            }
        } else {
            pickup.GetRidOfObjects();
        }
    }

    const auto pad = CPad::GetPad();
    if (pad->CollectPickupJustDown()) {
        CollectPickupBuffer = 6;
    } else if (CollectPickupBuffer) {
        CollectPickupBuffer--;
    }

    if (PlayerOnWeaponPickup) {
        PlayerOnWeaponPickup--;
    }

    if (pad->GetTarget()) {
        CollectPickupBuffer = 0;
    }

    const auto player1 = FindPlayerPed(PED_TYPE_PLAYER1);
    const auto p1Busy = player1->GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER) || player1->GetIntelligence()->FindTaskByType(TASK_COMPLEX_USE_MOBILE_PHONE);

    start = 620 * (CTimer::GetFrameCounter() % 6) / 6;
    end   = 620 * (CTimer::GetFrameCounter() % 6 + 1) / 6;
    for (auto i = start; i < end; i++) {
        auto& pickup = aPickUps[i];

        if (pickup.m_nPickupType == PICKUP_NONE || !pickup.m_nFlags.bVisible)
            continue;

        if (!p1Busy) {
            if (pickup.Update(FindPlayerPed(), FindPlayerVehicle(), CWorld::PlayerInFocus)) {
                AddToCollectedPickupsArray(i);
            }
        } else if (FindPlayerPed(PED_TYPE_PLAYER2)) {
            if (pickup.Update(FindPlayerPed(1), FindPlayerVehicle(1), PED_TYPE_PLAYER2)) {
                AddToCollectedPickupsArray(i);
            }
        }
    }
}

// 0x455680
void CPickups::UpdateMoneyPerDay(tPickupReference pickupRef, uint16 money) {
    if (auto idx = GetActualPickupIndex(pickupRef); idx != -1) {
        aPickUps[idx].m_nMoneyPerDay = money;
    }
}

// 0x454AE0
eWeaponType CPickups::WeaponForModel(int32 modelId) {
    if (modelId == MI_PICKUP_BODYARMOUR) {
        return WEAPON_ARMOUR;
    }
    if (modelId == MI_PICKUP_HEALTH) {
        return WEAPON_LAST_WEAPON;
    }
    if (modelId == MI_PICKUP_ADRENALINE) {
        return WEAPON_ARMOUR;
    }

    switch (modelId) {
    case MODEL_JETPACK:
        return WEAPON_LAST_WEAPON;

    case MODEL_INVALID:
        return WEAPON_UNARMED;
    }

    if (auto mi = CModelInfo::GetModelInfo(modelId); mi->GetModelType() == MODEL_INFO_WEAPON) {
        return mi->AsWeaponModelInfoPtr()->GetWeaponInfo();
    }

    return WEAPON_UNARMED;
}

// 0x5D35A0
void CPickups::Load() {
    for (auto& pickup : aPickUps) {
        CGenericGameStorage::LoadDataFromWorkBuffer(pickup);
        if (pickup.m_nPickupType != PICKUP_NONE && pickup.m_pObject) {
            pickup.m_pObject = nullptr;
            pickup.m_nFlags.bVisible = false;
        }
    }
    NumMessages = 0u;
    CGenericGameStorage::LoadDataFromWorkBuffer(CPickups::CollectedPickUpIndex);
    CGenericGameStorage::LoadDataFromWorkBuffer(CPickups::DisplayHelpMessage);

    for (auto& collected : aPickUpsCollected) {
        CGenericGameStorage::LoadDataFromWorkBuffer(collected);
    }
}
// 0x5D3540
void CPickups::Save() {
    for (auto& pickup : aPickUps) {
        CGenericGameStorage::SaveDataToWorkBuffer(pickup);
    }
    CGenericGameStorage::SaveDataToWorkBuffer(CPickups::CollectedPickUpIndex);
    CGenericGameStorage::SaveDataToWorkBuffer(CPickups::DisplayHelpMessage);

    for (auto& collected : aPickUpsCollected) {
        CGenericGameStorage::SaveDataToWorkBuffer(collected);
    }
}

// 0x454B70
void ModifyStringLabelForControlSetting(char* stringLabel) {
    const auto len = strlen(stringLabel);

    if (len < 2 || stringLabel[len - 2] != '_')
        return;

    switch (CPad::GetPad(0)->Mode) {
    case 0:
    case 1:
        stringLabel[len - 1] = 'L';
        break;
    case 2:
        stringLabel[len - 1] = 'T';
        break;
    case 3:
        stringLabel[len - 1] = 'C';
        break;
    }
}
