// Stream R, slice R-6 (.notes/STREAM_R_PLAN.md): the portable build renders every SA atomic with librw's own gl3 pipelines (portable build only).
//
//   * default pipeline  (gl3pipe.cpp defaultInstanceCB / gl3render.cpp defaultRenderCB): positions, normals, prelit colours, UV sets, per-mesh material colour,
//     texture, ambient + directional / point / spot lights of the RpWorld, fog, alpha test;
//   * skin pipeline     (gl3skin.cpp): installed by librw's Skin plugin when an atomic carries skin data (the shim's skin plugin = librw's);
//   * matfx pipeline    (gl3matfx.cpp): installed by librw's MatFX plugin for atomics with the effects flag (env map; dual / bump / uv-anim fall back to the plain render).
// They are created by librw itself (Engine::open -> gl3 driver open, Driver plugin hooks of Skin / MatFX); atomics whose `pipeline` is null (every SA atomic under
// GL3 until R-8 / R-9, see rw_stubs_gl3.cpp) use the driver default pipeline.
//
// Texture orientation (R-4 rule): the game's rasters hold the top row first (D3D order) while librw's gl3 shaders sample (u, 1 - v). With rw::gl3::d3dTexelOrder set
// (engine_gl3.cpp RwEngineOpen) the fork's instance callbacks (default and skin) store 1 - v and the matfx env matrix flips its v scale, so no shader and no per-draw
// compensation is needed and Im2D / Im3D keep their own rule (NotsaGl3TextureNeedsVFlip).
//
// Night vertex colours (CCustomBuildingDNPipeline): atomics keep the stock pipeline; PreRenderUpdate -> NVCPipelineProcess writes the blended colours into the
// geometry's prelit array under RpGeometryLock(LOCKPRELIGHT), the default instance callback re-uploads exactly the colour stream on the next draw (lockedSinceInst).
#ifdef NOTSA_RW_LIBRW
#include "standalone/Fixups.h"
#include "fakerw.h"
#include <src/gl/rwgl3impl.h>
#include <src/gl/rwgl3plg.h>

#include <cstdarg>
#include <cstdio>

#include "pipelines_gl3.h"

namespace {
void LogF(const char* fmt, ...) {
    char    buf[512];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
#ifdef NOTSA_STANDALONE_RUN
    notsa::standalone::Fixups::Log("%s", buf);
#else
    std::fprintf(stderr, "%s\n", buf);
#endif
}
} // namespace

bool NotsaGl3StockPipelinesReady() {
    if (!rw::engine) {
        return false;
    }
    const bool def  = rw::engine->driver[rw::PLATFORM_GL3] && rw::engine->driver[rw::PLATFORM_GL3]->defaultPipeline;
    const bool skin = rw::skinGlobals.pipelines[rw::PLATFORM_GL3] != nullptr;
    const bool fx   = rw::matFXGlobals.pipelines[rw::PLATFORM_GL3] != nullptr;
    return def && skin && fx;
}

// Called by RwEngineStart (standalone/rw/engine.cpp -> engine_gl3.cpp) once the GL context and the gl3 device (shaders) exist.
void RwShimPipelineEnsure() {
    LogF("R-6: stock gl3 pipelines default=%d skin=%d matfx=%d (d3dTexelOrder=%d)", rw::engine && rw::engine->driver[rw::PLATFORM_GL3] && rw::engine->driver[rw::PLATFORM_GL3]->defaultPipeline,
         rw::skinGlobals.pipelines[rw::PLATFORM_GL3] != nullptr, rw::matFXGlobals.pipelines[rw::PLATFORM_GL3] != nullptr, (int)rw::gl3::d3dTexelOrder);
    if (!NotsaGl3StockPipelinesReady()) {
        LogF("R-6: WARNING a stock pipeline is missing (Skin / MatFX plugin not attached before RwEngineStart?): atomics of that kind will use the default pipeline");
    }
}

void RwShimPipelineShutdown() {}
#endif
