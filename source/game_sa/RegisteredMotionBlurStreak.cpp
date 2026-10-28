#include "StdInc.h"

#include "RegisteredMotionBlurStreak.h"

#include "MotionBlurStreaks.h"

void CRegisteredMotionBlurStreak::InjectHooks()
{
    RH_ScopedClass(CRegisteredMotionBlurStreak);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Update, 0x721EE0);
    RH_ScopedInstall(Render, 0x721F70);
}

void CRegisteredMotionBlurStreak::Update()
{
    bool bAlive = false;
    for (int32 i = 2; i >= 1; --i) {
        m_avecLeftPoints[i] = m_avecLeftPoints[i - 1];
        m_avecRightPoints[i] = m_avecRightPoints[i - 1];
        m_acRenderHistory[i] = m_acRenderHistory[i - 1];
        if (bAlive || m_acRenderHistory[i])
            bAlive = true;
    }

    m_acRenderHistory[0] = false;
    if (!bAlive)
        m_nId = 0;
}

void CRegisteredMotionBlurStreak::Render()
{
    for (int32 iInd = 0; iInd < 2; ++iInd) {
        auto iAlpha = 255 - (iInd * 85);
        if (!m_acRenderHistory[0] || !m_acRenderHistory[iInd + 1])
            continue;

        // 0x721FA8..0x721FF0: INTEGER (alpha / 3) * color.a (imul 0x55555556), then fild + fmul [0x859A3C] (= 1/255 as float 0x3B808081), _ftol.
        // The port divided in float (a * alpha / 3.0F) and then by 255.0F.
        constexpr float kInv255 = std::bit_cast<float>(0x3B808081u);
        auto ucAlpha1 = NOTSA_FTOL(RwUInt8, static_cast<float>((iAlpha / 3) * (int32)m_color.a) * kInv255);
        auto ucAlpha2 = NOTSA_FTOL(RwUInt8, static_cast<float>(((iAlpha - 85) / 3) * (int32)m_color.a) * kInv255);

        auto rwColor1 = CRGBA(m_color.r, m_color.g, m_color.b, ucAlpha1).ToRwRGBA();
        auto rwColor2 = CRGBA(m_color.r, m_color.g, m_color.b, ucAlpha2).ToRwRGBA();

        RxObjSpace3DVertexSetPreLitColor(&CMotionBlurStreaks::aStreakVertices[0], &rwColor1);
        RxObjSpace3DVertexSetPreLitColor(&CMotionBlurStreaks::aStreakVertices[1], &rwColor1);
        RxObjSpace3DVertexSetPreLitColor(&CMotionBlurStreaks::aStreakVertices[2], &rwColor2);
        RxObjSpace3DVertexSetPreLitColor(&CMotionBlurStreaks::aStreakVertices[3], &rwColor2);

        RxObjSpace3DVertexSetPos(&CMotionBlurStreaks::aStreakVertices[0], &m_avecLeftPoints[iInd]);
        RxObjSpace3DVertexSetPos(&CMotionBlurStreaks::aStreakVertices[1], &m_avecRightPoints[iInd]);
        RxObjSpace3DVertexSetPos(&CMotionBlurStreaks::aStreakVertices[2], &m_avecLeftPoints[iInd + 1]);
        RxObjSpace3DVertexSetPos(&CMotionBlurStreaks::aStreakVertices[3], &m_avecRightPoints[iInd + 1]);

        LittleTest();
        if (RwIm3DTransform(CMotionBlurStreaks::aStreakVertices, CMotionBlurStreaks::NUM_STREAK_VERTICES, nullptr, rwMATRIXTYPENORMAL)) {
            RwIm3DRenderIndexedPrimitive(RwPrimitiveType::rwPRIMTYPETRILIST, CMotionBlurStreaks::aIndices, CMotionBlurStreaks::NUM_STREAK_INDICES);
            RwIm3DEnd();
        }
    }
}
