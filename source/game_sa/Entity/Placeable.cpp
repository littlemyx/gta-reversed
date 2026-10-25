/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Placeable.h"
#ifdef NOTSA_PORTABLE
#include <execinfo.h>
static void NotsaMatrixTraceDtor(CPlaceable* self);
#endif

void CPlaceable::InjectHooks() {
    RH_ScopedVirtualClass(CPlaceable, 0x863C40, 1);
    RH_ScopedCategory("Entity");

    RH_ScopedOverloadedInstall(SetPosn, "xyz", 0x420B80, void(CPlaceable::*)(float, float, float));
    RH_ScopedOverloadedInstall(SetPosn, "vector", 0x4241C0, void(CPlaceable::*)(const CVector&));
    RH_ScopedOverloadedInstall(SetOrientation, "xyz", 0x439A80, void(CPlaceable::*)(float, float, float));
    RH_ScopedInstall(SetHeading, 0x43E0C0);
    RH_ScopedInstall(GetHeading, 0x441DB0);
    RH_ScopedInstall(GetRoll, 0x420B30);
    RH_ScopedOverloadedInstall(IsWithinArea, "xy", 0x54F200, bool(CPlaceable::*)(float, float, float, float) const);
    RH_ScopedOverloadedInstall(IsWithinArea, "xyz", 0x54F2B0, bool(CPlaceable::*)(float, float, float, float, float, float) const);
    RH_ScopedInstall(RemoveMatrix, 0x54F3B0);
    RH_ScopedInstall(AllocateStaticMatrix, 0x54F4C0);
    RH_ScopedInstall(AllocateMatrix, 0x54F560);
    RH_ScopedInstall(SetMatrix, 0x54F610);
    RH_ScopedInstall(GetMatrix, 0x411990);
    RH_ScopedInstall(ShutdownMatrixArray, 0x54EFD0);
    RH_ScopedInstall(InitMatrixArray, 0x54F3A0);
    RH_ScopedInstall(FreeStaticMatrix, 0x54F010);
}

CPlaceable::CPlaceable() : m_placement() {
    m_matrix = nullptr;
}

CPlaceable::~CPlaceable() {
#ifdef NOTSA_PORTABLE
    NotsaMatrixTraceDtor(this);
#endif
    if (m_matrix) {
        CPlaceable::RemoveMatrix();
        --numMatrices;
    }

    m_matrix = reinterpret_cast<CMatrixLink*>(&gDummyMatrix);
}

CVector CPlaceable::GetRightVector() {
    if (m_matrix)
        return m_matrix->GetRight();
    return { (float)(x87::cos(m_placement.m_fHeading)), (float)(x87::sin(m_placement.m_fHeading)), 0.0f };
}

CVector CPlaceable::GetForwardVector() {
    if (m_matrix)
        return m_matrix->GetForward();
    return { (float)(-x87::sin(m_placement.m_fHeading)), (float)(x87::cos(m_placement.m_fHeading)), 0.0f };
}

CVector CPlaceable::GetUpVector() {
    if (m_matrix)
        return m_matrix->GetUp();
    return {0.0f, 0.0f, 1.0f};
}

void CPlaceable::SetPosn(float x, float y, float z) {
    auto& pos = GetPosition();
    pos.Set(x, y, z);
}

void CPlaceable::SetPosn(const CVector& posn) {
    auto& pos = GetPosition();
    pos = posn;
}

void CPlaceable::SetOrientation(float x, float y, float z) {
    if (!m_matrix) {
        m_placement.m_fHeading = z;
        return;
    }

    CVector vecPos = m_matrix->GetPosition();
    m_matrix->SetRotate(x, y, z);
    m_matrix->GetPosition() += vecPos;
}

void CPlaceable::SetHeading(float heading) {
    if (m_matrix)
        m_matrix->SetRotateZOnly(heading);
    else
        m_placement.m_fHeading = heading;
}

float CPlaceable::GetHeading() const {
    return (float)GetHeadingExt();
}

double CPlaceable::GetHeadingExt() const {
    if (m_matrix) {
        const auto& fwd = m_matrix->GetForward();
        return x87::atan2(-(double)fwd.x, (double)fwd.y); // FPATAN: stays in ST0, rounded by the caller's next op / store
    }
    return (double)m_placement.m_fHeading;
}

// 0x420B30
float CPlaceable::GetRoll() const {
    // 0x420B30: // The exe's x87 code VERBATIM (note: atan2(right.z, +-sqrt(right.x^2 + right.y^2)), the old port used the SQUARED magnitude) (generated from the asm, called inside the __asm block with the original stack frame): every product / sum is rounded at the current
    // precision control and the sine / cosine / arctangent results stay unrounded on the FPU stack exactly like the original.
    static const uint32 K858B50 = 0x00000000u;
    static const uint32 K858C1C = 0xBF800000u;
    float res;
    const CPlaceable* self_ = this;
#ifdef NOTSA_PORTABLE
#include "x87_lifted/Placeable_GetRoll.inc"
#else
    __asm {
        mov ecx, self_
        call L_BODY
        fstp dword ptr [res]
        jmp L_END
    L_BODY:
        mov ecx, dword ptr [ecx + 0x14]
        test ecx, ecx
        je L_420B6A
        fld dword ptr [ecx + 4]
        fld dword ptr [ecx]
        fld st(0)
        fmul st(0), st(1)
        fld st(2)
        fmul st(0), st(3)
        faddp st(1), st(0)
        fsqrt
        fstp st(2)
        fstp st(0)
        fld dword ptr [ecx + 0x28]
        fcomp dword ptr [K858B50]
        fnstsw ax
        test ah, 5
        jp L_420B62
        fmul dword ptr [K858C1C]
    L_420B62:
        fld dword ptr [ecx + 8]
        fxch st(1)
        fpatan
        ret
    L_420B6A:
        fld dword ptr [K858B50]
        ret
    L_END:
    }
#endif
    return res;
}

bool CPlaceable::IsWithinArea(float x1, float y1, float x2, float y2) const {
    const auto& vecPos = GetPosition();
    if (x1 > x2)
        std::swap(x1, x2);

    if (y1 > y2)
        std::swap(y1, y2);

    return vecPos.x >= x1 && vecPos.x <= x2 && vecPos.y >= y1 && vecPos.y <= y2;
}


bool CPlaceable::IsWithinArea(float x1, float y1, float z1, float x2, float y2, float z2) const {
    const auto& vecPos = GetPosition();
    if (x1 > x2)
        std::swap(x1, x2);

    if (y1 > y2)
        std::swap(y1, y2);

    if (z1 > z2)
        std::swap(z1, z2);

    return vecPos.x >= x1
        && vecPos.x <= x2
        && vecPos.y >= y1
        && vecPos.y <= y2
        && vecPos.z >= z1
        && vecPos.z <= z2;
}

void CPlaceable::RemoveMatrix() {
    const auto& vecForward = m_matrix->GetForward();
    auto fHeading = x87::atan2(-vecForward.x, vecForward.y);

    m_placement.m_vPosn = m_matrix->GetPosition();
    m_placement.m_fHeading = fHeading;

    m_matrix->m_pOwner = nullptr;
    gMatrixList.MoveToFreeList(m_matrix);
    m_matrix = nullptr;
}

#ifdef NOTSA_PORTABLE
// STAB1: the exe evicts the owner of the oldest list-1 link (`oldest->m_pOwner->RemoveMatrix()`). A link in list 1 whose owner is null or no longer points back at it (seen as a null
// `this` fault in CPlaceable::GetMatrix from ProcessVerticalLineSectorList after ~4 min of mixed play) is reclaimed directly instead of being dereferenced; an empty list 1 (every
// matrix static) leaves the allocation to fail (m_matrix stays null) instead of writing through a null link.
static void NotsaEvictOldestMatrix() {
    auto* const oldest = gMatrixList.GetOldestLink();
    if (oldest == &gMatrixList.m_head) {
        static bool once = false;
        if (!once) { once = true; NOTSA_LOG_WARN("[matrix] list 1 is empty and the free list too: free {} list1 {} list2 {}", gMatrixList.GetNumFree(), gMatrixList.GetNumUsed1(), gMatrixList.GetNumUsed2()); }
        return;
    }
    auto* const owner = oldest->m_pOwner;
    if (owner && owner->m_matrix == oldest) {
        owner->RemoveMatrix();
        return;
    }
    static int n = 0;
    if (n++ < 50) { std::fprintf(stderr, "[matrix] ERROR: ownerless / stale link reclaimed (guard FIRED #%d): oldest link %p has owner %p whose matrix is %p (free %d list1 %d list2 %d) frame %u\n", n, (void*)oldest, (void*)owner, owner ? (void*)owner->m_matrix : nullptr, gMatrixList.GetNumFree(), gMatrixList.GetNumUsed1(), gMatrixList.GetNumUsed2(), (unsigned)CTimer::GetFrameCounter()); std::fflush(stderr); }
    oldest->m_pOwner = nullptr;
    gMatrixList.MoveToFreeList(oldest);
}
#endif
#ifdef NOTSA_PORTABLE
// STAB2 diagnostic (env NOTSA_MATRIX_CHECK): does any list-1 / list-2 link already claim `self` as its owner?
static void* g_notsaLinkBt[900][8]; static int g_notsaLinkBtN[900];
static void NotsaRecordAlloc(CMatrixLink* l) {
    static const bool enabled = std::getenv("NOTSA_MATRIX_CHECK") != nullptr;
    if (!enabled || !l) return;
    int i = (int)(l - gMatrixList.m_pObjects);
    if (i >= 0 && i < 900) g_notsaLinkBtN[i] = backtrace(g_notsaLinkBt[i], 8);
}
static CMatrixLink* NotsaFindLinkOwnedBy(CPlaceable* self) {
    CMatrixLink* const heads[2] = { &gMatrixList.m_head, &gMatrixList.m_allocatedListHead };
    CMatrixLink* const tails[2] = { &gMatrixList.m_tail, &gMatrixList.m_allocatedListTail };
    for (int li = 0; li < 2; li++) {
        int n = 0;
        for (CMatrixLink* c = heads[li]->m_pNext; c != tails[li] && n < 2000; c = c->m_pNext, n++) {
            if (c->m_pOwner == self) return c;
        }
    }
    return nullptr;
}
static void NotsaMatrixTrace(const char* what, CPlaceable* self, CMatrixLink* other) {
    static const bool enabled = std::getenv("NOTSA_MATRIX_CHECK") != nullptr;
    if (!enabled) return;
    CMatrixLink* c = NotsaFindLinkOwnedBy(self);
    if (!c || c == other) return;
    static int n = 0;
    if (n++ >= 8) return;
    std::fprintf(stderr, "[matrix] %s: %p already owns link %p (#%d, own m_matrix %p) frame %u\n", what, (void*)self, (void*)c, (int)(c - gMatrixList.m_pObjects), (void*)other, (unsigned)CTimer::GetFrameCounter());
    void* bt[12]; int cnt = backtrace(bt, 12); backtrace_symbols_fd(bt, cnt, 2);
    int i = (int)(c - gMatrixList.m_pObjects);
    std::fprintf(stderr, "[matrix] ... the stale link #%d was allocated at:\n", i);
    if (i >= 0 && i < 900) backtrace_symbols_fd(g_notsaLinkBt[i], g_notsaLinkBtN[i], 2);
}
#endif
#ifdef NOTSA_PORTABLE
// STAB2: list-integrity checker for the CMatrixLink lists. Walks list 1, list 2 and the free list and verifies prev/next symmetry, the list length (== the capacity in total) and that
// every used link's owner points back at it (owner->m_matrix == link) and every free link has no owner. Env NOTSA_MATRIX_CHECK=1 enables the per-frame call (CGame::Process) and
// the call sites that hand-edit link matrices; the first failure of each kind is printed loudly with the frame number and the call site.
static void NotsaMatrixTraceDtor(CPlaceable* self) {
    if (!self->m_matrix) NotsaMatrixTrace("dtor without matrix", self, nullptr);
}
bool NotsaCheckMatrixLists(const char* where) {
    static const bool enabled = std::getenv("NOTSA_MATRIX_CHECK") != nullptr;
    if (!enabled) return true;
    static int reported = 0, calls = 0;
    if (calls++ == 0) { std::fprintf(stderr, "[matrix] list checker active (first call at %s, frame %u)\n", where, (unsigned)CTimer::GetFrameCounter()); }
    if (std::strncmp(where, "CCarCtrl", 8) == 0) { static int sites = 0; if (sites++ < 10) { std::fprintf(stderr, "[matrix] site reached: %s frame %u\n", where, (unsigned)CTimer::GetFrameCounter()); } }
    auto& L = gMatrixList;
    CMatrixLink* const heads[3] = { &L.m_head, &L.m_allocatedListHead, &L.m_freeListHead };
    CMatrixLink* const tails[3] = { &L.m_tail, &L.m_allocatedListTail, &L.m_freeListTail };
    const char* const names[3] = { "list1", "list2", "free" };
    int total = 0, bad = 0;
    char msg[512]{};
    for (int li = 0; li < 3 && !bad; li++) {
        int n = 0;
        for (CMatrixLink* c = heads[li]->m_pNext; c != tails[li]; c = c->m_pNext) {
            if (++n > 5000 || !c) { std::snprintf(msg, sizeof msg, "%s walk runaway (n %d link %p)", names[li], n, (void*)c); bad = 1; break; }
            if (c->m_pNext->m_pPrev != c) { std::snprintf(msg, sizeof msg, "%s link %p: next->prev != self", names[li], (void*)c); bad = 1; break; }
            if (c < L.m_pObjects || c >= L.m_pObjects + CPlaceable::NUM_MATRICES_TO_CREATE) { std::snprintf(msg, sizeof msg, "%s link %p outside the link array", names[li], (void*)c); bad = 1; break; }
            if (li < 2) {
                CPlaceable* o = c->m_pOwner;
                if (!o) { std::snprintf(msg, sizeof msg, "%s link %p (#%d) has NO OWNER", names[li], (void*)c, (int)(c - L.m_pObjects)); bad = 1; break; }
                if (o->m_matrix != c) {
                    std::snprintf(msg, sizeof msg, "%s link %p (#%d) owner %p (vehicle pool %d ped %d object %d building %d dummy %d) has m_matrix %p != link", names[li], (void*)c, (int)(c - L.m_pObjects), (void*)o,
                        (int)GetVehiclePool()->IsPtrFromPool((const CVehicle*)o), (int)GetPedPool()->IsPtrFromPool((const CPed*)o), (int)GetObjectPool()->IsPtrFromPool((const CObject*)o),
                        (int)GetBuildingPool()->IsPtrFromPool((const CBuilding*)o), (int)GetDummyPool()->IsPtrFromPool((const CDummy*)o), (void*)o->m_matrix);
                    bad = 1; break;
                }
            } else if (c->m_pOwner) { std::snprintf(msg, sizeof msg, "free link %p (#%d) still has owner %p", (void*)c, (int)(c - L.m_pObjects), (void*)c->m_pOwner); bad = 1; break; }
        }
        total += n;
    }
    if (!bad && total != (int)CPlaceable::NUM_MATRICES_TO_CREATE) { std::snprintf(msg, sizeof msg, "lists hold %d links, capacity %d (a link is lost or duplicated)", total, (int)CPlaceable::NUM_MATRICES_TO_CREATE); bad = 1; }
    if (bad && reported++ < 12) {
        std::fprintf(stderr, "[matrix] CHECK FAILED at %s, frame %u: %s\n", where, (unsigned)CTimer::GetFrameCounter(), msg);
        std::fflush(stderr);
    }
    return !bad;
}
#endif
void CPlaceable::AllocateStaticMatrix() {
    if (m_matrix)
        return gMatrixList.MoveToList2(m_matrix);

#ifdef NOTSA_PORTABLE
    if (gMatrixList.IsFull())
        NotsaEvictOldestMatrix();
    if (gMatrixList.IsFull())
        return;
#else
    if (gMatrixList.IsFull())
        gMatrixList.GetOldestLink()->m_pOwner->RemoveMatrix();
#endif

    m_matrix = gMatrixList.AddToList2();
    m_matrix->m_pOwner = this;
#ifdef NOTSA_PORTABLE
    NotsaMatrixTrace("AllocateStaticMatrix", this, m_matrix);
    NotsaRecordAlloc(m_matrix);
#endif
}

void CPlaceable::AllocateMatrix() {
    if (m_matrix)
        return;

#ifdef NOTSA_PORTABLE
    if (gMatrixList.IsFull())
        NotsaEvictOldestMatrix();
    if (gMatrixList.IsFull())
        return;
#else
    if (gMatrixList.IsFull())
        gMatrixList.GetOldestLink()->m_pOwner->RemoveMatrix();
#endif

    m_matrix = gMatrixList.AddToList1();
    m_matrix->m_pOwner = this;
#ifdef NOTSA_PORTABLE
    NotsaMatrixTrace("AllocateMatrix", this, m_matrix);
    NotsaRecordAlloc(m_matrix);
#endif
}

void CPlaceable::SetMatrix(CMatrix& matrix) {
    if (!m_matrix) {
        if (matrix.GetUp().z == 1.0F) {
            auto& vecForward = matrix.GetForward();
            auto fHeading = x87::atan2(-vecForward.x, vecForward.y);

            m_placement.m_vPosn = matrix.GetPosition();
            m_placement.m_fHeading = fHeading;
            return;
        }
        CPlaceable::AllocateMatrix();
    }

    *static_cast<CMatrix*>(m_matrix) = matrix;
}

// NOTSA
bool CPlaceable::IsPointInRange(const CVector& point, float range) {
    return DistanceBetweenPointsSquared(point, GetPosition()) <= sq(range);
}

CMatrix& CPlaceable::GetMatrix() {
    if (!m_matrix) {
        CPlaceable::AllocateMatrix();
        m_placement.UpdateMatrix(m_matrix);
    }

    return *m_matrix;
}

void CPlaceable::ShutdownMatrixArray() {
    gMatrixList.Shutdown();
}

void CPlaceable::InitMatrixArray() {
    ZoneScoped;

    gMatrixList.Init(CPlaceable::NUM_MATRICES_TO_CREATE);
}

void CPlaceable::FreeStaticMatrix() {
    gMatrixList.MoveToList1(m_matrix);
}

void CPlaceable::GetOrientation(float& x, float& y, float& z) {
    if (!m_matrix) {
        z = m_placement.m_fHeading;
        return;
    }

    x = x87::asin(GetForward().z);

    float cosx = x87::cos(x);
    float cosy = GetUp().z / cosx;
    y = x87::acos(cosy);

    float cosz = GetForward().y / cosx;
    z = x87::acos(cosz);
}
