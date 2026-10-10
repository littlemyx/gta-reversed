/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "WeaponEffects.h"

void CWeaponEffects::InjectHooks() {
    RH_ScopedClass(CWeaponEffects);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x742AB0);
    RH_ScopedInstall(Shutdown, 0x742B80);
    RH_ScopedInstall(IsLockedOn, 0x742BD0);
    RH_ScopedInstall(MarkTarget, 0x742BF0);
    RH_ScopedInstall(ClearCrossHair, 0x742C60);
    RH_ScopedInstall(ClearCrossHairs, 0x742C80);
    RH_ScopedInstall(ClearCrossHairImmediately, 0x742CA0);
    RH_ScopedInstall(ClearCrossHairsImmediately, 0x742CC0);
    RH_ScopedInstall(Render, 0x742CF0);
}

// 0x742AB0
void CWeaponEffects::Init() {
    for (auto& crossHair : gCrossHair) {
        crossHair.m_vecPosn = CVector();
        crossHair.m_bActive = false;
        crossHair.m_color = CRGBA(255, 0, 0, 127);
        crossHair.m_fSize = 1.0f;
        crossHair.m_fRingAngle = 0.0f;
        crossHair.m_nTimeWhenToDeactivate = 0;
        crossHair.m_bClearImmediately = false;
        crossHair.m_fRotation = 0.0f;
    }

    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(CTxdStore::FindTxdSlot("particle"));
    gpCrossHairTex          = RwTextureRead("target256",  "target256m");
    gpCrossHairTexFlight[0] = RwTextureRead("lockon",     "lockonA");
    gpCrossHairTexFlight[1] = RwTextureRead("lockonFire", "lockonFireA");
    CTxdStore::PopCurrentTxd();
}

// 0x742B80
void CWeaponEffects::Shutdown() {
    for (auto& crossHair : gpCrossHairTexFlight) {
        RwTextureDestroy(crossHair);
        crossHair = nullptr;
    }

    RwTextureDestroy(gpCrossHairTex);
    gpCrossHairTex = nullptr;
}

// 0x742BD0
bool CWeaponEffects::IsLockedOn(CrossHairId id) {
    return gCrossHair[id].m_fRotation;
}

// 0x742BF0
void CWeaponEffects::MarkTarget(CrossHairId id, CVector posn, uint8 red, uint8 green, uint8 blue, uint8 alpha, float size, bool bClearImmediately) {
    auto& crossHair = gCrossHair[id];
    crossHair.m_vecPosn               = posn;
    crossHair.m_color.r               = red;
    crossHair.m_color.g               = green;
    crossHair.m_color.b               = blue;
    crossHair.m_color.a               = alpha;
    crossHair.m_bActive               = true;
    crossHair.m_fSize                 = size;
    crossHair.m_nTimeWhenToDeactivate = -1;
    crossHair.m_bClearImmediately     = bClearImmediately;
}

// 0x742C60
void CWeaponEffects::ClearCrossHair(CrossHairId id) {
    gCrossHair[id].m_nTimeWhenToDeactivate = static_cast<int32>(CTimer::GetTimeInMS() + 400);
}

// 0x742C80
void CWeaponEffects::ClearCrossHairs() {
    for (auto& crossHair : gCrossHair) {
        crossHair.m_bActive = false;
    }
}

// 0x742CA0
void CWeaponEffects::ClearCrossHairImmediately(CrossHairId id) {
    gCrossHair[id].m_nTimeWhenToDeactivate = static_cast<int32>(CTimer::GetTimeInMS() - 100);
    gCrossHair[id].m_bActive = false;
}

// 0x742CC0
void CWeaponEffects::ClearCrossHairsImmediately() {
    for (auto i = 0u; i < gCrossHair.size(); i++) {
        ClearCrossHair(i);
    }
}

// 0x742CF0
void CWeaponEffects::Render() {
    ZoneScoped;

    if (TheCamera.m_bWideScreenOn) {
        return;
    }

    constexpr float DEG_TO_RAD = 0.017453292f; // 0x8595EC
    constexpr float ROT_STEP   = 0.006135923f; // 0x865034 (2 * PI / 1024)
    constexpr float SHRINK     = 15.0f;        // 0x8D6140
    constexpr float RING_BASE  = 30.0f;        // 0x8D6148

    // Angle (in the camera's right/up plane) of the direction from `from` towards `to`
    const auto GetAngleToCamera = [](const CVector& to, const CVector& from) {
        const auto& cam = TheCamera.m_mCameraMatrix;
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float dz = to.z - from.z;
        const float up    = (cam.GetUp().x * dx + cam.GetUp().z * dz) + cam.GetUp().y * dy;
        const float right = (cam.GetRight().x * dx + cam.GetRight().z * dz) + cam.GetRight().y * dy;
        return x87::atan2(up, right);
    };

    for (auto i = 0u; i < gCrossHair.size(); i++) {
        auto&             ch     = gCrossHair[i];
        auto&             ring   = gCrossHairRingOffset[i];
        CPlayerPed* const player = CWorld::Players[i].m_pPed;

        if (ch.m_nTimeWhenToDeactivate != 0 && static_cast<uint32>(ch.m_nTimeWhenToDeactivate) < CTimer::GetTimeInMS()) {
            ch.m_bActive                = false;
            ch.m_nTimeWhenToDeactivate = 0;
            ring                        = 0.0f;
        }
        if (ch.m_nTimeWhenToDeactivate != -1) {
            ring = 0.0f;
        }
        if (!player) {
            ch.m_bActive = false;
        }
        if (!ch.m_bActive) {
            continue;
        }

        // NOTE: Field is named `m_bClearImmediately`, but it is what selects the flight lock-on style here
        if (ch.m_bClearImmediately == true) {
            CVector out{};
            float   w = 0.0f, h = 0.0f;
            if (CSprite::CalcScreenCoors(ch.m_vecPosn, &out, &w, &h, true, true)) {
                const float f = 20.0f / h;
                if (f > 1.0f) {
                    w = w * f;
                    h = f * h;
                }

                RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
                RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
                RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
                RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
                RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(RwTextureGetRaster(gpCrossHairTexFlight[0])));

                const float scales[2]{ 0.95f, 1.05f };
                for (const float scale : scales) {
                    CSprite::RenderOneXLUSprite_Rotate_Aspect(
                        out,
                        CVector2D{ std::min(28.0f, w) * scale * 1.8f, std::min(20.0f, h) * scale * 1.8f },
                        0, 0, 0,
                        255,
                        0.01f,
                        0.0f,
                        255
                    );
                }
                CSprite::RenderOneXLUSprite_Rotate_Aspect(
                    out,
                    CVector2D{ std::min(28.0f, w) * 1.8f, std::min(20.0f, h) * 1.8f },
                    ch.m_color.r, ch.m_color.g, ch.m_color.b,
                    255,
                    0.01f,
                    0.0f,
                    ch.m_color.a
                );

                RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(gpCrossHairTexFlight[1])));

                const float rot = static_cast<float>(CTimer::GetTimeInMS() & 0x3FF) * ROT_STEP;
                ch.m_fLockOnFade = ch.m_fLockOnFade - 20.0f * rot;
                if (ch.m_fLockOnFade < 0.0f) {
                    ch.m_fLockOnFade = 0.0f;
                }
                ch.m_fRotation = ch.m_fLockOnFade == 0.0f ? 1.0f : 0.0f;
                gCrossHairLockOffsetSin = x87::sin(rot) * ch.m_fLockOnFade;
                gCrossHairLockOffsetCos = x87::cos(rot) * ch.m_fLockOnFade;

                for (const float scale : scales) {
                    CSprite::RenderOneXLUSprite_Rotate_Aspect(
                        CVector{ out.x - gCrossHairLockOffsetSin, out.y - gCrossHairLockOffsetCos, out.z },
                        CVector2D{ std::min(28.0f, w) * scale * 0.8f, std::min(20.0f, h) * scale * 0.8f },
                        0, 0, 0,
                        255,
                        0.01f,
                        rot,
                        255
                    );
                }
                CSprite::RenderOneXLUSprite_Rotate_Aspect(
                    CVector{ out.x - gCrossHairLockOffsetSin, out.y - gCrossHairLockOffsetCos, out.z },
                    CVector2D{ std::min(28.0f, w) * 0.8f, std::min(20.0f, h) * 0.8f },
                    ch.m_color.r, ch.m_color.g, ch.m_color.b,
                    ch.m_color.a,
                    0.01f,
                    rot,
                    255
                );
            }
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,  RWRSTATE(TRUE));
        } else {
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));

            CVector out{};
            float   w = 0.0f, h = 0.0f;
            CSprite::CalcScreenCoors(ch.m_vecPosn, &out, &w, &h, false, false); // NOTE: Result ignored

            if ((float)RsGlobal.maximumWidth < out.x || out.x < 0.0f || (float)RsGlobal.maximumHeight < out.y || out.y < 0.0f) {
                // Off-screen: draw arrows on the screen edge (co-op only)
                if (CGameLogic::IsCoopGameGoingOn()) {
                    const float angle1 = GetAngleToCamera(ch.m_vecPosn, TheCamera.m_mCameraMatrix.GetPosition());
                    const float sin1   = x87::sin(angle1);
                    const float cos1   = x87::cos(angle1);

                    const float angle2 = GetAngleToCamera(ch.m_vecPosn, player->GetPosition());
                    const float sin2   = x87::sin(angle2);
                    const float cos2   = x87::cos(angle2);

                    CVector2D v1, v2, v3;
                    v1.x = (1.0f - cos1) * SCREEN_WIDTH * 0.5f;
                    v1.y = (1.0f - sin1) * SCREEN_HEIGHT * 0.5f;

                    const float t = sin2 * 0.5f + cos2;
                    v2.x = t * SHRINK + v1.x;

                    const float a = SHRINK * sin2;
                    const float b = cos2 * SHRINK;
                    v2.y = (a + v1.y) - b * 0.5f;
                    v3.x = (b + v1.x) - a * 0.5f;

                    const float u = cos2 * 0.5f + sin2;
                    v3.y = u * SHRINK + v1.y;

                    CSprite::RenderOneXLUSprite_Triangle(v1, v2, v3, 2.5f, 0, 0, 0, 255, 1.0f, 255);

                    v1.x = static_cast<float>(cos2 * SHRINK * 0.1 + v1.x);
                    const float a2 = SHRINK * sin2;
                    v1.y = 0.1f * a2 + v1.y;
                    v2.x = t * SHRINK * 0.8f + v1.x;
                    const float b2 = cos2 * SHRINK;
                    v2.y = (a2 - 0.5f * b2) * 0.8f + v1.y;
                    v3.x = (b2 - a2 * 0.5f) * 0.8f + v1.x;
                    v3.y = u * SHRINK * 0.8f + v1.y;

                    CSprite::RenderOneXLUSprite_Triangle(v1, v2, v3, 2.5f, ch.m_color.r, ch.m_color.g, ch.m_color.b, 255, 1.0f, 255);
                }
            } else {
                // On-screen: draw 3 rotating triangles around the target
                ch.m_fSize = std::max(std::min(ch.m_fSize, 1.2f), 0.3f);

                float innerRadius = 5.0f * ch.m_fSize;
                float outerRadius = ch.m_fSize * 25.0f;

                if (out.z < 2.5f) {
                    out.z = 2.5f;
                }

                const float weaponRadius = player->GetWeaponRadiusOnScreen();
                if (weaponRadius > 0.0f) {
                    innerRadius = weaponRadius * RING_BASE;
                    outerRadius = ch.m_fSize * 20.0f + innerRadius;
                }

                RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDZERO));
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDZERO));

                for (auto j = 0; j < 3; j++) {
                    const float a = static_cast<float>(j) * 120.0f;

                    const float angle0 = DEG_TO_RAD * a + ch.m_fRingAngle;
                    const float r0     = innerRadius + ring;
                    const float x0     = -(x87::sin(angle0) * r0);
                    const float y0     = -(x87::cos(angle0) * r0);

                    const float angle1 = (a + 15.0f) * DEG_TO_RAD + ch.m_fRingAngle;
                    const float r1     = outerRadius + ring;
                    const float x1     = -(x87::sin(angle1) * r1);
                    const float y1     = -(x87::cos(angle1) * r1);

                    const float angle2 = (a - 15.0f) * DEG_TO_RAD + ch.m_fRingAngle;
                    const float x2     = -(x87::sin(angle2) * r1);
                    const float y2     = -(x87::cos(angle2) * r1);

                    CVector2D v1{ x0 + out.x, y0 + out.y };
                    CVector2D v2{ x1 + out.x, y1 + out.y };
                    CVector2D v3{ x2 + out.x, y2 + out.y };

                    CSprite::RenderOneXLUSprite_Triangle(v1, v2, v3, out.z, 0, 0, 0, 255, 1.0f, ch.m_color.a);

                    const float cx = ((v3.x + v2.x) + v1.x) * 0.33333334f;
                    const float cy = ((v3.y + v2.y) + v1.y) * 0.33333334f;
                    v1.x = (v1.x - cx) * 0.75f + cx;
                    v1.y = (v1.y - cy) * 0.75f + cy;
                    v2.x = (v2.x - cx) * 0.75f + cx;
                    v2.y = (v2.y - cy) * 0.75f + cy;
                    v3.x = (v3.x - cx) * 0.75f + cx;
                    v3.y = (v3.y - cy) * 0.75f + cy;

                    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDSRCALPHA));
                    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
                    RwRenderStateSet(rwRENDERSTATECULLMODE,  RWRSTATE(rwCULLMODECULLNONE));

                    CSprite::RenderOneXLUSprite_Triangle(v1, v2, v3, out.z, ch.m_color.r, ch.m_color.g, ch.m_color.b, 255, 1.0f, ch.m_color.a);
                }

                const auto t = ch.m_nTimeWhenToDeactivate;
                if (t != 0 && t != -1) {
                    ch.m_fRingAngle = ch.m_fRingAngle + 0.75f;
                    ring            = ring + 2.0f;
                    ch.m_fSize      = ch.m_fSize * 0.9f;
                } else {
                    ch.m_fRingAngle = ch.m_fRingAngle + 0.05f;
                }
                if (ch.m_fRingAngle > 6.2831855f) {
                    ch.m_fRingAngle = 0.0f;
                }

                if (t == 0) {
                    if (gCrossHairRingGrowing[i]) {
                        ring = ch.m_fSize + ch.m_fSize + ring;
                        if (ring > ch.m_fSize * 20.0f) {
                            gCrossHairRingGrowing[i] = false;
                        }
                    } else {
                        ring = ring - 2.0f;
                        if (ring < 0.0f) {
                            gCrossHairRingGrowing[i] = true;
                        }
                    }
                }
            }

            RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
        }
    }

    // Lock-on marker of the 2 player in-car camera mode
    if (TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
        RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(RwTextureGetRaster(gpCrossHairTex)));

        const auto& activeCam = TheCamera.m_aCams[TheCamera.m_nActiveCam];
        CSprite::RenderOneXLUSprite_Rotate_Aspect(
            CVector{ (activeCam.m_fX_Targetting + 1.0f) * SCREEN_WIDTH * 0.5f, (activeCam.m_fY_Targetting + 1.0f) * SCREEN_HEIGHT * 0.5f, 100.0f },
            CVector2D{ 10.0f, 10.0f },
            255, 128, 0,
            128,
            0.01f,
            static_cast<float>(CTimer::GetTimeInMS() & 0x3FF) * ROT_STEP,
            255
        );

        // The 2nd player's ped (or the 1st one if they are a passenger) is the one shooting
        CPlayerPed* shooter = CWorld::Players[1].m_pPed;
        {
            CPlayerPed* const ped0 = CWorld::Players[0].m_pPed;
            if (ped0->m_pVehicle && ped0->m_pVehicle->m_pDriver != ped0) {
                shooter = ped0;
            }
        }

        CEntity* target{};
        if (shooter) {
            const auto* const wi = CWeaponInfo::GetWeaponInfo(shooter->GetActiveWeapon().m_Type, shooter->GetWeaponSkill());
            target = CWeapon::FindNearestTargetEntityWithScreenCoors(
                activeCam.m_fX_Targetting,
                activeCam.m_fY_Targetting,
                wi->m_fTargetRange + wi->m_fTargetRange,
                shooter->GetPosition(),
                nullptr,
                nullptr
            );
        }

        if (target != gpLastCrossHairTarget) {
            gLastCrossHairTargetTime = CTimer::GetTimeInMS();
            gpLastCrossHairTarget    = target;
        }

        if (target) {
            CVector out{};
            float   w = 0.0f, h = 0.0f;
            if (CSprite::CalcScreenCoors(target->GetPosition(), &out, &w, &h, true, true)) {
                const float f = 20.0f / h;
                if (f > 1.0f) {
                    w = w * f;
                    h = f * h;
                }

                const int32 dt = static_cast<int32>(CTimer::GetTimeInMS() - gLastCrossHairTargetTime);
                float fade = 3.0f - static_cast<float>(dt) * 0.001953125f;
                if (fade < 1.0f) {
                    fade = 1.0f;
                }
                const int32 intensity = std::min(dt / 4 + 70, 255);

                CSprite::RenderOneXLUSprite_Rotate_Aspect(
                    out,
                    CVector2D{ fade * w, h * fade },
                    static_cast<uint8>(intensity), 0, 0,
                    static_cast<int16>(intensity),
                    0.01f,
                    static_cast<float>(CTimer::GetTimeInMS() & 0x3FF) * ROT_STEP,
                    255
                );
            }
        }

        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE,  RWRSTATE(TRUE));
    }

    for (auto i = 0; i < 2; i++) {
        CWorld::Players[i].DrawCrosshair(i); // 0x56EF90
    }
}
