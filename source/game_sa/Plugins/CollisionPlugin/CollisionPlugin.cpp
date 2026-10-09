#include "StdInc.h"

#include "CollisionPlugin.h"

auto& gCollisionPluginOffset = StaticRef<RwInt32>(0x9689DC);

static RwStream* ClumpCollisionStreamRead(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);

void CCollisionPlugin::InjectHooks() {
    RH_ScopedClass(CCollisionPlugin);
    RH_ScopedCategory("Plugins");

    RH_ScopedInstall(PluginAttach, 0x41B310);
    RH_ScopedInstall(SetModelInfo, 0x41B350);
    RH_ScopedGlobalInstall(ClumpCollisionStreamRead, 0x41B1D0);
}

// internal
// 0x41B1A0
static void* ClumpCollisionConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return object;
}

// internal
// 0x41B1C0
static void* ClumpCollisionDestructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return object;
}

// internal
// 0x41B1B0
static void* ClumpCollisionCopyConstructor(void* dstObject, const void* srcObject, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return dstObject;
}

// internal
// 0x41B1D0
static RwStream* ClumpCollisionStreamRead(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    CMemoryMgr::LockScratchPad(); // 0x72F4A0
    RwStreamRead(stream, &PC_Scratch, binaryLength); // 0x7EC9D0
    auto* const model = new CColModel(); // 0x40FC30, 0x40FB60

    uint8* const header = reinterpret_cast<uint8*>(&PC_Scratch[0]);
    uint8* const data   = reinterpret_cast<uint8*>(&PC_Scratch[0x20]);
    switch (*reinterpret_cast<uint32*>(header)) {
    case MakeFourCC("COLL"):
        CFileLoader::LoadCollisionModel(data, *model); // 0x537580
        break;
    case MakeFourCC("COL2"):
        CFileLoader::LoadCollisionModelVer2(data, *reinterpret_cast<uint32*>(&header[4]) - 0x18, *model, nullptr); // 0x537EE0
        break;
    case MakeFourCC("COL3"):
        CFileLoader::LoadCollisionModelVer3(data, *reinterpret_cast<uint32*>(&header[4]) - 0x18, *model, nullptr); // 0x537CE0
        break;
    default: // NOTE: Unknown signature, the original loads it as `COLL` but from the start of the buffer (not from the data after the header)
        CFileLoader::LoadCollisionModel(header, *model);
        break;
    }

    model->MakeMultipleAlloc();
    CCollisionPlugin::ms_currentModel->SetColModel(model, true); // 0x4C4BC0
    CCollisionPlugin::ms_currentModel->bOwnsCollisionModel = true;
    CMemoryMgr::ReleaseScratchPad(); // 0x72F4B0
    return stream;
}

// internal
// 0x41B2F0
static RwStream* ClumpCollisionStreamWrite(RwStream* stream, RwInt32 binaryLength, const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return stream;
}

// internal
// 0x41B300
static RwInt32 ClumpCollisionGetSize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return -1;
}

// 0x41B310
bool CCollisionPlugin::PluginAttach() {
    // 0x9689DC unused
    gCollisionPluginOffset = RpClumpRegisterPlugin(
        0,
        rwID_COLLISIONPLUGIN,
        ClumpCollisionConstructor,
        ClumpCollisionDestructor,
        ClumpCollisionCopyConstructor
    );

    RpClumpRegisterPluginStream(
        rwID_COLLISIONPLUGIN,
        ClumpCollisionStreamRead,
        ClumpCollisionStreamWrite,
        ClumpCollisionGetSize
    );

    return TRUE;
}

// 0x41B350
void CCollisionPlugin::SetModelInfo(CClumpModelInfo* modelInfo) {
    ms_currentModel = modelInfo;
}
