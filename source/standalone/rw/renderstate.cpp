// P2B-02c: RwRenderStateSet / RwRenderStateGet on top of librw's render-state device (rw::SetRenderState / GetRenderState).
//
// RW's state numbering (rwRENDERSTATE*) differs from librw's, so every state is mapped here; the VALUE enums (blend, cull, filter, address, stencil
// op/func) are numerically identical to librw's (static_asserts below), the exceptions are:
//   * FOGCOLOR       RW value = 0xAARRGGBB (RWRGBALONG), librw = 0xAABBGGRR -> R/B swapped both ways.
//   * ALPHATESTFUNCTION  RW has the full compare set (NEVER..ALWAYS = D3DCMP 1..8), librw only ALWAYS/GEQUAL/LESS: the exact compare goes to
//                        D3DRS_ALPHAFUNC through librw's own D3D state cache (d3d::setRenderState), librw's enum gets the nearest equivalent.
//   * FOGENABLE / FOGTYPE / FOGDENSITY / SHADEMODE: librw does fog in its shaders and has no shade mode; these also drive the D3D fixed-function
//                        states (D3DRS_FOGENABLE / FOGTABLEMODE / FOGDENSITY / SHADEMODE) because the game's custom fixed-function pipelines draw with them.
//   * BORDERCOLOR    -> D3DSAMP_BORDERCOLOR of stage 0.   TEXTUREPERSPECTIVE: no D3D9 equivalent (kept in the shadow only).
//
// Get semantics: a state librw tracks is read back from librw (so draws made by librw itself, which change blend/cull/..., are visible - as they
// were in RW); states librw does not track come from the shadow table below (= last value passed to Set, RW's defaults before that). Without a
// started device (unit tests, early init) only the shadow is used.
//
// Needs only fakerw + librw + the CRT; excluded from the unity build (see source/CMakeLists.txt).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h> // d3d9Globals (device caps)

#include <cstdint>
#include <cstring>

namespace {

using u32 = std::uint32_t;

// Numeric identity of the value enums RW <-> librw. A mismatch here would silently corrupt every blend/cull/stencil call.
static_assert(rwBLENDZERO == rw::BLENDZERO && rwBLENDONE == rw::BLENDONE && rwBLENDSRCCOLOR == rw::BLENDSRCCOLOR && rwBLENDINVSRCCOLOR == rw::BLENDINVSRCCOLOR &&
              rwBLENDSRCALPHA == rw::BLENDSRCALPHA && rwBLENDINVSRCALPHA == rw::BLENDINVSRCALPHA && rwBLENDDESTALPHA == rw::BLENDDESTALPHA &&
              rwBLENDINVDESTALPHA == rw::BLENDINVDESTALPHA && rwBLENDDESTCOLOR == rw::BLENDDESTCOLOR && rwBLENDINVDESTCOLOR == rw::BLENDINVDESTCOLOR &&
              rwBLENDSRCALPHASAT == rw::BLENDSRCALPHASAT);
static_assert(rwCULLMODECULLNONE == rw::CULLNONE && rwCULLMODECULLBACK == rw::CULLBACK && rwCULLMODECULLFRONT == rw::CULLFRONT);
static_assert(rwSTENCILOPERATIONKEEP == rw::STENCILKEEP && rwSTENCILOPERATIONZERO == rw::STENCILZERO && rwSTENCILOPERATIONREPLACE == rw::STENCILREPLACE &&
              rwSTENCILOPERATIONINCRSAT == rw::STENCILINCSAT && rwSTENCILOPERATIONDECRSAT == rw::STENCILDECSAT && rwSTENCILOPERATIONINVERT == rw::STENCILINVERT &&
              rwSTENCILOPERATIONINCR == rw::STENCILINC && rwSTENCILOPERATIONDECR == rw::STENCILDEC);
static_assert(rwSTENCILFUNCTIONNEVER == rw::STENCILNEVER && rwSTENCILFUNCTIONLESS == rw::STENCILLESS && rwSTENCILFUNCTIONEQUAL == rw::STENCILEQUAL &&
              rwSTENCILFUNCTIONLESSEQUAL == rw::STENCILLESSEQUAL && rwSTENCILFUNCTIONGREATER == rw::STENCILGREATER && rwSTENCILFUNCTIONNOTEQUAL == rw::STENCILNOTEQUAL &&
              rwSTENCILFUNCTIONGREATEREQUAL == rw::STENCILGREATEREQUAL && rwSTENCILFUNCTIONALWAYS == rw::STENCILALWAYS);
static_assert(rwFILTERNEAREST == rw::Texture::NEAREST && rwFILTERLINEAR == rw::Texture::LINEAR && rwFILTERMIPNEAREST == rw::Texture::MIPNEAREST &&
              rwFILTERMIPLINEAR == rw::Texture::MIPLINEAR && rwFILTERLINEARMIPNEAREST == rw::Texture::LINEARMIPNEAREST && rwFILTERLINEARMIPLINEAR == rw::Texture::LINEARMIPLINEAR);
static_assert(rwTEXTUREADDRESSWRAP == rw::Texture::WRAP && rwTEXTUREADDRESSMIRROR == rw::Texture::MIRROR && rwTEXTUREADDRESSCLAMP == rw::Texture::CLAMP &&
              rwTEXTUREADDRESSBORDER == rw::Texture::BORDER);
// RW's alpha-test compare enum == D3DCMP (NEVER=1 .. ALWAYS=8)
static_assert(rwALPHATESTFUNCTIONNEVER == D3DCMP_NEVER && rwALPHATESTFUNCTIONLESS == D3DCMP_LESS && rwALPHATESTFUNCTIONEQUAL == D3DCMP_EQUAL &&
              rwALPHATESTFUNCTIONLESSEQUAL == D3DCMP_LESSEQUAL && rwALPHATESTFUNCTIONGREATER == D3DCMP_GREATER && rwALPHATESTFUNCTIONNOTEQUAL == D3DCMP_NOTEQUAL &&
              rwALPHATESTFUNCTIONGREATEREQUAL == D3DCMP_GREATEREQUAL && rwALPHATESTFUNCTIONALWAYS == D3DCMP_ALWAYS);
static_assert(rwSHADEMODEFLAT == D3DSHADE_FLAT && rwSHADEMODEGOURAUD == D3DSHADE_GOURAUD);

constexpr int kNumStates = rwRENDERSTATEALPHATESTFUNCTIONREF + 1;

// Last value passed to RwRenderStateSet (RW view, i.e. before any conversion) with RW's defaults before the first Set.
u32 g_shadow[kNumStates] = {};
bool g_shadowInit = false;

void InitShadow() {
    if (g_shadowInit) {
        return;
    }
    g_shadowInit = true;
    u32* s = g_shadow;
    s[rwRENDERSTATETEXTURERASTER]          = 0;
    s[rwRENDERSTATETEXTUREADDRESS]         = rwTEXTUREADDRESSWRAP;
    s[rwRENDERSTATETEXTUREADDRESSU]        = rwTEXTUREADDRESSWRAP;
    s[rwRENDERSTATETEXTUREADDRESSV]        = rwTEXTUREADDRESSWRAP;
    s[rwRENDERSTATETEXTUREPERSPECTIVE]     = 1;
    s[rwRENDERSTATEZTESTENABLE]            = 1;
    s[rwRENDERSTATESHADEMODE]              = rwSHADEMODEGOURAUD;
    s[rwRENDERSTATEZWRITEENABLE]           = 1;
    s[rwRENDERSTATETEXTUREFILTER]          = rwFILTERLINEAR;
    s[rwRENDERSTATESRCBLEND]               = rwBLENDSRCALPHA;
    s[rwRENDERSTATEDESTBLEND]              = rwBLENDINVSRCALPHA;
    s[rwRENDERSTATEVERTEXALPHAENABLE]      = 0;
    s[rwRENDERSTATEBORDERCOLOR]            = 0xFF000000u; // exe 0x7FCF0C stage loop: opaque black
    s[rwRENDERSTATEFOGENABLE]              = 0;
    s[rwRENDERSTATEFOGCOLOR]               = 0;
    s[rwRENDERSTATEFOGTYPE]                = rwFOGTYPELINEAR;
    s[rwRENDERSTATEFOGDENSITY]             = 0x3F800000u; // 1.0f
    s[rwRENDERSTATECULLMODE]               = rwCULLMODECULLBACK;
    s[rwRENDERSTATESTENCILENABLE]          = 0;
    s[rwRENDERSTATESTENCILFAIL]            = rwSTENCILOPERATIONKEEP;
    s[rwRENDERSTATESTENCILZFAIL]           = rwSTENCILOPERATIONKEEP;
    s[rwRENDERSTATESTENCILPASS]            = rwSTENCILOPERATIONKEEP;
    s[rwRENDERSTATESTENCILFUNCTION]        = rwSTENCILFUNCTIONALWAYS;
    s[rwRENDERSTATESTENCILFUNCTIONREF]     = 0;
    s[rwRENDERSTATESTENCILFUNCTIONMASK]    = 0xFFFFFFFFu;
    s[rwRENDERSTATESTENCILFUNCTIONWRITEMASK] = 0xFFFFFFFFu;
    s[rwRENDERSTATEALPHATESTFUNCTION]      = rwALPHATESTFUNCTIONGREATER;       // exe 0x7FCFEC: index 5
    s[rwRENDERSTATEALPHATESTFUNCTIONREF]   = 0;                                // exe 0x7FD022
}

// A started D3D9 device: only then may librw's render-state device (which dereferences the D3D9 device for some states) be called.
bool DeviceLive() {
    return rw::engine && rw::Engine::state == rw::Engine::Started && rw::d3d::d3ddevice;
}

bool IsValidState(int state) {
    // exe 0x7FE420: jump table entries 18 and 19 (the gap between FOGDENSITY = 17 and CULLMODE = 20) go to the "invalid state" return
    return state > rwRENDERSTATENARENDERSTATE && state < kNumStates && state != 18 && state != 19;
}

u32 ToD3DFogTableMode(u32 rwFogType);

// D3DPRASTERCAPS the exe tests (0x7FE43B / 0x7FE530): fog support, table + W fog
constexpr u32 kRasterFogVertexOrTable = 0x180;    // D3DPRASTERCAPS_FOGVERTEX | FOGTABLE
constexpr u32 kRasterFogTable         = 0x100;    // D3DPRASTERCAPS_FOGTABLE
constexpr u32 kRasterWFog             = 0x100000; // D3DPRASTERCAPS_WFOG
u32 RasterCaps() { return rw::d3d::d3d9Globals.caps.RasterCaps; }

// exe 0x7FE515 (rwRENDERSTATEFOGTYPE): pixel (table) fog when the device has table + W fog, otherwise vertex fog (D3DRS_FOGVERTEXMODE = 140)
void ApplyFogType(u32 rwFogType) {
    const u32 mode = ToD3DFogTableMode(rwFogType);
    const u32 caps = RasterCaps();
    if ((caps & kRasterFogTable) && (caps & kRasterWFog)) {
        rw::d3d::setRenderState(D3DRS_FOGTABLEMODE, mode);
        rw::d3d::setRenderState(140 /* D3DRS_FOGVERTEXMODE */, D3DFOG_NONE);
    } else {
        rw::d3d::setRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
        rw::d3d::setRenderState(140 /* D3DRS_FOGVERTEXMODE */, mode);
    }
}

// RW 0xAARRGGBB <-> librw 0xAABBGGRR
u32 SwapRB(u32 c) {
    return (c & 0xFF00FF00u) | ((c & 0x00FF0000u) >> 16) | ((c & 0x000000FFu) << 16);
}

u32 ToD3DFogTableMode(u32 rwFogType) {
    switch (rwFogType) {
    case rwFOGTYPEEXPONENTIAL:  return D3DFOG_EXP;
    case rwFOGTYPEEXPONENTIAL2: return D3DFOG_EXP2;
    default:                    return D3DFOG_LINEAR; // rwFOGTYPELINEAR (and the invalid NA value)
    }
}

// Value ranges of the enums librw indexes tables with; out-of-range values would read past those tables.
bool InRange(u32 v, u32 lo, u32 hi) { return v >= lo && v <= hi; }

// Nearest librw alpha-test mode for an RW compare function (keeps librw's own cache in step; the exact compare is written to D3D separately).
int NearestLibrwAlphaFunc(u32 f) {
    switch (f) {
    case rwALPHATESTFUNCTIONALWAYS:                                                 return rw::ALPHAALWAYS;
    case rwALPHATESTFUNCTIONGREATER: case rwALPHATESTFUNCTIONGREATEREQUAL:
    case rwALPHATESTFUNCTIONEQUAL: case rwALPHATESTFUNCTIONNOTEQUAL:                return rw::ALPHAGREATEREQUAL;
    default:                                                                        return rw::ALPHALESS;
    }
}

} // namespace

namespace notsa_rw02c {
// Test hook: forget the shadow (back to RW defaults).
void ResetRenderStateShadow() {
    g_shadowInit = false;
    std::memset(g_shadow, 0, sizeof(g_shadow));
}
} // namespace notsa_rw02c

//--------------------------------------------------------------------------------------------------
// A: RwRenderStateSet. Returns TRUE for every valid state (RW's fpRenderStateSet did; invalid values are ignored with FALSE).
//--------------------------------------------------------------------------------------------------
RwBool RwRenderStateSet(RwRenderState state, void* pValue) {
    if (!IsValidState(state)) {
        return FALSE;
    }
    InitShadow();
    const u32 v = static_cast<u32>(reinterpret_cast<std::uintptr_t>(pValue));
    const u32 b = v ? 1u : 0u;

    // Reject values the backend would index tables with, BEFORE touching the shadow.
    switch (state) {
    case rwRENDERSTATESRCBLEND:
    case rwRENDERSTATEDESTBLEND:       if (!InRange(v, rwBLENDZERO, rwBLENDSRCALPHASAT)) return FALSE; break;
    case rwRENDERSTATECULLMODE:        if (!InRange(v, rwCULLMODECULLNONE, rwCULLMODECULLFRONT)) return FALSE; break;
    case rwRENDERSTATESTENCILFAIL:
    case rwRENDERSTATESTENCILZFAIL:
    case rwRENDERSTATESTENCILPASS:     if (!InRange(v, rwSTENCILOPERATIONKEEP, rwSTENCILOPERATIONDECR)) return FALSE; break;
    case rwRENDERSTATESTENCILFUNCTION: if (!InRange(v, rwSTENCILFUNCTIONNEVER, rwSTENCILFUNCTIONALWAYS)) return FALSE; break;
    case rwRENDERSTATETEXTUREFILTER:   if (!InRange(v, rwFILTERNEAREST, rwFILTERLINEARMIPLINEAR)) return FALSE; break;
    case rwRENDERSTATETEXTUREADDRESS:
    case rwRENDERSTATETEXTUREADDRESSU:
    case rwRENDERSTATETEXTUREADDRESSV: if (!InRange(v, rwTEXTUREADDRESSWRAP, rwTEXTUREADDRESSBORDER)) return FALSE; break;
    case rwRENDERSTATEALPHATESTFUNCTION: if (!InRange(v, rwALPHATESTFUNCTIONNEVER, rwALPHATESTFUNCTIONALWAYS)) return FALSE; break;
    case rwRENDERSTATESHADEMODE:       if (!InRange(v, rwSHADEMODEFLAT, rwSHADEMODEGOURAUD)) return FALSE; break;
    default: break;
    }

    if (state == rwRENDERSTATETEXTUREPERSPECTIVE) {
        // exe 0x7FE8CF: no state at all, the return value is (value != 0)
        return v != 0 ? TRUE : FALSE;
    }
    if (state == rwRENDERSTATEFOGENABLE && b && !g_shadow[rwRENDERSTATEFOGENABLE] && DeviceLive() && !(RasterCaps() & kRasterFogVertexOrTable)) {
        return TRUE; // exe 0x7FE44D: fog is only switched on when the device supports vertex or table fog; the RW flag stays off
    }

    g_shadow[state] = v;
    // TEXTUREADDRESS sets both axes (what librw's own device does for stage 0)
    if (state == rwRENDERSTATETEXTUREADDRESS) {
        g_shadow[rwRENDERSTATETEXTUREADDRESSU] = v;
        g_shadow[rwRENDERSTATETEXTUREADDRESSV] = v;
    }
    if (!DeviceLive()) {
        return TRUE; // shadow only
    }

    using namespace rw;
    switch (state) {
    case rwRENDERSTATETEXTURERASTER:       SetRenderStatePtr(TEXTURERASTER, pValue); break;
    case rwRENDERSTATETEXTUREADDRESS:      SetRenderState(TEXTUREADDRESS, v); break;
    case rwRENDERSTATETEXTUREADDRESSU:     SetRenderState(TEXTUREADDRESSU, v); break;
    case rwRENDERSTATETEXTUREADDRESSV:     SetRenderState(TEXTUREADDRESSV, v); break;
    case rwRENDERSTATEZTESTENABLE:         SetRenderState(ZTESTENABLE, b); break;
    case rwRENDERSTATESHADEMODE:           d3d::setRenderState(D3DRS_SHADEMODE, v); break;
    case rwRENDERSTATEZWRITEENABLE:        SetRenderState(ZWRITEENABLE, b); break;
    case rwRENDERSTATETEXTUREFILTER:       SetRenderState(TEXTUREFILTER, v); break;
    case rwRENDERSTATESRCBLEND:            SetRenderState(SRCBLEND, v); break;
    case rwRENDERSTATEDESTBLEND:           SetRenderState(DESTBLEND, v); break;
    case rwRENDERSTATEVERTEXALPHAENABLE:   SetRenderState(VERTEXALPHA, b); break;
    case rwRENDERSTATEBORDERCOLOR:         d3d::setSamplerState(0, D3DSAMP_BORDERCOLOR, v); break;
    case rwRENDERSTATEFOGENABLE:
        SetRenderState(FOGENABLE, b);                 // librw: shader fog constants
        d3d::setRenderState(D3DRS_FOGENABLE, b);      // fixed-function pipelines
        break;
    case rwRENDERSTATEFOGCOLOR:
        SetRenderState(FOGCOLOR, SwapRB(v));          // librw also writes D3DRS_FOGCOLOR (D3DCOLOR_RGBA of the same bytes = 0xAARRGGBB)
        break;
    case rwRENDERSTATEFOGTYPE:             ApplyFogType(v); break;
    case rwRENDERSTATEFOGDENSITY:          d3d::setRenderState(D3DRS_FOGDENSITY, v); break; // float bits, as RW passed them
    case rwRENDERSTATECULLMODE:            SetRenderState(CULLMODE, v); break;
    case rwRENDERSTATESTENCILENABLE:       SetRenderState(STENCILENABLE, b); break;
    case rwRENDERSTATESTENCILFAIL:         SetRenderState(STENCILFAIL, v); break;
    case rwRENDERSTATESTENCILZFAIL:        SetRenderState(STENCILZFAIL, v); break;
    case rwRENDERSTATESTENCILPASS:         SetRenderState(STENCILPASS, v); break;
    case rwRENDERSTATESTENCILFUNCTION:     SetRenderState(STENCILFUNCTION, v); break;
    case rwRENDERSTATESTENCILFUNCTIONREF:  SetRenderState(STENCILFUNCTIONREF, v); break;
    case rwRENDERSTATESTENCILFUNCTIONMASK: SetRenderState(STENCILFUNCTIONMASK, v); break;
    case rwRENDERSTATESTENCILFUNCTIONWRITEMASK: SetRenderState(STENCILFUNCTIONWRITEMASK, v); break;
    case rwRENDERSTATEALPHATESTFUNCTION:
        SetRenderState(ALPHATESTFUNC, NearestLibrwAlphaFunc(v)); // keeps librw's cache coherent
        d3d::setRenderState(D3DRS_ALPHAFUNC, v);                  // exact compare (RW enum == D3DCMP)
        {   // exe 0x7FE96D: while alpha blending is on, the alpha test is on exactly when the compare is not ALWAYS.
            // (librw's own vertex/texture-alpha toggles write ALPHATESTENABLE = blend again; the game never selects ALWAYS.)
            u32 blend = 0;
            d3d::getRenderState(D3DRS_ALPHABLENDENABLE, &blend);
            if (blend) {
                d3d::setRenderState(D3DRS_ALPHATESTENABLE, v != rwALPHATESTFUNCTIONALWAYS ? 1u : 0u);
            }
        }
        break;
    case rwRENDERSTATEALPHATESTFUNCTIONREF: SetRenderState(ALPHATESTREF, v); break;
    default: break;
    }
    return TRUE;
}

//--------------------------------------------------------------------------------------------------
// A: RwRenderStateGet. `pValue` points at a 32-bit variable (a pointer for TEXTURERASTER).
//--------------------------------------------------------------------------------------------------
RwBool RwRenderStateGet(RwRenderState state, void* pValue) {
    // exe 0x7FD810: TEXTUREPERSPECTIVE (and the invalid states) are not gettable: FALSE, nothing written
    if (!IsValidState(state) || !pValue || state == rwRENDERSTATETEXTUREPERSPECTIVE) {
        return FALSE;
    }
    InitShadow();
    u32 v = g_shadow[state];
    RwBool ok = TRUE;
    if (state == rwRENDERSTATETEXTUREADDRESS) {
        if (g_shadow[rwRENDERSTATETEXTUREADDRESSU] == g_shadow[rwRENDERSTATETEXTUREADDRESSV]) {
            v = g_shadow[rwRENDERSTATETEXTUREADDRESSU];
        } else {
            v = 0u;
            ok = FALSE; // exe 0x7FD868: *value = 0 and FALSE when U != V
        }
    }

    if (DeviceLive()) {
        using namespace rw;
        switch (state) {
        case rwRENDERSTATETEXTURERASTER:       v = static_cast<u32>(reinterpret_cast<std::uintptr_t>(GetRenderStatePtr(TEXTURERASTER))); break;
        case rwRENDERSTATETEXTUREADDRESS: {
            const u32 u = GetRenderState(TEXTUREADDRESSU), w = GetRenderState(TEXTUREADDRESSV);
            v  = (u == w) ? u : 0u; // RW: "no single address mode" = rwTEXTUREADDRESSNATEXTUREADDRESS
            ok = (u == w) ? TRUE : FALSE;
            break;
        }
        case rwRENDERSTATETEXTUREADDRESSU:     v = GetRenderState(TEXTUREADDRESSU); break;
        case rwRENDERSTATETEXTUREADDRESSV:     v = GetRenderState(TEXTUREADDRESSV); break;
        case rwRENDERSTATEZTESTENABLE:         v = GetRenderState(ZTESTENABLE); break;
        case rwRENDERSTATEZWRITEENABLE:        v = GetRenderState(ZWRITEENABLE); break;
        case rwRENDERSTATETEXTUREFILTER:       v = GetRenderState(TEXTUREFILTER); break;
        case rwRENDERSTATESRCBLEND:            v = GetRenderState(SRCBLEND); break;
        case rwRENDERSTATEDESTBLEND:           v = GetRenderState(DESTBLEND); break;
        case rwRENDERSTATEVERTEXALPHAENABLE:   v = GetRenderState(VERTEXALPHA); break;
        case rwRENDERSTATEFOGENABLE:           v = GetRenderState(FOGENABLE); break;
        case rwRENDERSTATEFOGCOLOR:            v = SwapRB(GetRenderState(FOGCOLOR)); break;
        case rwRENDERSTATECULLMODE:            v = GetRenderState(CULLMODE); break;
        case rwRENDERSTATESTENCILENABLE:       v = GetRenderState(STENCILENABLE); break;
        case rwRENDERSTATESTENCILFAIL:         v = GetRenderState(STENCILFAIL); break;
        case rwRENDERSTATESTENCILZFAIL:        v = GetRenderState(STENCILZFAIL); break;
        case rwRENDERSTATESTENCILPASS:         v = GetRenderState(STENCILPASS); break;
        case rwRENDERSTATESTENCILFUNCTION:     v = GetRenderState(STENCILFUNCTION); break;
        case rwRENDERSTATESTENCILFUNCTIONREF:  v = GetRenderState(STENCILFUNCTIONREF); break;
        case rwRENDERSTATESTENCILFUNCTIONMASK: v = GetRenderState(STENCILFUNCTIONMASK); break;
        case rwRENDERSTATESTENCILFUNCTIONWRITEMASK: v = GetRenderState(STENCILFUNCTIONWRITEMASK); break;
        case rwRENDERSTATEALPHATESTFUNCTIONREF: v = GetRenderState(ALPHATESTREF); break;
        default: break; // shade mode, perspective, border colour, fog type/density, alpha function: shadow (librw does not track them exactly)
        }
    }
    *static_cast<u32*>(pValue) = v; // x86: sizeof(RwRaster*) == 4
    return ok;
}
//--------------------------------------------------------------------------------------------------
// Called once the device exists (platform.cpp: NotsaRwPlatform_OnEngineStarted <- RwEngineStart).
// RW's render-state layer starts from its own defaults (exe 0x7FCAC0..0x7FD0F6 / the Reset at 0x7FD100); librw's caches start from other values
// (ZTEST / ZWRITE cached as 0 while the device has them TRUE - so a first RwRenderStateSet(ZTESTENABLE, 0) was dropped -, cull NONE, alpha
// compare GEQUAL/10, linear filter unset ...). Re-state every default through RwRenderStateSet so librw's caches and the device agree with RW.
//--------------------------------------------------------------------------------------------------
void NotsaRwRenderState_OnEngineStarted() {
    if (!DeviceLive()) {
        return;
    }
    using namespace rw;
    notsa_rw02c::ResetRenderStateShadow();
    InitShadow();
    const auto set = [](RwRenderState state, u32 value) { RwRenderStateSet(state, reinterpret_cast<void*>(static_cast<std::uintptr_t>(value))); };

    // order of the exe's defaults; the cached-as-0 states first, so a cache that already matches the device still gets its write
    set(rwRENDERSTATEZTESTENABLE, 1);
    set(rwRENDERSTATEZWRITEENABLE, 1);
    set(rwRENDERSTATESHADEMODE, rwSHADEMODEGOURAUD);
    set(rwRENDERSTATETEXTUREFILTER, rwFILTERLINEAR);
    set(rwRENDERSTATETEXTUREADDRESS, rwTEXTUREADDRESSWRAP);
    set(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA);
    set(rwRENDERSTATEDESTBLEND, rwBLENDINVSRCALPHA);
    set(rwRENDERSTATEVERTEXALPHAENABLE, 0);
    set(rwRENDERSTATEBORDERCOLOR, 0xFF000000u);
    set(rwRENDERSTATEFOGENABLE, 0);
    set(rwRENDERSTATEFOGCOLOR, 0);
    set(rwRENDERSTATEFOGTYPE, rwFOGTYPELINEAR);
    set(rwRENDERSTATEFOGDENSITY, 0x3F800000u);
    set(rwRENDERSTATECULLMODE, rwCULLMODECULLBACK);
    set(rwRENDERSTATESTENCILENABLE, 0);
    set(rwRENDERSTATESTENCILFAIL, rwSTENCILOPERATIONKEEP);
    set(rwRENDERSTATESTENCILZFAIL, rwSTENCILOPERATIONKEEP);
    set(rwRENDERSTATESTENCILPASS, rwSTENCILOPERATIONKEEP);
    set(rwRENDERSTATESTENCILFUNCTION, rwSTENCILFUNCTIONALWAYS);
    set(rwRENDERSTATESTENCILFUNCTIONREF, 0);
    set(rwRENDERSTATESTENCILFUNCTIONMASK, 0xFFFFFFFFu);
    set(rwRENDERSTATESTENCILFUNCTIONWRITEMASK, 0xFFFFFFFFu);
    set(rwRENDERSTATEALPHATESTFUNCTION, rwALPHATESTFUNCTIONGREATER);
    set(rwRENDERSTATEALPHATESTFUNCTIONREF, 0);

    // fixed-function defaults of the same exe code (D3D9 FF state RW owns; ids as in D3DRS_*)
    RwD3D9SetRenderState(D3DRS_LIGHTING, FALSE);
    RwD3D9SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
    RwD3D9SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
    RwD3D9SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
    RwD3D9SetRenderState(D3DRS_DITHERENABLE, RasterCaps() & D3DPRASTERCAPS_DITHER ? TRUE : FALSE);
    RwD3D9SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    RwD3D9SetRenderState(D3DRS_LOCALVIEWER, FALSE);
    RwD3D9SetRenderState(D3DRS_AMBIENT, 0xFFFFFFFFu);
    RwD3D9SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
    // stage 0 passes the vertex colour through, the other stages are off (exe 0x7FD06D / the stage loop at 0x7FCF0C)
    RwD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    RwD3D9SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    for (RwUInt32 st = 1; st < 8; st++) {
        // the exe's per-stage sampler defaults (stage 0 went through RwRenderStateSet above)
        d3d::setSamplerState(st, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        d3d::setSamplerState(st, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        d3d::setSamplerState(st, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        d3d::setSamplerState(st, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
        d3d::setSamplerState(st, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
        d3d::setSamplerState(st, D3DSAMP_BORDERCOLOR, 0xFF000000u);
        d3d::setSamplerState(st, D3DSAMP_MAXANISOTROPY, 1);
        RwD3D9SetTextureStageState(st, D3DTSS_COLOROP, D3DTOP_DISABLE);
        RwD3D9SetTextureStageState(st, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }
}
#endif // NOTSA_RW_LIBRW
