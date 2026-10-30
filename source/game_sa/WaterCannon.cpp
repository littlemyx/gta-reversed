#include "StdInc.h"

#include "WaterCannon.h"
#include "FireManager.h"

void CWaterCannon::InjectHooks() {
    RH_ScopedClass(CWaterCannon);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x728B40);
    RH_ScopedInstall(Update_OncePerFrame, 0x72A280);
    RH_ScopedInstall(Update_NewInput, 0x728C20);
    RH_ScopedInstall(PushPeds, 0x7295E0);
    RH_ScopedInstall(Render, 0x728DA0);
}

// 0x728B40
void CWaterCannon::Init() {
    m_nId = 0;
    m_nSectionsCount = 0;
    m_nCreationTime = CTimer::GetTimeInMS();
    std::ranges::fill(m_abUsed, false);
    m_auRenderIndices = { 0, 1, 2, 1, 3, 2, 4, 5, 6, 5, 7, 6, 8, 9, 10, 9, 11, 10 };
    m_Audio.Initialise(this);
}

// 0x72A280
void CWaterCannon::Update_OncePerFrame(int16 index) {
    const auto LIFETIME = 150;

    if (CTimer::GetTimeInMS() > m_nCreationTime + LIFETIME) {
        m_nSectionsCount = (m_nSectionsCount + 1) % SECTIONS_COUNT;
        m_abUsed[m_nSectionsCount] = false;
    }

    for (auto i = 0; i < SECTIONS_COUNT; i++) {
        if (m_abUsed[i]) {
            CVector& speed = m_sectionMoveSpeed[i];
            speed.z -= CTimer::GetTimeStep() * ExeRecip(250.0f);

            CVector& point = m_sectionPoint[i];
            point += speed * CTimer::GetTimeStep();

            // Originally done in a separate loop, but we do it here
            gFireManager.ExtinguishPointWithWater(point, 2.0f, 0.5f);
        }
    }

    if ((uint8)(CTimer::GetFrameCounter() + index) % 4 == 0) { // Notice cast to byte
        PushPeds();
    }

    // free if unused
    for (auto& used : m_abUsed) {
        if (used) {
            return;
        }
    }
    m_nId = 0;
}

// 0x728C20
void CWaterCannon::Update_NewInput(const CVector* start, const CVector* end) {
    m_sectionPoint[m_nSectionsCount]     = *start;
    m_sectionMoveSpeed[m_nSectionsCount] = *end;
    m_abUsed[m_nSectionsCount]           = true;
}

// 0x7295E0
void CWaterCannon::PushPeds() {
#ifdef FIX_BUGS
    // FIX_BUGS: R* originally used 10'000.f here, but thats bug prone (if map size gets increased)
    CVector min{ FLT_MAX, FLT_MAX, FLT_MAX }, max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
#else
    // 0x7295E0 (0x729605..0x72962D): the exe starts the box at (+10000, -10000) per axis (0x461C4000 / 0xC61C4000)
    CVector min{ 10000.0f, 10000.0f, 10000.0f }, max{ -10000.0f, -10000.0f, -10000.0f };
#endif
    // 0x729640..0x7296CC: bounding box of the active sections. The exe replaces the stored value when `!(stored < v)` (min) / `!(stored > v)` (max), so NaN and equal values are taken
    for (auto i = 0; i < SECTIONS_COUNT; i++) {
        if (!IsSectionActive(i))
            continue;

        const auto& secPos = GetSectionPosn(i);

        if (!(min.x < secPos.x)) min.x = secPos.x;
        if (!(max.x > secPos.x)) max.x = secPos.x;
        if (!(min.y < secPos.y)) min.y = secPos.y;
        if (!(max.y > secPos.y)) max.y = secPos.y;
        if (!(min.z < secPos.z)) min.z = secPos.z;
        if (!(max.z > secPos.z)) max.z = secPos.z;
    }

    for (auto& ped : NOTSA_EXE_OR(GetPedPool()->GetAllValidReverse(), GetPedPool()->GetAllValid())) {
        // 0x729719: `test byte [ped + 0x1C], 1` = CEntity::m_bUsesCollision (the port tested bMakeMassTwiceAsBig)
        if (!ped.m_bUsesCollision)
            continue;

        CVector pedPosn = ped.GetPosition();

        // 0x729730..0x7297C8: strictly inside the box on every axis (NaN leaves)
        if (!(pedPosn.x > min.x && pedPosn.x < max.x
           && pedPosn.y > min.y && pedPosn.y < max.y
           && pedPosn.z > min.z && pedPosn.z < max.z))
            continue;

        for (auto i = 0; i < SECTIONS_COUNT; i++) {
            if (!IsSectionActive(i)) // 0x7297D0
                continue;

            const CVector& secPosn = GetSectionPosn(i);

            // 0x729840..0x72987D: unrounded x87, (dz^2 + dy^2) + dx^2, strictly below 5
            {
                const x87d dz = (x87d)secPosn.z - pedPosn.z;
                const x87d dy = (x87d)secPosn.y - pedPosn.y;
                const x87d dx = (x87d)secPosn.x - pedPosn.x;
                if (!((dz * dz + dy * dy) + dx * dx < 5.0))
                    continue;
            }

            const CVector secMoveSpeed = GetSectionMoveSpeed(i);

            {
                CEventHitByWaterCannon event(secPosn, secMoveSpeed);
                ped.GetEventGroup().Add(&event, false);
            }

            ped.bWasStanding = false;
            ped.bIsStanding = false;

            ped.ApplyMoveForce({ 0.0f, 0.0f, CTimer::GetTimeStep() });

            {
                // 0x72990B..0x729969: the value stays in extended precision, a component is applied only if it points the same way as the section speed.
                // The exe ADDS THE PED'S OWN current speed (esi+0x44 / 0x48), not the section's (the port added secMoveSpeed)
                const x87d applyX = ((x87d)ExeRecip(10.0f) * secMoveSpeed.x - ped.m_vecMoveSpeed.x) * ExeRecip(10.0f);
                if (applyX * secMoveSpeed.x > 0.0)
                    ped.m_vecMoveSpeed.x = (float)(applyX + ped.m_vecMoveSpeed.x);

                const x87d applyY = ((x87d)ExeRecip(10.0f) * secMoveSpeed.y - ped.m_vecMoveSpeed.y) * ExeRecip(10.0f);
                if (applyY * secMoveSpeed.y > 0.0)
                    ped.m_vecMoveSpeed.y = (float)(applyY + ped.m_vecMoveSpeed.y);
            }

            FxPrtMult_c prtInfo{ 1.0f, 1.0f, 1.0f, 0.6f, 0.75f, 0.0f, 0.2f };

            g_fx.m_SmokeII3expand->AddParticle(pedPosn, ped.m_vecMoveSpeed * 0.3f, 0.0f, prtInfo);

            // 0x729A18: z = speed.z * -0.3 + 0.5 with a single rounding
            const CVector particleVelocity2{
                ped.m_vecMoveSpeed.x * -0.3f,
                ped.m_vecMoveSpeed.y * -0.3f,
                (float)((x87d)ped.m_vecMoveSpeed.z * -0.3f + 0.5f)
            };
            g_fx.m_SmokeII3expand->AddParticle(pedPosn, particleVelocity2, 0.0f, prtInfo);

            break;
        }
    }
}

// 0x728DA0
void CWaterCannon::Render() {
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,             RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,            RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,            RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,        RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,    RWRSTATE(rwALPHATESTFUNCTIONGREATER));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(0));

    size_t prevIdx = m_nSectionsCount % SECTIONS_COUNT;
    size_t currIdx = prevIdx == 0 ? SECTIONS_COUNT - 1 : prevIdx - 1;

    bool hasCalculatedMatrix = false;

    CVector right{}, fwd{}, up{};

    m_Audio.ClearSplashInfo();

    for (auto i = 0; i < SECTIONS_COUNT; i++) {
        if (IsSectionActive(prevIdx) && IsSectionActive(currIdx)) {
            const CVector& prevPosn = GetSectionPosn(prevIdx);
            const CVector& currPosn = GetSectionPosn(currIdx);

            const CVector currToPrevDir = prevPosn - currPosn;
            if (currToPrevDir.SquaredMagnitude() < 25.0f) {
                if (!hasCalculatedMatrix) {
                    hasCalculatedMatrix = true;

                    up = Normalized(CrossProduct(prevPosn - currPosn, TheCamera.GetForward())) * ExeRecip(20.0f);
                    right = Normalized(CVector::Random(0.0f, 1.0f)) * ExeRecip(20.0f);
                    fwd = Normalized(CVector::Random(0.0f, 1.0f)) * ExeRecip(20.0f);
                }

                const float size = (float)(i * i) / (float)SECTIONS_COUNT + 3.0f;

                static RxObjSpace3DVertex vertices[12]{}; // 0xC80550

                // Set alpha depending on current `i`. The higher, the lower the alpha.
                const float progress = (float)i / (float)SECTIONS_COUNT;
                const auto  alpha = NOTSA_FTOL(RwUInt8, 64.0f * (1.0f - progress));
                RwRGBA color{ 200, 200, 255, alpha };
                for (auto& vertice : vertices) {
                    RxObjSpace3DVertexSetPreLitColor(&vertice, &color);
                }

                const CVector thisUp = up * size, thisRight = right * size, thisFwd = fwd * size;
                const CVector pos[std::size(vertices)] = {
                    currPosn - thisUp,
                    currPosn + thisUp,
                    prevPosn - thisUp,
                    prevPosn + thisUp,

                    currPosn - thisRight,
                    currPosn + thisRight,
                    prevPosn - thisRight,
                    prevPosn + thisRight,

                    currPosn - thisFwd,
                    currPosn + thisFwd,
                    prevPosn - thisFwd,
                    prevPosn + thisFwd,
                };
                for (size_t v = 0; v < std::size(vertices); v++) {
                    RxObjSpace3DVertexSetPos(&vertices[v], &pos[v]);
                }

                CColPoint colPoint{};
                CEntity* hitEntity{};
                const bool hasSectionHit = CWorld::ProcessLineOfSight(prevPosn, currPosn, colPoint, hitEntity, true, true, false, false, false, false, false, false);
                if (hasSectionHit) {
                    FxPrtMult_c prtInfo{ 1.0f, 1.0f, 1.0f, 0.15f, 0.75f, 1.0f, 0.2f };
                    CVector velocity0 = colPoint.m_vecNormal * 3.0f * CVector::Random(0.2f, 1.8f);

                    for (auto n = 0; n < 2; n++) {
                        const auto unk = (float)n / CTimer::GetTimeStepInMS();

                        g_fx.m_WaterSplash->AddParticle(colPoint.m_vecPoint, velocity0, unk, prtInfo);
                        g_fx.m_WaterSplash->AddParticle(colPoint.m_vecPoint, velocity0 * 0.6f, unk, prtInfo);
                    }

                    m_Audio.SetSplashInfo(colPoint.m_vecPoint, velocity0.Magnitude());

                    break;
                }


                if (RwIm3DTransform(vertices, std::size(vertices), nullptr, rwIM3D_VERTEXRGBA))
                {
                    RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, m_auRenderIndices.data(), m_auRenderIndices.size());
                    RwIm3DEnd();
                }
            }
        }
        currIdx = prevIdx;
        if (prevIdx == 0)
            prevIdx = SECTIONS_COUNT - 1;
        else
            prevIdx--;
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
}

bool CWaterCannon::IsSectionActive(size_t idx) const {
    return m_abUsed[idx];
}

CVector& CWaterCannon::GetSectionPosn(size_t idx) {
    return m_sectionPoint[idx];
}

CVector& CWaterCannon::GetSectionMoveSpeed(size_t idx) {
    return m_sectionMoveSpeed[idx];
}
