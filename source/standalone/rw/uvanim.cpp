// P2B-05b: RpUVAnim on top of librw's uvanim.cpp (rw::UVAnim material plugin, rw::UVAnimDictionary) and the RtDict entry points the game uses for the
// UV-animation dictionary (Streaming.cpp:470-483: RtDictSchemaStreamReadDict / SetCurrentDict / RtDictDestroy with RpUVAnimDictSchema).
//   RpUVAnimPluginAttach, RpMaterialUVAnim{Exists,AddAnimTime,ApplyUpdate}, RtDictSchemaStreamReadDict, RtDictSchemaSetCurrentDict, RtDictDestroy.
// Verified against the exe: Exists 0x7CC530, AddAnimTime 0x7CC4B0 (-> RtAnimInterpolatorAddAnimTime 0x7CD8D0), ApplyUpdate 0x7CC110 (reset both
// uv matrices to identity, for every used interpolator and node: apply callback -> RwMatrixTransform(uv[channel], PRECONCAT), channel < 2 unsigned;
// then RpMatFXMaterialSetUVTransformMatrices 0x8129E0), linear / param interpolation 0x7CCA40 / 0x7CC600, apply 0x7CC9F0 / 0x7CC560,
// RtDictSchemaStreamReadDict 0x7CF240 (FindChunk STRUCT, count, entries), RtDictDestroy 0x7CF130 (clears schema->currentDict if it is the dict,
// unlinks, destroys every entry through the entry destroy callback = RpUVAnimDestroy: refcount--, free at 0).
// Review (exe, independent walk): the material stream read 0x7CBC20 looks names up case-sensitively (strcmp, RtDictFindNamedEntry 0x7CEFE0), a missing name gets a
// private identity stand-in 0x7CBE00 (reference count 1, not added to the dictionary), the uv matrices are created only for channels some node maps to (0x7CBD80);
// ApplyUpdate steps to the next interpolated frame only after an applied node; float order of the interpolation callbacks (0x7CCA40 / 0x7CC600) is plain float
// arithmetic (the game runs the x87 at 24 bit after D3D9 device creation, see .notes/PHASE2_PLAN.md D7).
// Not ported by the exe-faithful path: the exe's material copy constructor (0x7CBB30) shares the uv matrices / interpolators with the source (double free on destroy);
// librw deep-copies them instead. The anim / loop callbacks of AddAnimTime (0x7CD8D0) are never set by the game.
// Cross-domain: the param-scheme rotation uses rw::Matrix::rotate (exe RwMatrixRotate 0x7F1FD0 converts degrees*(pi/180f) and uses x87 fsin/fcos).
// librw's uvanim.cpp was "TODO fully"; the NOTSA fork of vendor/librw (branch `notsa`) fixes what differed from the exe: exact param interpolation
// (single +-2pi correction), AddAnimTime keeps the overshoot when an animation wraps, cloned materials get their own uv matrices (and they are freed with
// the plugin), unsigned channel check. Not implemented in librw (nil callbacks, never called by the game): key-frame blend / add / mul-recip.
// RpUVAnimDictSchema is one global schema: the shim keeps the "current dictionary" in librw's `currentUVAnimDictionary` (the schema pointer is ignored).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

namespace {
bool Registered() {
    return rw::Material::s_plglist.getPluginOffset(rw::ID_UVANIMATION) >= 0;
}
} // namespace

// W: registers librw's UVAnim plugin (material slot with 8 interpolators + 2 uv matrices, streams) and, at Engine::open, its two interpolator schemes
// (ids 0x1C0 linear / 0x1C1 param). The game attaches MatFX first (the uv matrices are handed to MatFX at ApplyUpdate).
RwBool RpUVAnimPluginAttach() {
    if (!Registered()) {
        rw::registerUVAnimPlugin();
    }
    return Registered() ? TRUE : FALSE;
}

// W: any of the 8 slots has an interpolator.
RwBool RpMaterialUVAnimExists(const RpMaterial* material) {
    if (!Registered() || !material) {
        return FALSE;
    }
    return rw::UVAnim::exists(const_cast<RpMaterial*>(material)) ? TRUE : FALSE;
}

// W: adds the time to every interpolator (wraps keeping the overshoot, see the file comment).
RpMaterial* RpMaterialUVAnimAddAnimTime(RpMaterial* material, RwReal deltaTime) {
    if (Registered() && material) {
        rw::UVAnim::addTime(material, deltaTime);
    }
    return material;
}

// W: rebuilds the base / dual uv matrices from the interpolated frames and hands them to the material's UVTRANSFORM pass.
RpMaterial* RpMaterialUVAnimApplyUpdate(RpMaterial* material) {
    if (Registered() && material && rw::Material::s_plglist.getPluginOffset(rw::ID_MATFX) >= 0) {
        rw::UVAnim::applyUpdate(material);
    }
    return material;
}

// A: UVAnimDictionary::streamRead; the chunk header (0x2B) has already been consumed by the caller.
RtDict* RtDictSchemaStreamReadDict(RtDictSchema* schema, RwStream* stream) {
    (void)schema;
    return rw::UVAnimDictionary::streamRead(stream);
}

// A: sets the dictionary the material stream reader looks the animations up in (NULL = none).
RtDictSchema* RtDictSchemaSetCurrentDict(RtDictSchema* schema, RtDict* dict) {
    rw::currentUVAnimDictionary = dict;
    return schema;
}

// A: 0x7CF130: forgets the dictionary as the current one and releases its entries (animations still referenced by materials survive).
RwBool RtDictDestroy(RtDict* dictionary) {
    if (!dictionary) {
        return FALSE;
    }
    if (rw::currentUVAnimDictionary == dictionary) {
        rw::currentUVAnimDictionary = nullptr;
    }
    dictionary->destroy();
    return TRUE;
}
#endif
