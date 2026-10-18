// (g) VT-OVERRIDES: reference / anim-callback bookkeeping of task constructors and destructors, and CCopPed::RemoveCriminalToKill, against the exe's machine code.
// The class of bug: a port destructor that forgets `CEntity::CleanUpOldReference(&m_pEntity)` / `anim->SetFinishCallback(DefaultAnimCB)` (or a constructor that forgets RegisterReference, or calls the wrong one)
// leaves a reference entry behind that later NULLS a field of freed memory, and an anim whose callback points at a freed task. Fixture: the entity-reference world of logic_oracle_refs.inc (fake pools, the
// 3000-node CReference pool) plus one fake CAnimBlendAssociation and one task buffer; each case: setup (valid pointers, references registered by the port) -> snapshot -> port -> snapshot -> restore -> exe -> snapshot ->
// compare (every reference node, the free list, all entity storage, the holders, the task buffer except its vptr, the anim buffer). Destructors: port `obj->T::~T()` vs the exe's destructor body.
#include "Tasks/TaskTypes/TaskSimpleChoking.h"
#include "Tasks/TaskTypes/TaskSimpleStealthKill.h"
#include "Tasks/TaskTypes/TaskSimpleCarFallOut.h"
#include "Tasks/TaskTypes/TaskSimpleCarGetIn.h"
#include "Tasks/TaskTypes/TaskSimpleCarJumpOut.h"
#include "Tasks/TaskTypes/TaskSimpleCarCloseDoorFromInside.h"
#include "Tasks/TaskTypes/TaskSimpleCarCloseDoorFromOutside.h"
#include "Tasks/TaskTypes/TaskComplexEvasiveDiveAndGetUp.h"
#include "Animation/AnimBlendAssociation.h"
#include "Entity/Ped/CopPed.h"

namespace tk {
alignas(16) static uint8 g_buf[0x100];
alignas(16) static uint8 g_anim[0x80];
static bool inited = false;
static std::vector<Snap> snaps;   // [0] task buffer, [1] anim buffer (the reference world's snaps are rf::snaps)

static void Init() {
    if (inited) return;
    inited = true;
    snaps.emplace_back((uintptr_t)g_buf, sizeof g_buf);
    snaps.emplace_back((uintptr_t)g_anim, sizeof g_anim);
}

struct World { rf::Blobs w; std::vector<std::vector<uint8>> x; };
static void Save(World& o) { rf::SaveAll(o.w); o.x.resize(snaps.size()); for (size_t i = 0; i < snaps.size(); ++i) { snaps[i].Save(); o.x[i] = snaps[i].b; } }
static void Restore(const World& o) { rf::RestoreAll(o.w); for (size_t i = 0; i < snaps.size(); ++i) { snaps[i].b = o.x[i]; snaps[i].Restore(); } }
// the port's default anim callback lives at its own address; the exe's is 0x4D1390 (anim +0x34 = m_pCallbackFunc)
static std::vector<uint8> NormAnim(std::vector<uint8> v) {
    uint32 cb; std::memcpy(&cb, v.data() + 0x34, 4);
    if (cb == (uint32)(uintptr_t)&CDefaultAnimCallback::DefaultAnimCB) { cb = 0x4D1390; std::memcpy(v.data() + 0x34, &cb, 4); }
    return v;
}
static bool Same(const World& a, const World& b, std::string& d) {
    bool ok = rf::SameAll(a.w, b.w, d);
    // the task buffer: everything but the vptr (dword 0: the port's vtable differs from the exe's); the anim buffer completely
    if (!SameBlob(a.x[0].data() + 4, b.x[0].data() + 4, a.x[0].size() - 4)) { ok = false; d += " task" + DiffDesc(a.x[0].data(), b.x[0].data(), a.x[0].size(), "t"); }
    const auto na = NormAnim(a.x[1]), nb = NormAnim(b.x[1]);
    if (!SameBlob(na.data(), nb.data(), na.size())) { ok = false; d += " anim" + DiffDesc(na.data(), nb.data(), na.size(), "a"); }
    return ok;
}

// a valid entity pointer (ped or vehicle, sometimes null) stored at `off` of the task buffer; registered through the port's RegisterReference (the only way the game stores them)
static void PutEntity(Rng& r, size_t off, bool vehicle, bool alwaysRegister = true) {
    CEntity* e = r.below(8) == 0 ? nullptr : (vehicle ? rf::VehE((int)r.below(rf::kV)) : rf::PedE((int)r.below(rf::kP)));
    std::memcpy(g_buf + off, &e, 4);
    if (e && (alwaysRegister || r.below(5))) { e->RegisterReference(reinterpret_cast<CEntity**>(g_buf + off)); }
}
static void PutAnim(Rng& r, size_t off) {
    CAnimBlendAssociation* a = r.below(4) == 0 ? nullptr : reinterpret_cast<CAnimBlendAssociation*>(g_anim);
    std::memcpy(g_buf + off, &a, 4);
}
static void FreshBuffers(Rng& r) {
    for (size_t i = 0; i + 4 <= sizeof g_buf; i += 4) { const uint32 v = r.u32(); std::memcpy(g_buf + i, &v, 4); }
    for (size_t i = 0; i + 4 <= sizeof g_anim; i += 4) { const uint32 v = r.u32(); std::memcpy(g_anim + i, &v, 4); }
    float bd = r.f01() * 8.f - 4.f; std::memcpy(g_anim + 0x1C, &bd, 4);          // m_BlendDelta
}

template<class Setup, class Port, class Exe>
static bool Case(Rng& r, std::string& d, Setup&& setup, Port&& port, Exe&& exe) {
    rf::Enter();
    rf::Reset(r, (int)r.below(2));
    FreshBuffers(r);
    setup();
    static World w0, w1, w2;
    Save(w0);
    port(); Save(w1);
    Restore(w0);
    exe(); Save(w2);
    rf::Leave();
    d.clear();
    return Same(w1, w2, d);
}

template<class T> static void PortDtor() { reinterpret_cast<T*>(g_buf)->T::~T(); }
static void ExeDtor(uint32 a) { oracle::Fn<void __fastcall(void*, int)>(a)(g_buf, 0); }
}   // namespace tk

static void TestTaskRefs() {
    using namespace tk;
    Init();
    rf::Init();
    // layout pins the rows rely on (the exe's offsets)
    static_assert(offsetof(CCopPed, m_apCriminalsToKill) == 0x7AC);

    const auto DtorRow = [&](const char* name, uint32 dtor, size_t refOff, bool refIsVehicle, int animOff, void (*portDtor)(), bool complex = false) {
        Run(name, [=](Rng& r, std::string& d) {
            // CTaskComplex-derived: the base destructor deletes the sub task (+8) - none here
            return Case(r, d, [&] { if (complex) { const uint32 z = 0; std::memcpy(g_buf + 8, &z, 4); } PutEntity(r, refOff, refIsVehicle, r.below(6) != 0); if (animOff >= 0) { PutAnim(r, (size_t)animOff); } }, portDtor, [&] { ExeDtor(dtor); });
        }, 400);
    };
    DtorRow("CTaskSimpleChoking::~CTaskSimpleChoking 0x620370 (reference + finish callback)",            0x620370, 0x08, false, 0x0C, &PortDtor<CTaskSimpleChoking>);
    DtorRow("CTaskSimpleStealthKill::~CTaskSimpleStealthKill 0x622680 (reference + delete callback)",    0x622680, 0x0C, false, 0x18, &PortDtor<CTaskSimpleStealthKill>);
    DtorRow("CTaskSimpleCarFallOut::~CTaskSimpleCarFallOut 0x648DE0 (reference)",                        0x648DE0, 0x10, true,  0x0C, &PortDtor<CTaskSimpleCarFallOut>);
    DtorRow("CTaskSimpleCarGetIn::~CTaskSimpleCarGetIn 0x646710 (reference + finish callback)",           0x646710, 0x18, true,  0x0C, &PortDtor<CTaskSimpleCarGetIn>);
    DtorRow("CTaskSimpleCarCloseDoorFromInside::~ 0x646380 (reference + finish callback)",                 0x646380, 0x10, true,  0x0C, &PortDtor<CTaskSimpleCarCloseDoorFromInside>);
    DtorRow("CTaskSimpleCarCloseDoorFromOutside::~ 0x646570 (reference + finish callback)",                0x646570, 0x10, true,  0x0C, &PortDtor<CTaskSimpleCarCloseDoorFromOutside>);
    DtorRow("CTaskSimpleCarJumpOut::~CTaskSimpleCarJumpOut 0x647490 (reference + finish callback)",       0x647490, 0x10, true,  0x0C, &PortDtor<CTaskSimpleCarJumpOut>);
    DtorRow("CTaskComplexEvasiveDiveAndGetUp::~ 0x653740 (reference)",                                     0x653740, 0x0C, true,  -1,   &PortDtor<CTaskComplexEvasiveDiveAndGetUp>, true);

    // constructors: the registered references (and every other store) of the task
    const auto Veh = [&](Rng& r) -> CVehicle* { return r.below(8) == 0 ? nullptr : reinterpret_cast<CVehicle*>(rf::VehE((int)r.below(rf::kV))); };
    const auto Ped = [&](Rng& r) -> CPed* { return r.below(8) == 0 ? nullptr : reinterpret_cast<CPed*>(rf::PedE((int)r.below(rf::kP))); };
    const auto LineUp = [&](Rng& r) { return reinterpret_cast<CTaskUtilityLineUpPedWithCar*>((uintptr_t)(r.below(3) ? 0x12345670u + 16 * r.below(16) : 0u)); };
    Run("CTaskComplexEvasiveDiveAndGetUp::CTaskComplexEvasiveDiveAndGetUp 0x6536B0 (RegisterReference)", [&](Rng& r, std::string& d) {
        CVehicle* v = Veh(r); const int t = (int)r.below(5000); CVector dir{ r.f01() - .5f, r.f01() - .5f, r.f01() }; const bool b = r.below(2);
        return Case(r, d, [] {}, [&] { ::new (static_cast<void*>(g_buf)) CTaskComplexEvasiveDiveAndGetUp(v, t, dir, b); }, [&] { oracle::Fn<void* __fastcall(void*, int, CVehicle*, int, const CVector*, int)>(0x6536B0)(g_buf, 0, v, t, &dir, (int)b); });
    }, 300);
    Run("CTaskSimpleCarFallOut::CTaskSimpleCarFallOut 0x648D60 (RegisterReference)", [&](Rng& r, std::string& d) {
        CVehicle* v = Veh(r); const int door = (int)r.below(6); auto* lu = LineUp(r);
        return Case(r, d, [] {}, [&] { ::new (static_cast<void*>(g_buf)) CTaskSimpleCarFallOut(v, (eTargetDoor)door, lu); }, [&] { oracle::Fn<void* __fastcall(void*, int, CVehicle*, int, void*)>(0x648D60)(g_buf, 0, v, door, lu); });
    }, 300);
    Run("CTaskSimpleCarJumpOut::CTaskSimpleCarJumpOut 0x64BE40 (RegisterReference + initial speed)", [&](Rng& r, std::string& d) {
        CVehicle* v = Veh(r); const uint32 door = r.below(6); auto* lu = LineUp(r);
        return Case(r, d, [] {}, [&] { ::new (static_cast<void*>(g_buf)) CTaskSimpleCarJumpOut(v, door, lu); }, [&] { oracle::Fn<void* __fastcall(void*, int, CVehicle*, uint32, void*)>(0x64BE40)(g_buf, 0, v, door, lu); });
    }, 300);
    Run("CTaskSimpleCarGetIn::CTaskSimpleCarGetIn 0x646690 (RegisterReference)", [&](Rng& r, std::string& d) {
        CVehicle* v = Veh(r); const uint32 door = r.below(6); auto* lu = LineUp(r);
        return Case(r, d, [] { std::memset(g_buf + 0x10, 0, 8); /* m_unk / m_unk2: never written by the exe's constructor, zero-initialised in the port */ }, [&] { ::new (static_cast<void*>(g_buf)) CTaskSimpleCarGetIn(v, door, lu); }, [&] { oracle::Fn<void* __fastcall(void*, int, CVehicle*, uint32, void*)>(0x646690)(g_buf, 0, v, door, lu); });
    }, 300);
    Run("CTaskSimpleCarCloseDoorFromInside::CTaskSimpleCarCloseDoorFromInside 0x646300 (RegisterReference)", [&](Rng& r, std::string& d) {
        CVehicle* v = Veh(r); const uint32 door = r.below(6); auto* lu = LineUp(r);
        return Case(r, d, [] {}, [&] { ::new (static_cast<void*>(g_buf)) CTaskSimpleCarCloseDoorFromInside(v, door, lu); }, [&] { oracle::Fn<void* __fastcall(void*, int, CVehicle*, uint32, void*)>(0x646300)(g_buf, 0, v, door, lu); });
    }, 300);
    Run("CTaskSimpleCarCloseDoorFromOutside::CTaskSimpleCarCloseDoorFromOutside 0x6464F0 (RegisterReference)", [&](Rng& r, std::string& d) {
        CVehicle* v = Veh(r); const uint32 door = r.below(6); auto* lu = LineUp(r);
        return Case(r, d, [] {}, [&] { ::new (static_cast<void*>(g_buf)) CTaskSimpleCarCloseDoorFromOutside(v, door, lu); }, [&] { oracle::Fn<void* __fastcall(void*, int, CVehicle*, uint32, void*)>(0x6464F0)(g_buf, 0, v, door, lu); });
    }, 300);
    Run("CTaskSimpleStealthKill::CTaskSimpleStealthKill 0x6225F0 (RegisterReference)", [&](Rng& r, std::string& d) {
        CPed* p = Ped(r); const bool keep = r.below(2); const int grp = (int)r.below(40);
        return Case(r, d, [] {}, [&] { ::new (static_cast<void*>(g_buf)) CTaskSimpleStealthKill(keep, p, (AssocGroupId)grp); }, [&] { oracle::Fn<void* __fastcall(void*, int, int, CPed*, int)>(0x6225F0)(g_buf, 0, (int)keep, p, grp); });
    }, 300);

    // CCopPed::RemoveCriminalToKill 0x5DE040 (the port REGISTERED the slot instead of cleaning it up): the cop is a fake ped of the reference world
    Run("CCopPed::RemoveCriminalToKill 0x5DE040", [&](Rng& r, std::string& d) {
        rf::Enter();
        rf::Reset(r, (int)r.below(2));
        auto* cop = reinterpret_cast<CCopPed*>(rf::PedE((int)r.below(rf::kP)));
        const int idx = (int)r.below(6);
        for (int i = 0; i < 6; ++i) {
            CEntity* c = r.below(3) ? rf::PedE((int)r.below(rf::kP)) : nullptr;
            cop->m_apCriminalsToKill[i] = reinterpret_cast<CPed*>(c);
            if (c && r.below(6)) { c->RegisterReference(reinterpret_cast<CEntity**>(&cop->m_apCriminalsToKill[i])); }
        }
        static rf::Blobs w0, w1, w2;
        rf::SaveAll(w0);
        cop->RemoveCriminalToKill(0, idx); rf::SaveAll(w1);
        rf::RestoreAll(w0);
        oracle::Fn<void __fastcall(CCopPed*, int, int, int)>(0x5DE040)(cop, 0, 0, idx); rf::SaveAll(w2);
        rf::Leave();
        d.clear(); return rf::SameAll(w1, w2, d);
    }, 400);
}
