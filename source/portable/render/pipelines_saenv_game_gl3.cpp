// Stream R, slice R-9 / R-8 (portable build only): the CPU half of SA's FF env-map / specular render callbacks, feeding pipelines_carenv_gl3.cpp.
//   * CCustomCarEnvMapPipeline::CustomPipeRenderCB  (0x5D9900)  -> CarPrepare
//   * CCustomBuildingPipeline::CustomPipeRenderCB   (0x5D77D0)  -> BuildingPrepare(false)
//   * CCustomBuildingDNPipeline::CustomPipeRenderCB (0x5D6480)  -> BuildingPrepare(true)
// The math below is a line-by-line copy of the per-mesh decisions and values of those callbacks (the game_sa originals keep only the D3D9 state calls, guarded out under
// NOTSA_PORTABLE): flags, spec / power, the texture-matrix translation fmod, the wave offset animation, TFACTOR bytes. What the FF device did with them is in the shader.
#include "StdInc.h"

#include "pipelines_carenv_gl3.h"
#include "CustomCarEnvMapPipeline.h"
#include "CustomBuildingDNPipeline.h"
#include "CarFXRenderer.h"
#include "app_light.h"
#include "VisibilityPlugins.h"
#include "Fx.h"

namespace {
using C = CCustomCarEnvMapPipeline;

// g_GameLight (CCustomCarEnvMapPipeline::PreRenderUpdate): direction of pDirect's frame, specular 0.65 (diffuse 0.25 / ambient 0.75 are in the vertex shader)
void FillLight(NotsaSaEnvMesh& out) {
    if (!pDirect) {
        return;
    }
    const auto dir = CVector{*RwMatrixGetAt(RwFrameGetMatrix(RpLightGetFrame(pDirect)))}.Normalized();
    out.lightDir[0] = -dir.x;
    out.lightDir[1] = -dir.y;
    out.lightDir[2] = -dir.z;
}

void CarPrepare(RpAtomic* atomic, RpMaterial* mat, NotsaSaEnvMesh& out) {
    out = NotsaSaEnvMesh{};
    const auto specIntensity = gSpecIntensity * 1.85f;
    auto* const geo          = RpAtomicGetGeometry(atomic);
    const auto atmFlags      = CVisibilityPlugins::GetAtomicId(atomic);
    const auto noReflections = (atmFlags & (ATOMIC_PIPE_NO_EXTRA_PASSES_LOD | ATOMIC_PIPE_NO_EXTRA_PASSES)) != 0;
    const auto isBlownUp     = notsa::IsFixBugs() && (atmFlags & ATOMIC_PIPE_NO_EXTRA_PASSES_LOD) && !(RpLightGetFlags(pDirect) & rpLIGHTLIGHTATOMICS);

    const bool isLightingEnabled = (geo->flags & rpGEOMETRYLIGHT) != 0; // D3DRS_LIGHTING as RW sets it from the geometry flag

    auto* const clump = RpAtomicGetClump(atomic);
    auto* const frame = clump ? RpClumpGetFrame(clump) : RpAtomicGetFrame(atomic);
    const auto* const ltm = RwFrameGetLTM(frame);
    const auto* const pos = RwMatrixGetPos(ltm);

    const auto matFlags = C::GetMaterialFlags(mat);

    const auto useSpecular = isLightingEnabled && (matFlags & C::MF_HAS_SPECULARITY) && (g_fx.GetFxQuality() >= FX_QUALITY_HIGH || !noReflections);
    if (useSpecular && !isBlownUp) {
        const auto specularity = C::GetFxSpecSpecularity(mat);
        out.spec  = std::min(1.f, specIntensity * specularity * 2.f);
        out.power = specularity * 100.f;
        FillLight(out);
    }

    // EnvCam
    if ((matFlags & C::MF_HAS_SHINE_CAM) && !noReflections && !isBlownUp) {
        const auto CalculatePos = [](float p, float s) { return std::fmod(p, s) / s; };
        out.mode     = NOTSA_SAENV_CAR_CAM;
        out.envTex   = C::GetFxEnvTexture(mat);
        out.env[2]   = -CalculatePos(pos->x, C::GetFxEnvTransSclX(mat) * 50.f);
        out.env[3]   = -CalculatePos(pos->y, C::GetFxEnvTransSclY(mat) * 50.f);
        out.env[0]   = C::GetFxEnvScaleX(mat);
        out.env[1]   = C::GetFxEnvScaleY(mat);
        const auto c = (uint32)std::min<int64>(0xFF, (int64)(((double)std::bit_cast<uint8>(C::EnvMapPlGetData(mat)->Shininess) * (double)ExeRecip(255.f) * (double)specIntensity) * 254.0));
        out.factor   = (float)c / 255.f;
    }

    // EnvWave
    if ((matFlags & C::MF_HAS_SHINE_WAVE) && !noReflections && (geo->flags & rpGEOMETRYTEXTURED2) && !isBlownUp) {
        auto* const envMapAtmData = C::AllocEnvMapPipeAtomicData(atomic);
        if (envMapAtmData) {
            auto* const env_map_atm = C::EnvMapAtmPlGetData(atomic);
            auto* const env_map_mat = C::EnvMapPlGetData(mat);

            const auto tsx = C::GetFxEnvTransSclX(mat) * 50.f, tsy = C::GetFxEnvTransSclY(mat) * 50.f;
            if (env_map_mat->RenderFrame != RWSRCGLOBAL(renderFrame)) {
                env_map_mat->RenderFrame = RWSRCGLOBAL(renderFrame);
                const auto CalcPos = [](float p, float s) {
                    const auto n = (int32)(p / s);
                    const auto t = std::abs((p - (float)(n)*s) / s);
                    return n % 2 == 0 ? 1.f - t : t;
                };
                const CVector2D currUV{CalcPos(pos->x, tsx), CalcPos(pos->y, tsy)};
                const CVector2D prevUV{CalcPos(env_map_atm->PrevPosX, tsx), CalcPos(env_map_atm->PrevPosY, tsy)};
                const auto dir = (CVector2D{*pos} - CVector2D{env_map_atm->PrevPosX, env_map_atm->PrevPosY}).Dot(CVector2D{ltm->up});
                env_map_atm->OffsetU = notsa::wrap(env_map_atm->OffsetU + std::copysignf(prevUV.CWSum() - currUV.CWSum(), dir), 0.f, 1.f);
                env_map_atm->PrevPosX = pos->x;
                env_map_atm->PrevPosY = pos->y;
            }
            auto v = std::clamp(ltm->at.x + ltm->at.y, 0.f, 0.1f);
            if (ltm->at.z < 0.f) {
                v = 1.f - v;
            }
            out.mode   = NOTSA_SAENV_CAR_WAVE;
            out.envTex = C::GetFxEnvTexture(mat);
            out.env[0] = -env_map_atm->OffsetU;
            out.env[1] = v;
            const auto a = std::min(255u, (uint32)(specIntensity * 24.f));
            out.factor = (float)a / 255.f;
        }
    }

    // "painted over" placeholder colours render black (0x5D9E62: the 24 colour bits as a little endian dword, 0xBBGGRR)
    if (isLightingEnabled) {
        const uint32 bgr = (uint32)mat->color.red | ((uint32)mat->color.green << 8) | ((uint32)mat->color.blue << 16);
        out.blackColor = notsa::contains<uint32>({
            0xFF | (0xAF << 16),
            0x00FFB9,
            0x00FF3C,
            0x003CFF,
            0x00AFFF,
            0x00 | (0xFF << 8) | (0xC8 << 16),
            0xFF00FF,
            0xFFFF00,
        }, bgr);
    }
}

void BuildingPrepare(RpMaterial* mat, NotsaSaEnvMesh& out, bool dn) {
    out = NotsaSaEnvMesh{};
    const auto matFlags = C::GetMaterialFlags(mat);
    const auto* const envMapData = C::EnvMapPlGetData(mat);
    if ((matFlags & C::MF_HAS_SHINE_CAM) && envMapData && envMapData->Texture) {
        out.mode   = dn ? NOTSA_SAENV_BLD_CAM_DN : NOTSA_SAENV_BLD_CAM;
        out.envTex = envMapData->Texture;
        auto c = (int32)(int64)(((double)std::bit_cast<uint8>(envMapData->Shininess) * (double)ExeRecip(255.f)) * 254.0);
        if (c > 0xFF) {
            c = 0xFF;
        }
        out.factor = (float)c / 255.f;
    }
}

void BuildingPrepareCB(RpAtomic*, RpMaterial* mat, NotsaSaEnvMesh& out) { BuildingPrepare(mat, out, false); }
void BuildingDNPrepareCB(RpAtomic*, RpMaterial* mat, NotsaSaEnvMesh& out) { BuildingPrepare(mat, out, true); }
} // namespace

RxPipeline* NotsaGl3CreateCarEnvPipe() { return NotsaGl3CreateSaEnvPipeline(CarPrepare, CUSTOM_CAR_ENV_MAP_PIPELINE_PLUGIN_ID); }
RxPipeline* NotsaGl3CreateBuildingPipe() { return NotsaGl3CreateSaEnvPipeline(BuildingPrepareCB, rpPDS_MAKEPIPEID(rwVENDORID_DEVELOPER, 0x9C)); }
RxPipeline* NotsaGl3CreateBuildingDNPipe() { return NotsaGl3CreateSaEnvPipeline(BuildingDNPrepareCB, CUSTOM_BUILDING_DN_PIPELINE_ID); }
