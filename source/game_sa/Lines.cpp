#include "StdInc.h"

#include "Lines.h"
#include "Fx/FxFtol.h"

void CLines::InjectHooks() {
    RH_ScopedClass(CLines);
    RH_ScopedCategoryGlobal();

    RH_ScopedOverloadedInstall(RenderLineWithClipping, "", 0x6FF4F0, void(*)(float, float, float, float, float, float, uint32, uint32));
}

// 0x6FF460
void CLines::RenderLineNoClipping(float startX, float startY, float startZ, float endX, float endY, float endZ, uint32 startColor, uint32 endColor) {
    RxObjSpace3DVertex vertices[2]{};
    RwCompatVertexPos(vertices[0]) = { startX, startY, startZ };
    vertices[0].color              = startColor >> 8 | startColor << 24;
    RwCompatVertexPos(vertices[1]) = { endX, endY, endZ };
    vertices[1].color              = endColor >> 8 | endColor << 24;

    LittleTest();
    if (RwIm3DTransform(vertices, 2u, nullptr, 0)) {
        RwIm3DRenderLine(0, 1);
        RwIm3DEnd();
    }
}

// 0x6FF4F0
void CLines::RenderLineWithClipping(float startX, float startY, float startZ, float endX, float endY, float endZ, uint32 startColor, uint32 endColor) {
    // NOTE: The original keeps all of the below at extended precision (x87), hence the `double`s.
    const x87d dx   = (x87d)startX - (x87d)endX;
    const x87d dy   = (x87d)startY - (x87d)endY;
    const x87d dz   = (x87d)startZ - (x87d)endZ;
    x87d       iters = std::sqrt(dz * dz + dy * dy + dx * dx) * (x87d)0.4f + (x87d)1.0;
    if (7.0 < iters) { // `FCOMP` + `TEST AH, 5` + `JP`: unordered (NaN) leaves `iters` as is (-> `_ftol` of NaN = 0 vertices)
        iters = (x87d)7.0;
    }
    const auto numVerts = (int16)notsa::detail::Ftol(iters); // _ftol

    // Colors are RGBA, the output is ARGB
    const int32 sb[4] = { (int32)(startColor & 0xFF), (int32)((startColor >> 8) & 0xFF), (int32)((startColor >> 16) & 0xFF), (int32)(startColor >> 24) };
    const int32 eb[4] = { (int32)(endColor & 0xFF),   (int32)((endColor >> 8) & 0xFF),   (int32)((endColor >> 16) & 0xFF),   (int32)(endColor >> 24) };
    const int32 delta[4] = { eb[0] - sb[0], eb[1] - sb[1], eb[2] - sb[2], eb[3] - sb[3] };

    // The deltas of the positions are rounded to float in the original
    const float deltaX = (float)((x87d)endX - (x87d)startX);
    const float deltaY = (float)((x87d)endY - (x87d)startY);
    const float deltaZ = (float)((x87d)endZ - (x87d)startZ);

    for (int32 i = 0; i < numVerts; i++) {
        const x87d t[2] = { (x87d)i / (x87d)numVerts, (x87d)(i + 1) / (x87d)numVerts };
        for (int32 k = 0; k < 2; k++) {
            auto& vert = TempBufferVertices.m_3d[i * 2 + k];

            const auto Channel = [&](int32 c) { return (uint32)(uint8)notsa::detail::Ftol(X87R((x87d)delta[c] * t[k]) + (x87d)sb[c]); };
            const uint32 c0 = Channel(0), c3 = Channel(3), c2 = Channel(2), c1 = Channel(1); // Same order as in the original
            vert.color = c0 << 24 | c3 << 16 | c2 << 8 | c1;

            RwCompatVertexPos(vert).x = (float)(X87R((x87d)deltaX * t[k]) + (x87d)startX);
            RwCompatVertexPos(vert).y = (float)(X87R((x87d)deltaY * t[k]) + (x87d)startY);
            RwCompatVertexPos(vert).z = (float)(X87R((x87d)deltaZ * t[k]) + (x87d)startZ);
        }
    }

    static RwImVertexIndex indices[] = { // 0x8D503C
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16, 17, 18, 19, 20, 0
    };
    LittleTest();
    if (RwIm3DTransform(TempBufferVertices.m_3d, numVerts * 2, nullptr, 0)) {
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST, indices, numVerts * 2);
        RwIm3DEnd();
    }
}

// 0x6FF790
void CLines::ImmediateLine2D(int32 startX, int32 startY, int32 endX, int32 endY, uint8 startR, uint8 startG, uint8 startB, uint8 startA, uint8 endR, uint8 endG, uint8 endB, uint8 endA) {
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));

    RwIm2DVertex vertices[] = {
        { .x = float(startX), .y = float(startY), .emissiveColor = CRGBA(startR, startG, startB, startA).ToIntARGB() },
        { .x = float(endX),   .y = float(endY),   .emissiveColor = CRGBA(endR, endG, endB, endA).ToIntARGB() }
    };
    RwIm2DRenderLine(vertices, 2, 0, 1);

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
}
