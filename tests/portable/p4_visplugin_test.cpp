// P4-S2 host test: the frame-visibility plugin of source/game_sa/VisibilityPlugins.{h,cpp}. The REAL text of tFrameVisibilityPlugin, the FRAMEPLG_* macros and the six functions that touch the
// slot (FrameConstructor / FrameCopyConstructor / Get|SetFrameHierarchyId / Get|SetClumpModelInfo) is extracted by the runner into $OUT/visplugin.inc and compiled against a tiny stub of the engine.
//   tools/portable/run_p4_visplugin_test.sh  (`mutate`: truncating the pointer, not clearing it on SetFrameHierarchyId, copy ctor without the pointer, ... must FAIL)
// Semantics under test (the exe has ONE 4 byte slot): clump ROOT frame = CClumpModelInfo*, any other frame = small component id; readers that test it for 0 / compare it see a non-zero value for a root.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <new>
#include <sys/mman.h>
using int32 = int32_t; using uint32 = uint32_t; using uint8 = uint8_t; using RwInt32 = int32_t;
#define NOTSA_PORTABLE 1
#define VALIDATE_SIZE(struc, size) static_assert(true)
#define NOTSA_PTR_ID32(p) static_cast<uint32>(reinterpret_cast<uintptr_t>(p))
#define RWPLUGINOFFSET(_type, _base, _offset) ((_type*)(((uint8*)(_base)) + (_offset)))
// ---- engine stub
enum { MODEL_INFO_VEHICLE = 5, MODEL_INFO_PED = 7 };
struct CVehicleModelInfo;
struct CClumpModelInfo { int type = MODEL_INFO_PED; int GetModelType() const { return type; } CVehicleModelInfo* AsVehicleModelInfoPtr() { return nullptr; } };
struct CVehicleModelInfo { bool IsTrain() { return false; } int m_nVehicleType; };
enum { VEHICLE_TYPE_FPLANE = 1 };
struct RwFrame { alignas(8) uint8 raw[128]; };
struct RpClump { RwFrame* frame; };
static RwFrame* RpClumpGetFrame(RpClump* c) { return c->frame; }
using ClumpVisibilityCallback = bool(*)(RpClump*);
struct tClumpVisibilityPlugin { ClumpVisibilityCallback m_visibilityCallBack; int32 m_alpha; };
#define CLUMPPLG_VISIBILITYCB(clump) (*(ClumpVisibilityCallback*)nullptr)   /* vehicle path: not exercised */
static bool VehicleVisibilityCB(RpClump*) { return true; }
static bool VehicleVisibilityCB_BigVehicle(RpClump*) { return true; }
struct CVisibilityPlugins {
    static inline RwInt32 ms_framePluginOffset = 32; // like librw: pointer aligned plugin offset
    static void* FrameConstructor(void* frame, RwInt32 offset, RwInt32 size);
    static void* FrameCopyConstructor(void* destFrame, const void* srcFrame, RwInt32 offset, RwInt32 size);
    static int32 GetFrameHierarchyId(RwFrame* frame);
    static void SetFrameHierarchyId(RwFrame* frame, int32 id);
    static void SetClumpModelInfo(RpClump* clump, CClumpModelInfo* clumpModelInfo);
    static CClumpModelInfo* GetClumpModelInfo(RpClump* clump);
};
#include "visplugin.inc"

static int g_fails = 0;
#define CHECK(c, ...) do { if (!(c)) { g_fails++; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

int main() {
    static_assert(sizeof(void*) == 8);
    using P = CVisibilityPlugins;
    RwFrame root, child, copy; std::memset(&root, 0xCD, sizeof root); std::memset(&child, 0xCD, sizeof child); std::memset(&copy, 0xCD, sizeof copy);
    P::FrameConstructor(&root, 0, 0); P::FrameConstructor(&child, 0, 0); P::FrameConstructor(&copy, 0, 0);
    RpClump clump{ &root };
    CHECK(P::GetFrameHierarchyId(&root) == 0 && P::GetClumpModelInfo(&clump) == nullptr, "constructor: id 0, no model info (PlayerPed.cpp:443 tests the result for null)");

    auto* mi = new CClumpModelInfo;
    P::SetClumpModelInfo(&clump, mi);
    CHECK(P::GetClumpModelInfo(&clump) == mi, "full 64-bit pointer round trip: %p vs %p", (void*)P::GetClumpModelInfo(&clump), (void*)mi);
    CHECK(P::GetFrameHierarchyId(&root) != 0, "a root frame with a model info must read as non-zero (ClumpModelInfo.cpp:283 skips it by `!id`, Vehicle.cpp:2394 does not destroy it)");
    CHECK((uint32)P::GetFrameHierarchyId(&root) == NOTSA_PTR_ID32(mi) || NOTSA_PTR_ID32(mi) == 0, "id slot = low 32 bits of the pointer (what the exe's 32 bit pointer is): %x vs %x", (uint32)P::GetFrameHierarchyId(&root), NOTSA_PTR_ID32(mi));
    std::printf("heap model info at %p (%s 4 GiB)\n", (void*)mi, ((uintptr_t)mi >> 32) ? "above" : "below");

    // ---- a model info whose address has all-zero low 32 bits: the id must still read as non-zero
    {
        void* want = nullptr; void* mem = MAP_FAILED;
        for (uint64_t k = 1; k < 0x4000 && mem != want; k++) { // find a free page at an address k * 4 GiB
            if (mem != MAP_FAILED) munmap(mem, 4096);
            want = (void*)(k << 32);
            mem  = mmap(want, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        }
        if (mem == want) {
            auto* lz = new (mem) CClumpModelInfo;
            RwFrame f; P::FrameConstructor(&f, 0, 0); RpClump c{ &f };
            P::SetClumpModelInfo(&c, lz);
            CHECK(NOTSA_PTR_ID32(lz) == 0, "setup");
            CHECK(P::GetClumpModelInfo(&c) == lz, "pointer with zero low half round trip");
            CHECK(P::GetFrameHierarchyId(&f) != 0, "zero-low-half pointer must still read as a non-zero id");
        } else {
            std::printf("NOTE: could not map any k*4GiB page (got %p): zero-low-half case skipped\n", mem);
        }
        if (mem != MAP_FAILED) munmap(mem, 4096);
    }

    // ---- child frame: plain id, no model info
    P::SetFrameHierarchyId(&child, 7);
    CHECK(P::GetFrameHierarchyId(&child) == 7 && RWPLUGINOFFSET(tFrameVisibilityPlugin, &child, P::ms_framePluginOffset)->m_modelInfo == nullptr, "child id");
    P::SetFrameHierarchyId(&child, -3);
    CHECK(P::GetFrameHierarchyId(&child) == -3, "negative ids survive (int32 slot)");

    // ---- one slot: writing an id over a root replaces the stored model info (Vehicle.cpp:2552 SetFrameHierarchyId(frame, 0))
    RpClump c2{ &copy }; P::SetClumpModelInfo(&c2, mi);
    CHECK(P::GetClumpModelInfo(&c2) == mi, "second root");
    P::SetFrameHierarchyId(&copy, 0);
    CHECK(P::GetClumpModelInfo(&c2) == nullptr && P::GetFrameHierarchyId(&copy) == 0, "id write replaces the model info, like the single exe slot");

    // ---- copy constructor (RwFrameClone) copies the whole slot
    RwFrame dst; std::memset(&dst, 0xCD, sizeof dst); P::FrameConstructor(&dst, 0, 0);
    P::FrameCopyConstructor(&dst, &root, 0, 0);
    RpClump c3{ &dst };
    CHECK(P::GetClumpModelInfo(&c3) == mi && P::GetFrameHierarchyId(&dst) == P::GetFrameHierarchyId(&root), "copy ctor copies id and pointer");
    RwFrame dst2; P::FrameConstructor(&dst2, 0, 0); P::FrameCopyConstructor(&dst2, &child, 0, 0);
    CHECK(P::GetFrameHierarchyId(&dst2) == -3 && RWPLUGINOFFSET(tFrameVisibilityPlugin, &dst2, P::ms_framePluginOffset)->m_modelInfo == nullptr, "copy of a child");

    // ---- alignment of the pointer member inside the frame buffer (librw rounds plugin offsets to sizeof(void*))
    CHECK(((uintptr_t)RWPLUGINOFFSET(tFrameVisibilityPlugin, &root, P::ms_framePluginOffset) % alignof(tFrameVisibilityPlugin)) == 0, "plugin slot aligned");
    delete mi;
    std::printf("p4 visibility-plugin test: %s (fails=%d)\n", g_fails ? "FAILED" : "ALL OK", g_fails);
    return g_fails != 0;
}
