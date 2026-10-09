// P2B-04b: RpMaterial* and RpMaterialList (_rpMaterialList*) on top of librw (rw::Material / rw::MaterialList).
//   RpMaterial{Create,Destroy,SetTexture} (+ RpMaterial{Clone,StreamRead,StreamWrite,StreamGetSize}); _rpMaterialList{AppendMaterial,
//   Deinitialize,FindMaterialIndex}. RpMaterial{Get,Set}{Color,Texture,SurfaceProperties} and RpMaterialAddRef are accessors / inlines in
//   fakerw (rwaccessors.h / rwextra.h).
//   NOT here (batch 04c, one file for all plugin-registration thunks): RpMaterialRegisterPlugin / RpMaterialRegisterPluginStream.
//   NOT here (batch 05b): RpMaterialUVAnim*.
// Verified against the exe: RpMaterialDestroy 0x74DA20, RpMaterialSetTexture 0x74DBC0, _rpMaterialListDeinitialize 0x74E150,
// _rpMaterialListFindMaterialIndex 0x74E420, _rpMaterialListAppendMaterial 0x74E350.
// Struct notes: rw::Material {texture, color, surfaceProps{ambient,specular,diffuse}, pipeline(Pipeline*), refCount(int32)}; RW's refCount is
// an int16 at +0x18 (the game does `material->refCount++`, which works on both).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

// D: white colour, surface properties {1,1,1}, no texture, refCount 1.
RpMaterial* RpMaterialCreate() {
    return rw::Material::create();
}

// D: RW frees at refCount == 1 (the texture reference is released); returns TRUE even when only the count was decremented.
RwBool RpMaterialDestroy(RpMaterial* material) {
    if (!material) {
        return FALSE;
    }
    material->destroy();
    return TRUE;
}

// W: RW takes the reference on the new texture BEFORE releasing the old one (0x74DBC0), so re-setting the texture the material already holds
// (refCount 1) is safe; librw's Material::setTexture releases first and would free the texture it is about to reference.
RpMaterial* RpMaterialSetTexture(RpMaterial* material, RwTexture* texture) {
    if (!material) {
        return nullptr;
    }
    if (texture) {
        texture->refCount++;
    }
    if (material->texture) {
        material->texture->destroy();
    }
    material->texture = texture;
    return material;
}

// ---- material list ----

// D: grows by 20 entries (RW and librw agree), takes a reference on the material, returns its index (-1 on allocation failure).
// NOTE: VehicleModelInfo.cpp hands in a `new RpMaterial*[20]` array; librw reallocs it through the engine heap (= CRT heap in the shim).
RwInt32 _rpMaterialListAppendMaterial(RpMaterialList* matList, RpMaterial* material) {
    return matList->appendMaterial(material);
}

// W: RW destroys every material (releasing the list's references), frees the array and resets BOTH the pointer and the count; librw's deinit
// leaves numMaterials / space behind. Returns NULL like the exe (it returns the zeroed count in eax).
RpMaterialList* _rpMaterialListDeinitialize(RpMaterialList* matList) {
    if (!matList) {
        return nullptr;
    }
    matList->deinit();
    matList->materials    = nullptr;
    matList->numMaterials = 0;
    matList->space        = 0;
    return nullptr;
}

// W: the exe searches BACKWARDS from the last entry (a material listed twice yields its last index); librw's findIndex goes forward.
RwInt32 _rpMaterialListFindMaterialIndex(const RpMaterialList* matList, const RpMaterial* material) {
    for (RwInt32 i = matList->numMaterials; i > 0; --i) {
        if (matList->materials[i - 1] == material) {
            return i - 1;
        }
    }
    return -1;
}

// ---- 04ab extras (declared in rwextra.h) ----

RpMaterial* RpMaterialClone(RpMaterial* material) {
    return material ? material->clone() : nullptr;
}

RpMaterial* RpMaterialStreamRead(RwStream* stream) {
    return rw::Material::streamRead(stream);
}

const RpMaterial* RpMaterialStreamWrite(const RpMaterial* material, RwStream* stream) {
    return const_cast<RpMaterial*>(material)->streamWrite(stream) ? material : nullptr;
}

RwUInt32 RpMaterialStreamGetSize(const RpMaterial* material) {
    return const_cast<RpMaterial*>(material)->streamGetSize();
}
#endif
