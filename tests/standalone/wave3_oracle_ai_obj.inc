// ORACLE-W3-AI rows: Entity/Object/Object.cpp (classes A + B). Objects (type 4) in the scratch block with matrices / model infos, the double-link node pool + control-code list head (0xB9ACCC),
// a txd pool for the remap texture refcount, the entity reference world for the dummy / object-pool functions.
namespace w3a {
static PoolFx<CPtrNodeDoubleLinkPool, 0xC, 0xB74488, 8> g_dlp;   // CPtrNodeDoubleLink<void*> pool (exe global 0xB74488)
static PoolFx<CTxdPool, sizeof(TxdDef), 0xC8800C, 6> g_txp;
static void ObjWorld(Rng& r, size_t off, int matSlot) {
    Ent(r, off, matSlot, 4, (uint16_t)r.below(20000), 0x1A0, false);   // the matrix exists (GetMatrix() would allocate one from the matrix list)
    auto* o = reinterpret_cast<CObject*>(S(off));
    o->m_nObjectType = (uint8_t)(r.below(8) == 0 ? r.below(256) : r.below(9));
    o->m_fDoorStartAngle = r.below(3) ? Fl(r, 3.f, 6) : -1000.f - r.f01() * 2000.f;
    o->m_nRefModelIndex = (int16_t)r.below(6);
}
// UCase with the two fake pools compared as well (and the control-code list built over the double-link pool)
template<class PF, class EF>
static bool UCaseP(Rng& r, std::string& d, PF&& pf, EF&& ef) {
    const uint32_t seed = r.u32() | 1;
    g_dlp.Prepare(r, true); g_txp.Prepare(r, true);
    g_dlp.Enter(); g_txp.Enter();
    BuildAll(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    const bool ok = g_useRf ? BothA(d, seed, { sc, img, g_dlp.RegStruct(), g_dlp.RegFlags(), g_dlp.RegStore(), g_txp.RegStruct(), g_txp.RegFlags(), g_txp.RegStore() }, std::forward<PF>(pf), std::forward<EF>(ef), kRf)
                            : BothA(d, seed, { sc, img, g_dlp.RegStruct(), g_dlp.RegFlags(), g_dlp.RegStore(), g_txp.RegStruct(), g_txp.RegFlags(), g_txp.RegStore() }, std::forward<PF>(pf), std::forward<EF>(ef));
    if (g_useRf) rf::Leave();
    g_txp.Leave(); g_dlp.Leave();
    return ok;
}
} // namespace w3a

static void TestW3AiObjA() {
    using namespace w2; using namespace w3a;
    const auto objA = [] { return reinterpret_cast<CObject*>(S(oObjA)); };
    ROW("CObject::SetIsStatic", 0x5A0760, 2000) {
        const bool st = r.below(2) != 0;
        Tu tu([&](Rng& rr) { ObjWorld(rr, oObjA, 0); if (rr.below(2)) objA()->physicalFlags.bDisableMoveForce = true; });
        return UCase(r, d, [&]() -> uint64_t { objA()->CObject::SetIsStatic(st); return 0; }, [&]() -> uint64_t { ExeThis<void, int>(0x5A0760, objA(), (int)st); return 0; });
    } ENDROW;
    ROW("CObject::CanBeDeleted", 0x59F120, 300) {
        Tu tu([&](Rng& rr) { ObjWorld(rr, oObjA, 0); });
        return UCase(r, d, [&]() -> uint64_t { return B8(objA()->CanBeDeleted()); }, [&]() -> uint64_t { return B8(ExeThis<bool>(0x59F120, objA())); });
    } ENDROW;
    ROW("CObject::SetObjectTargettable", 0x59F300, 200) {
        const bool b = r.below(2) != 0;
        Tu tu([&](Rng& rr) { ObjWorld(rr, oObjA, 0); });
        return UCase(r, d, [&]() -> uint64_t { objA()->SetObjectTargettable(b); return 0; }, [&]() -> uint64_t { ExeThis<void, int>(0x59F300, objA(), (int)b); return 0; });
    } ENDROW;
    ROW("CObject::CanBeTargetted", 0x59F320, 200) {
        Tu tu([&](Rng& rr) { ObjWorld(rr, oObjA, 0); });
        return UCase(r, d, [&]() -> uint64_t { return B8(objA()->CanBeTargetted()); }, [&]() -> uint64_t { return B8(ExeThis<bool>(0x59F320, objA())); });
    } ENDROW;
    ROW("CObject::SetMatrixForTrainCrossing", 0x59F200, 2000) {
        const float ang = r.below(10) == 0 ? Fl(r, 10.f, 3) : (r.f01() - 0.5f) * 7.f;
        Tu tu([&](Rng& rr) { ObjWorld(rr, oObjA, 0); Mat(rr, 3, rr.below(4) != 0); });
        return UCase(r, d, [&]() -> uint64_t { CObject::SetMatrixForTrainCrossing(reinterpret_cast<CMatrix*>(S(oMat + 3 * 0x60)), ang); return 0; },
                                  [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, float)>(0x59F200)(S(oMat + 3 * 0x60), ang); return 0; });
    } ENDROW;
}
static void TestW3AiObjB() {
    using namespace w2; using namespace w3a;
    const auto objA = [] { return reinterpret_cast<CObject*>(S(oObjA)); };
    ROW("CObject::RefModelInfo", 0x59F330, 300) {
        const int idx = (int)r.below(4);
        Tu tu([&](Rng& rr) {
            ObjWorld(rr, oObjA, 0);
            for (int i = 0; i < g_txp.N; ++i) { auto* q = g_txp.store + i * sizeof(TxdDef); FillGarbage(rr, q, sizeof(TxdDef)); *reinterpret_cast<uint16_t*>(q + 4) = (uint16_t)rr.below(5); g_txp.flags[i] &= 0x7F; }
            for (int i = 0; i < 4; ++i) { ModelInfoFor(rr, i, (uint16_t)i, 5.f); SAt<int16_t>(oMi + (size_t)i * 0x400 + 0xA) = (int16_t)(rr.below(5) == 0 ? -1 : (int)rr.below(g_txp.N)); }
        });
        return UCaseP(r, d, [&]() -> uint64_t { objA()->RefModelInfo(idx); return 0; }, [&]() -> uint64_t { ExeThis<void, int>(0x59F330, objA(), idx); return 0; });
    } ENDROW;
    ROW("CObject::SetRemapTexture", 0x59F350, 400) {
        const int16_t txd = r.below(4) == 0 ? (int16_t)-1 : (int16_t)r.below(6);
        const uint32_t tex = r.u32();
        Tu tu([&](Rng& rr) { ObjWorld(rr, oObjA, 0); for (int i = 0; i < g_txp.N; ++i) { auto* q = g_txp.store + i * sizeof(TxdDef); FillGarbage(rr, q, sizeof(TxdDef)); *reinterpret_cast<uint16_t*>(q + 4) = (uint16_t)rr.below(5); } for (int i = 0; i < g_txp.N; ++i) g_txp.flags[i] &= 0x7F; });
        return UCaseP(r, d, [&]() -> uint64_t { objA()->SetRemapTexture(reinterpret_cast<RwTexture*>(uintptr_t(tex)), txd); return 0; },
                            [&]() -> uint64_t { ExeThis<void, uint32_t, int>(0x59F350, objA(), tex, (int)txd); return 0; });
    } ENDROW;
    // the control-code list: a few used nodes of the double-link pool chained from the head at 0xB9ACCC (item = a scratch object), the object's own node is one of them (or null)
    const auto buildList = [&](Rng& rr, bool own) {
        ObjWorld(rr, oObjA, 0);
        auto* store = g_dlp.store; auto* fl = g_dlp.flags;
        int used[8], n = 0;
        for (int i = 0; i < g_dlp.N; ++i) if (!(fl[i] & 0x80)) used[n++] = i;
        uint8_t* prev = nullptr; uint8_t* first = nullptr; uint8_t* mine = nullptr;
        for (int k = 0; k < n; ++k) {
            uint8_t* nd = store + used[k] * 0xC;
            *reinterpret_cast<void**>(nd) = rr.below(2) ? S(oObjB) : S(oObjA);                 // item
            *reinterpret_cast<void**>(nd + 4) = nullptr; *reinterpret_cast<void**>(nd + 8) = prev;   // next / prev
            if (prev) *reinterpret_cast<void**>(prev + 4) = nd; else first = nd;
            prev = nd;
            if (rr.below(3) == 0 || !mine) mine = nd;
        }
        // the exe's list is a push-front list: head = the last node chained (reverse the chain by treating `prev` as the head and swapping)
        // simpler: head = first, the chain runs through +4 (next); +8 (prev) was set to the previous node
        At<uint32_t>(0xB9ACCC) = (uint32_t)(uintptr_t)first;
        objA()->m_pControlCodeList = own ? reinterpret_cast<CPtrNodeDoubleLink<CObject*>*>(mine) : nullptr;
    };
    ROW("CObject::AddToControlCodeList", 0x59F400, 600) {
        Tu tu([&](Rng& rr) { buildList(rr, false); });
        return UCaseP(r, d, [&]() -> uint64_t { objA()->AddToControlCodeList(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x59F400, objA()); return 0; });
    } ENDROW;
    ROW("CObject::RemoveFromControlCodeList", 0x59F450, 600) {
        const bool own = r.below(5) != 0;
        Tu tu([&](Rng& rr) { buildList(rr, own); });
        return UCaseP(r, d, [&]() -> uint64_t { objA()->RemoveFromControlCodeList(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x59F450, objA()); return 0; });
    } ENDROW;
    ROW("CObject::SetRelatedDummy", 0x59F160, 400) {
        const int di = (int)r.below(rf::kO);
        Tu tu([&](Rng& rr) { ObjWorld(rr, oObjA, 0); }, true);
        return UCase(r, d, [&]() -> uint64_t { objA()->SetRelatedDummy(reinterpret_cast<CDummyObject*>(rf::ObjE(di))); return 0; },
                           [&]() -> uint64_t { ExeThis<void, void*>(0x59F160, objA(), rf::ObjE(di)); return 0; });
    } ENDROW;
    const auto ipv = [&](bool valid, uint32_t addr, Rng& r, std::string& d) -> bool {
        const int slot = (int)r.below(rf::kO + 2), mode = (int)r.below(6);   // mode: 0..3 slot start, 4 inside a slot, 5 null
        Tu tu([&](Rng& rr) {
            for (int i = 0; i < rf::kO; ++i) {
                auto* o = reinterpret_cast<CObject*>(rf::ObjE(i));
                o->m_bIsBIGBuilding = rr.below(3) == 0;
                *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(o) + offsetof(CObject, m_pCollisionList)) = rr.below(2) ? nullptr : S(oOut + 16 * rr.below(8));
            }
        }, true);
        const auto pick = [&]() -> CObject* { if (mode == 5) return nullptr; auto* p = reinterpret_cast<uint8_t*>(slot < rf::kO ? rf::ObjE(slot) : rf::ObjE(0) + (slot - rf::kO) * 0x19C * 40); return reinterpret_cast<CObject*>(mode == 4 ? p + 8 : p); };
        return UCase(r, d, [&]() -> uint64_t { return B8(valid ? IsObjectPointerValid(pick()) : IsObjectPointerValid_NotInWorld(pick())); },
                           [&]() -> uint64_t { return B8(oracle::Fn<bool __cdecl(void*)>(addr)(pick())); });
    };
    ROW("IsObjectPointerValid_NotInWorld", 0x5A2B90, 800) { return ipv(false, 0x5A2B90, r, d); } ENDROW;
    ROW("IsObjectPointerValid", 0x5A2C20, 800) { return ipv(true, 0x5A2C20, r, d); } ENDROW;
}
