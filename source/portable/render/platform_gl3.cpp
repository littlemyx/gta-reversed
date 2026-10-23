// Stream R, slice R-3a: portable twin of standalone/rw/platform.cpp (excluded from the portable build, PORTABLE_EXCLUDED_RENDER_FILES).
//
// platform.cpp is the D3D9 Reset watch (vtable patch of IDirect3DDevice9::Reset, camera/showRaster wrappers that fire the restore callback). A GL context
// is never "lost" and has no Reset, so nothing is watched: the restore callback is stored (the game only does Set(Get()), Game.cpp) and never fired.
// The other symbols platform.cpp defines for the game are kept here unchanged: RpAnisotPluginAttach, the no-op hook installers, the reset counter.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

namespace {
rwD3D9DeviceRestoreCallBack s_RestoreCallback = nullptr;
}

// W: stored; never invoked (no device Reset under GL)
void _rwD3D9DeviceSetRestoreCallback(rwD3D9DeviceRestoreCallBack callback) { s_RestoreCallback = callback; }

rwD3D9DeviceRestoreCallBack _rwD3D9DeviceGetRestoreCallback(void) { return s_RestoreCallback; }

void NotsaRwRenderState_OnEngineStarted(); // standalone/rw/renderstate.cpp: RW's render-state defaults + librw cache sync

// Called by RwEngineStart (engine_gl3.cpp) once the GL context exists
void NotsaRwPlatform_OnEngineStarted() { NotsaRwRenderState_OnEngineStarted(); }

// Called by RwEngineStop (engine.cpp) before librw releases the device: nothing to unwatch
void NotsaRwPlatform_OnEngineStopping() {}

// W: RpAnisot plugin (app.cpp PluginAttach, before RwEngineOpen): the anisotropy level lives in a Texture plugin that librw registers on request only.
RwBool RpAnisotPluginAttach(void) {
    static bool s_Attached = false;
    if (!s_Attached) {
        rw::registerAnisotropyPlugin();
        s_Attached = true;
    }
    return TRUE;
}

// S: hook installers of the exe-RenderWare wrappers (game_sa/RenderWare/rw/*.cpp, not built with librw) called from InjectHooksMain.cpp: nothing to patch.
void RwCoreInjectHooks() {}
namespace RtAnim { void InjectHooks() {} }

// Number of successful device Resets since RwEngineStart (diagnostics / tests): a GL context has none
unsigned NotsaRwPlatform_DeviceResetCount() { return 0; }

#endif // NOTSA_RW_LIBRW
