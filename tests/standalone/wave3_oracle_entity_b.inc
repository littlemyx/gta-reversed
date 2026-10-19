// ORACLE-W3-ENTITY rows B: CPlaceable matrix management on the real matrix list (gMatrixList 0xB74288), pool-pointer validity (placeholder: filled in below).
static void TestW3EntityB() {
    using namespace w2; using namespace w3e;
    ROW("CPlaceable::GetMatrix", 0x411990, 600) { EC(return U(&PlcA()->CPlaceable::GetMatrix());, return (uint64_t)ExeT<uint32_t>(0x411990, PlcA());); } ENDROW;
    ROW("CPlaceable::AllocateMatrix", 0x54F560, 600) { EC(PlcA()->CPlaceable::AllocateMatrix(); return 0;, ExeT<void>(0x54F560, PlcA()); return 0;); } ENDROW;
    ROW("CPlaceable::AllocateStaticMatrix", 0x54F4C0, 600) { EC(PlcA()->CPlaceable::AllocateStaticMatrix(); return 0;, ExeT<void>(0x54F4C0, PlcA()); return 0;); } ENDROW;
    ROW("CPlaceable::RemoveMatrix", 0x54F3B0, 400) {
        Tw tw([](Rng& rr) { if (!EntA()->m_matrix) { CMatrixLink* n = gMatrixList.m_head.m_pNext; (void)n; EntA()->m_matrix = Node(0); Node(0)->m_pOwner = EntA(); WireMatrixBody(rr, reinterpret_cast<uint8_t*>(Node(0))); Node(0)->m_pPrev = &gMatrixList.m_head; Node(0)->m_pNext = gMatrixList.m_head.m_pNext; gMatrixList.m_head.m_pNext->m_pPrev = Node(0); gMatrixList.m_head.m_pNext = Node(0); } });
        EC(PlcA()->CPlaceable::RemoveMatrix(); return 0;, ExeT<void>(0x54F3B0, PlcA()); return 0;);
    } ENDROW;
    ROW("CPlaceable::SetMatrix", 0x54F610, 800) {
        Tw tw([](Rng& rr) { WireMatrixBody(rr, P(kMat)); auto& M = At<CMatrix>(kMat); if (rr.below(2)) { M.GetUp() = CVector(0.f, 0.f, 1.f); } else if (rr.below(6) == 0) { M.GetUp().z = 1.0000001f; } });
        EC(PlcA()->CPlaceable::SetMatrix(At<CMatrix>(kMat)); return 0;, ExeT<void>(0x54F610, PlcA(), P(kMat)); return 0;);
    } ENDROW;
    ROW("CPlaceable::FreeStaticMatrix", 0x54F010, 400) {
        // the exe moves the placeable's own matrix node to the used list (the matrix is not null in the game); a null matrix would fault both sides
        Tw tw([](Rng& rr) { if (!EntA()->m_matrix) { EntA()->m_matrix = Node(0); Node(0)->m_pOwner = EntA(); WireMatrixBody(rr, reinterpret_cast<uint8_t*>(Node(0))); Node(0)->m_pPrev = &gMatrixList.m_allocatedListHead; Node(0)->m_pNext = gMatrixList.m_allocatedListHead.m_pNext; gMatrixList.m_allocatedListHead.m_pNext->m_pPrev = Node(0); gMatrixList.m_allocatedListHead.m_pNext = Node(0); } });
        EC(PlcA()->CPlaceable::FreeStaticMatrix(); return 0;, ExeT<void>(0x54F010, PlcA()); return 0;);
    } ENDROW;
}
