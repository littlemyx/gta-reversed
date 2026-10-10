// P2B-04a: RpLight* on top of librw (rw::Light).
//   RpLight{Create,Destroy,SetColor,SetRadius} (the ones the game calls) + RpLight{SetConeAngle,GetConeAngle,StreamRead,StreamGetSize}.
//   RpLightGet{Frame,Flags,Type,Color,Radius}/SetFlags/SetFrame are macros in fakerw/rwaccessors.h.
// Function classes (see .notes/P2B_SHIM_PLAN.md): D = direct, A = adapter, W = written here because the exe/librw semantics differ.
// Verified against the exe: RpLightCreate 0x752110, RpLightSetColor 0x751A90, RpLightSetRadius 0x751A70, RpLightDestroy 0x7520D0.
// NOT here: the exe's "ambient saturated" global (AmbientSaturated) is NOT touched by RpLightSetColor/RpWorldAddLight in RW 3.6 (they only
// store the colour / link the light): RW's D3D9 light setup refreshes it while it enumerates the lights of an atomic (render time), so it
// belongs to the lighting setup of the render path (P2B-02c/07/08c), not to this file.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include "rwtrig.h"
#include <cmath>

// D + W: Light::create, then the exe's defaults that librw differs in: RpLightCreate leaves minusCosAngle = 0 (librw: 1) and the colour at
// white/alpha 1; privateFlags = 1 ("grey colour"), flags = rpLIGHTLIGHTATOMICS | rpLIGHTLIGHTWORLD (same in librw).
RpLight* RpLightCreate(RwInt32 type) {
    RpLight* light = rw::Light::create(type);
    if (light) {
        light->minusCosAngle = 0.0f;
    }
    return light;
}

// A: RW does not touch the world on destroy (the game removes the light first); librw asserts that it is detached, so a light still tied
// to a world is unlinked here instead of aborting. A light inside a clump cannot be detached from here (no RpClumpRemoveLight on librw):
// that is a caller bug in RW as well.
RwBool RpLightDestroy(RpLight* light) {
    if (!light) {
        return FALSE;
    }
    if (light->world) {
        light->world->removeLight(light);
    }
    light->destroy();
    return TRUE;
}

// D: copies all four components (alpha too, as the exe) and recomputes the "grey" private flag the way RW does (r == g && r == b).
RpLight* RpLightSetColor(RpLight* light, const RwRGBAReal* color) {
    if (!light || !color) {
        return nullptr;
    }
    light->color                      = *color;
    light->object.object.privateFlags = (color->red == color->green && color->red == color->blue) ? 1 : 0;
    return light;
}

// A: `light->radius = r`, then the exe marks the attached frame dirty so the light is re-synced (RwObjectHasFrameSync).
RpLight* RpLightSetRadius(RpLight* light, RwReal radius) {
    if (!light) {
        return nullptr;
    }
    light->radius = radius;
    if (RwFrame* frame = light->getFrame()) {
        frame->updateObjects();
    }
    return light;
}

// ---- 04ab extras (declared in rwextra.h): not called by the game outside the stock RW headers ----

// W: exe 0x751D20 / 0x751AE0 (not called by the game itself; the exe's inline RwACos is the same table-sqrt polynomial as rtquat / hanim).
// Set: rejects angle < 0 and angle > pi/2 (returns NULL; a NaN passes both x87 compares, as in the exe: fcomp unordered sets C0|C2|C3 -> not 'less', not 'greater'), else minusCosAngle = -cos(angle) (x87 fcos, float store), returns the light.
RpLight* RpLightSetConeAngle(RpLight* light, RwReal angle) {
    if (!light || angle < 0.0f || angle > rwtrig::FromBits(0x3fc90fdbu)) {   // 0x858B50 = 0, 0x858FE4 = pi/2
        return nullptr;
    }
    light->minusCosAngle = static_cast<float>(-std::cos(static_cast<double>(angle)));
    return light;
}

// Get: acos(-minusCosAngle) with the exe's inlined RwACos (FreeBSD e_acosf structure, _rwSqrt 0x7EDB30 table) instead of libm acosf.
RwReal RpLightGetConeAngle(const RpLight* light) {
    return rwtrig::ACos(-light->minusCosAngle);
}

RpLight* RpLightStreamRead(RwStream* stream) {
    return rw::Light::streamRead(stream);
}

RwUInt32 RpLightStreamGetSize(const RpLight* light) {
    return const_cast<RpLight*>(light)->streamGetSize();
}
#endif
