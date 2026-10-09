#include "StdInc.h"

#include "RealTimeShadowManager.h"
#include "Shadows.h"

auto& g_realTimeShadowMan = StaticRef<CRealTimeShadowManager>(0xC40350);

void CRealTimeShadowManager::InjectHooks() {
    RH_ScopedClass(CRealTimeShadowManager);
    RH_ScopedCategory("Shadows");

    RH_ScopedInstall(Init, 0x7067C0);
    RH_ScopedInstall(ReInit, 0x706870);
    RH_ScopedInstall(ReturnRealTimeShadow, 0x705B30);
    RH_ScopedInstall(GetRealTimeShadow, 0x706970);
    RH_ScopedInstall(Update, 0x706AB0);
    RH_ScopedInstall(DoShadowThisFrame, 0x706BA0);
    RH_ScopedInstall(Exit, 0x706A60);
}

// 0x7067C0
void CRealTimeShadowManager::Init() {
    if (m_bInitialised) {
        return;
    }

    for (auto& shdw : m_apShadows) {
        shdw = new CRealTimeShadow();
        shdw->Create(true, 4, true);
    }

    m_BlurCamera.Create(6);

    m_GradientCamera.Create(6);
    m_GradientCamera.MakeGradientRaster();

    m_bInitialised = true;
}

// 0x706A60
void CRealTimeShadowManager::Exit() { // AKA `Shutdown`
    if (!m_bInitialised) {
        return;
    }

    for (auto& shdw : m_apShadows) {
        delete std::exchange(shdw, nullptr);
    }

    // Nice hack
    m_BlurCamera.Destroy();
    m_GradientCamera.Destroy();

    m_bInitialised = false;
}

// 0x705B30
void CRealTimeShadowManager::ReturnRealTimeShadow(CRealTimeShadow* shdw) {
    if (m_bInitialised) {
        shdw->m_pOwner->m_pShadowData = nullptr;
        shdw->m_pOwner = nullptr;
    }
}

// 0x706870
void CRealTimeShadowManager::ReInit() {
    // Recreate the camera's raster (with the same size) and attach it to the texture again
    const auto ReInitCamera = [](CShadowCamera& cam) {
        const auto oldRaster = cam.m_pRwCamera->frameBuffer;
        const auto size      = oldRaster->width;
        cam.m_pRwCamera->frameBuffer = nullptr;
        RwRasterDestroy(oldRaster);
        const auto newRaster = RwRasterCreate(size, size, 0, rwRASTERTYPECAMERATEXTURE);
        cam.m_pRwCamera->frameBuffer = newRaster;
        RwTextureSetRaster(cam.m_pRwRenderTexture, newRaster);
    };

    for (const auto shdw : m_apShadows) {
        ReInitCamera(shdw->m_camera);
        ReInitCamera(shdw->m_blurCamera);
    }
    ReInitCamera(m_BlurCamera);
    ReInitCamera(m_GradientCamera);
    m_GradientCamera.MakeGradientRaster();
}

// 0x706AB0
void CRealTimeShadowManager::Update() {
    ZoneScoped;

    if (m_bInitialised && m_bNeedsReinit) {
        ReInit();
        m_bNeedsReinit = false;
    }

    for (const auto shdw : m_apShadows) {
        if (!shdw->m_pOwner) {
            continue;
        }

        assert(shdw->m_pOwner->m_pShadowData == shdw);

        // 0x305eed - 0x305f0f: Update intensity
        constexpr auto INTENSITY_STEP = 3u;
        if (shdw->m_bKeepAlive) {
            shdw->m_nIntensity = std::min<uint8>(100u, shdw->m_nIntensity + INTENSITY_STEP);
        } else { // Fade out
            shdw->m_nIntensity = std::max<uint8>(shdw->m_nIntensity, INTENSITY_STEP) - INTENSITY_STEP; // Avoids underflow
        }

        if (shdw->m_nIntensity) {
            shdw->Update();
            CShadows::StoreRealTimeShadow(
                shdw->m_pOwner,

                CTimeCycle::m_fShadowDisplacementX[CTimeCycle::m_CurrentStoredValue],
                CTimeCycle::m_fShadowDisplacementY[CTimeCycle::m_CurrentStoredValue],

                CTimeCycle::m_fShadowFrontX[CTimeCycle::m_CurrentStoredValue],
                CTimeCycle::m_fShadowFrontY[CTimeCycle::m_CurrentStoredValue],

                CTimeCycle::m_fShadowSideX[CTimeCycle::m_CurrentStoredValue],
                CTimeCycle::m_fShadowSideY[CTimeCycle::m_CurrentStoredValue]
            );
        } else if (m_bInitialised) {
            shdw->m_pOwner->m_pShadowData = nullptr;
            shdw->m_pOwner = nullptr;
        }
    }

    // TODO: ??? - Perhaps debug code left accidentally in?
    for (const auto shdw : m_apShadows) {
        shdw->m_bKeepAlive = false;
    }
}

// 0x706970
CRealTimeShadow* CRealTimeShadowManager::GetRealTimeShadow(CPhysical* physical) {
    CRealTimeShadow* shdw{};

    const bool isMainPlayerPed = physical->GetIsTypePed() && physical->AsPed()->m_nPedType == PED_TYPE_PLAYER1;

    // Don't create new shadows if the player is driving too fast
    bool canGetShadow = true;
    if (!isMainPlayerPed) {
        const auto playerPed = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
        if (playerPed->bInVehicle) {
            if (const auto veh = playerPed->m_pVehicle) {
                if (veh->GetMoveSpeed().SquaredMagnitude() > 0.09f) {
                    canGetShadow = false;
                }
            }
        }
    }

    if (m_bInitialised && canGetShadow) {
        if (isMainPlayerPed) {
            shdw = m_apShadows[0]; // Player always gets the first one
        } else {
            // BUG: Original code never checks the first shadow (index 0, reserved for the player)
            for (auto i = 1; i < NUM_REALTIME_SHADOWS; i++) {
                if (!m_apShadows[i]->m_pOwner) {
                    shdw = m_apShadows[i]; // NOTE: Last free one wins
                }
            }
        }

        if (shdw) {
            (void)shdw->SetupForThisEntity(physical); // 0x706520
            physical->m_pShadowData = shdw;
            shdw->m_bKeepAlive = true;
            shdw->m_nIntensity = 0;
        }
    }

    return shdw;
}

// 0x706BA0
void CRealTimeShadowManager::DoShadowThisFrame(CPhysical* physical) {
    switch (g_fx.GetFxQuality()) {
    case FX_QUALITY_VERY_HIGH: // Always render
        break;
    case FX_QUALITY_HIGH: { // Only draw for main player
        if (physical->GetIsTypePed()) {
            if (physical->AsPed()->m_nPedType == PED_TYPE_PLAYER1) {
                break;
            }
        }
        return;
    }
    default: // For any other quality: skip
        return;
    }

    if (const auto shdw = physical->m_pShadowData) {
        shdw->m_bKeepAlive = true;
    } else {
        (void)GetRealTimeShadow(physical); // ???
    }
}
