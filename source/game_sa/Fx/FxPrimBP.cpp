#include "StdInc.h"

#include "FxPrimBP.h"
#include "FxTools.h"

void FxPrimBP_c::InjectHooks() {
    RH_ScopedClass(FxPrimBP_c);
    RH_ScopedCategory("Fx");

    RH_ScopedInstall(GetRWMatrix, 0x4A9DC0);
    RH_ScopedInstall(Load, 0x5C2010);
}

// 0x4A9CF0
FxPrimBP_c::FxPrimBP_c() {
    m_apTextures.fill(nullptr);
}

// 0x4A9D20
FxPrimBP_c::~FxPrimBP_c() {
    // FIX_BUGS: They forgot destroy third texture (texture 3 destroyed twice)
    for (auto& texture : m_apTextures) {
        RwTextureDestroy(texture);
        texture = nullptr;
    }
}

// 0x4A9DC0
void FxPrimBP_c::GetRWMatrix(RwMatrix& outMatrix) {
    if (m_pMatrixBuffered) {
        m_pMatrixBuffered->CopyToRwMatrix(outMatrix);
    } else {
        // 0x4A9DCA: the exe only stores the 12 floats (the flags / padding words of the RwMatrix stay untouched, `RwMatrixSetIdentity` would set the flags)
        outMatrix.right = { 1.f, 0.f, 0.f };
        outMatrix.up    = { 0.f, 1.f, 0.f };
        outMatrix.at    = { 0.f, 0.f, 1.f };
        outMatrix.pos   = { 0.f, 0.f, 0.f };
    }
}

// 0x5C2010
bool FxPrimBP_c::Load(FILESTREAM file, int32 version, FxName32_t* textureNames) {
    char line[256], field[128], name[128];

    // NOTSA: The original doesn't check the names of the fields, nor the number of the parsed values, so neither do we
    ReadLine(file, line, sizeof(line));
    (void)sscanf(line, "%s", field); // FX_PRIM_BASE_DATA:

    ReadLine(file, line, sizeof(line));
    (void)sscanf(line, "%s %s", field, name); // NAME:

    float mat[12]; // right, up, at, pos
    ReadLine(file, line, sizeof(line));
    (void)sscanf(
        line,
        "%s %f %f %f %f %f %f %f %f %f %f %f %f",
        field,
        &mat[0], &mat[1],  &mat[2],  // right
        &mat[3], &mat[4],  &mat[5],  // up
        &mat[6], &mat[7],  &mat[8],  // at
        &mat[9], &mat[10], &mat[11]  // pos
    );

    // If it's the identity matrix, don't allocate it
    if (mat[0] == 1.f && mat[1] == 0.f && mat[2]  == 0.f
     && mat[3] == 0.f && mat[4] == 1.f && mat[5]  == 0.f
     && mat[6] == 0.f && mat[7] == 0.f && mat[8]  == 1.f
     && mat[9] == 0.f && mat[10] == 0.f && mat[11] == 0.f
    ) {
        m_pMatrixBuffered = nullptr;
    } else {
        m_pMatrixBuffered = g_fxMan.Allocate<FxBufferedMatrix>(1);

        // The original does `(int16)_ftol(f * 32767.0f)`, with the multiplication being done in extended precision
        auto* const out = &m_pMatrixBuffered->right.x;
        static_assert(sizeof(FxBufferedMatrix) == 12 * sizeof(int16));
        for (auto i = 0; i < 12; i++) {
            out[i] = std::remove_reference_t<decltype(out[i])>{ // Construct from the pre-compressed value
                (int16)(int32)((double)mat[i] * (double)FxBufferedMatrix::FX_RECIPROCAL)
            };
        }
    }

    ReadLine(file, line, sizeof(line));
    (void)sscanf(line, "%s %s", field, textureNames[0]); // TEXTURE:

    if (version > 101) {
        ReadLine(file, line, sizeof(line));
        (void)sscanf(line, "%s %s", field, textureNames[1]); // TEXTURE2:
        ReadLine(file, line, sizeof(line));
        (void)sscanf(line, "%s %s", field, textureNames[2]); // TEXTURE3:
        ReadLine(file, line, sizeof(line));
        (void)sscanf(line, "%s %s", field, textureNames[3]); // TEXTURE4:
    }

    int32 value;

    ReadLine(file, line, sizeof(line));
    (void)sscanf(line, "%s %d", field, &value); // ALPHAON:
    m_bAlphaOn = (uint8)value != 0;

    ReadLine(file, line, sizeof(line));
    (void)sscanf(line, "%s %d", field, &value); // SRCBLENDID:
    m_nSrcBlendId = (uint8)value;

    ReadLine(file, line, sizeof(line));
    (void)sscanf(line, "%s %d", field, &value); // DSTBLENDID:
    m_nDstBlendId = (uint8)value;

    m_FxInfoManager.Load(file, version);
    return true;
}
