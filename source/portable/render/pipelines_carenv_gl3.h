#pragma once
// Stream R / R-9 (+ R-8 second half): the GL3 twin of SA's FF "env map + specular" object pipelines (CCustomCarEnvMapPipeline::CustomPipeRenderCB,
// CCustomBuildingPipeline::CustomPipeRenderCB, CCustomBuildingDNPipeline::CustomPipeRenderCB). Portable build only.
//
// Split: the CPU side (which materials get what, the texture-matrix translation, specular power, factors; all of it exe-exact math of the D3D render callbacks) is
// computed per mesh by a *prepare callback* supplied by the game side (pipelines_saenv_game_gl3.cpp) into NotsaSaEnvMesh; this file only turns that record
// into GL3 state + a small shader pair (default.vert lighting + env/specular terms). Meshes with mode == 0 and no specular draw with librw's own default shader.
#include "fakerw.h"

enum NotsaSaEnvMode : int {
    NOTSA_SAENV_NONE        = 0,
    NOTSA_SAENV_CAR_CAM     = 1, //!< car: camera-space normal, projective (D3DTTFF_COUNT3|PROJECTED) through (sx,sy,u,v); env += tex * factor        (D3DTOP_MULTIPLYADD, ARG2 = TFACTOR)
    NOTSA_SAENV_CAR_WAVE    = 2, //!< car: 2nd UV set + (u,v) offset; env = mix(current, tex, factor)                                               (D3DTOP_BLENDFACTORALPHA)
    NOTSA_SAENV_BLD_CAM     = 3, //!< building: raw camera-space normal xy as UV (texture transform disabled); env += tex * factor                   (ARG2 = TFACTOR)
    NOTSA_SAENV_BLD_CAM_DN  = 4, //!< building day/night: as 3, env += tex * lit vertex colour                                                       (ARG2 = DIFFUSE)
};

struct NotsaSaEnvMesh {
    int          mode{NOTSA_SAENV_NONE};
    rw::Texture* envTex{};
    float        env[4]{0.f, 0.f, 0.f, 0.f}; //!< CAR_CAM: sx, sy, u, v; CAR_WAVE: u offset, v offset (D3D uv space, v down)
    float        factor{0.f};                //!< 0..1: CAR_CAM / BLD_CAM: TFACTOR grey; CAR_WAVE: TFACTOR alpha
    float        spec{0.f};                  //!< material specular (r=g=b); 0 = no specular
    float        power{0.f};                 //!< specular power
    bool         blackColor{false};          //!< "painted over" placeholder colours are rendered black (CustomPipeRenderCB isSpecialColor)
    float        lightDir[3]{0.f, 0.f, -1.f}; //!< world space, direction TOWARDS the light (= -D3DLIGHT9.Direction), only used with spec > 0
    float        lightSpec{0.65f};           //!< D3DLIGHT9.Specular of g_GameLight
};

//! Fills `out` for mesh `meshIdx` of `atomic` (material `mat`). Must set out.mode / env / spec ...; the defaults mean "plain mesh".
typedef void (*NotsaSaEnvPrepareFn)(rw::Atomic* atomic, rw::Material* mat, NotsaSaEnvMesh& out);

//! Creates a gl3 ObjPipeline (default instance callback = positions / normals / prelit / UV sets with the 1-v rule) rendering through the SA env shader. The shader pair is
//! built lazily at the first draw (needs the GL context). `pluginId` is stored in the pipeline's pluginID / pluginData like the exe's RxPipeline.
rw::ObjPipeline* NotsaGl3CreateSaEnvPipeline(NotsaSaEnvPrepareFn prepare, uint32_t pluginId);

//! GL objects of the shared shader (after RwEngineStart / at shutdown); safe when never created.
void NotsaGl3SaEnvShutdown();
//! Number of draws that used the env / specular shader since start (diagnostics / tests).
uint32_t NotsaGl3SaEnvDrawCount();
