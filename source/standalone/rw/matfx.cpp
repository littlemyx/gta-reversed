// P2B-05b: RpMatFX* on top of librw (rw::MatFX material / atomic plugin).
//   RpMatFXPluginAttach, RpMatFXAtomicQueryEffects, RpMatFXMaterial{Get,Set}Effects, RpMatFXMaterialGetEnvMapTexture,
//   RpMatFXMaterialSetEnvMap{Coefficient,Frame}, MatFXD3D9EnvMapGetData (the MATFXD3D9ENVMAPGETDATA macro of the game's building pipelines).
// Verified against the exe: SetEffects 0x811C80, SetEnvMapFrame 0x8125B0, SetEnvMapCoefficient 0x812680, GetEnvMapTexture 0x8126F0,
// SetUVTransformMatrices 0x8129E0; the material's plugin data is {union data (0x14 bytes), RwUInt32 type} x 2 passes + an effect type (+0x30).
// Semantics shared with librw's rw::MatFX: SetEffects allocates the data lazily, clears the old pass data (releasing the env / bump / dual textures)
// only when the effect type changes (and always for NULL), BUMPENVMAP = {bump, env}, DUALUVTRANSFORM = {uvtransform, dual}; the env-map setters
// operate on the pass whose type is ENVMAP (the exe walks both passes), SetEnvMapFrame stores the frame without a reference.
// Deviation: the exe dereferences NULL when the env-map setters / getter are called on a material without an env pass (crash); here they are no-ops
// / return NULL.
// MatFXEnvMapData (rwextra.h, layout static_asserted against rw::MatFX::Env) is returned by pointer into the librw plugin data.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

namespace {
bool Registered() {
    return rw::Material::s_plglist.getPluginOffset(rw::ID_MATFX) >= 0;
}
} // namespace

// W: registers librw's MatFX plugin (atomic flag, material data, stream callbacks, D3D9 driver pipeline plugin) once per Engine::init cycle; the
// game calls it between RwEngineInit and RwEngineOpen like for skin / hanim.
RwBool RpMatFXPluginAttach() {
    if (!Registered()) {
        rw::registerMatFXPlugin();
    }
    return Registered() ? TRUE : FALSE;
}

// D: the atomic's "effects enabled" flag (the plugin slot; set by the DFF atomic chunk or RpMatFXAtomicEnableEffects).
RwBool RpMatFXAtomicQueryEffects(RpAtomic* atomic) {
    if (!Registered() || !atomic) {
        return FALSE;
    }
    return rw::MatFX::getEffects(atomic) ? TRUE : FALSE;
}

// D: rpMATFXEFFECTNULL (0) when the material has no MatFX data.
RpMatFXMaterialFlags RpMatFXMaterialGetEffects(const RpMaterial* material) {
    if (!Registered() || !material) {
        return rpMATFXEFFECTNULL;
    }
    return static_cast<RpMatFXMaterialFlags>(rw::MatFX::getEffects(material));
}

// D: allocates the plugin data on first use (zeroed), clears the old pass data when the type changes (and for NULL), sets the two pass types.
RpMaterial* RpMatFXMaterialSetEffects(RpMaterial* material, RpMatFXMaterialFlags flags) {
    if (!Registered() || !material) {
        return nullptr;
    }
    rw::MatFX::setEffects(material, static_cast<rw::uint32>(flags));
    return material;
}

// D: first pass of type ENVMAP (the exe returns NULL-deref garbage when there is none).
RwTexture* RpMatFXMaterialGetEnvMapTexture(const RpMaterial* material) {
    if (!Registered() || !material) {
        return nullptr;
    }
    rw::MatFX* fx = rw::MatFX::get(material);
    return fx ? fx->getEnvTexture() : nullptr;
}

// A: 0x812680: the coefficient of the ENVMAP pass.
RpMaterial* RpMatFXMaterialSetEnvMapCoefficient(RpMaterial* material, RwReal coef) {
    if (Registered() && material) {
        if (rw::MatFX* fx = rw::MatFX::get(material)) {
            fx->setEnvCoefficient(coef);
        }
    }
    return material;
}

// A: 0x8125B0: stores the frame pointer (no reference) in the ENVMAP pass.
RpMaterial* RpMatFXMaterialSetEnvMapFrame(RpMaterial* material, RwFrame* frame) {
    if (Registered() && material) {
        if (rw::MatFX* fx = rw::MatFX::get(material)) {
            fx->setEnvFrame(frame);
        }
    }
    return material;
}

// A: MATFXD3D9ENVMAPGETDATA(material, pass): pass data (not "the ENVMAP pass") of RW's private rpMatFXMaterialData; the building pipelines call it only
// after RpMatFXMaterialGetEffects == rpMATFXEFFECTENVMAP, i.e. pass 0 = the env data.
MatFXEnvMapData* MatFXD3D9EnvMapGetData(RpMaterial* material, RwInt32 pass) {
    if (!Registered() || !material || pass < 0 || pass >= rpMAXPASS) {
        return nullptr;
    }
    rw::MatFX* fx = rw::MatFX::get(material);
    return fx ? reinterpret_cast<MatFXEnvMapData*>(&fx->fx[pass].env) : nullptr;
}
#endif
