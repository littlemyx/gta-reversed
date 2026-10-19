// ORACLE-W3-WORLD rows B: PathFind.cpp (see wave3_oracle_world.inc). ROW / IC / Tw are defined there. Fixture: BuildPaths (72 areas of 10 nodes with real links, the last 16 link entries of every area free).
// Exe calling convention (thiscall, callee cleans): oracle::Fn<R __fastcall(CPathFind*, int, args...)>(addr)(&ThePaths, 0, args...); struct returns: hidden CNodeAddress* first.
template<class R, class... A> static R PFn(uint32_t addr, A... a) { return oracle::Fn<R __fastcall(CPathFind*, int, A...)>(addr)(&ThePaths, 0, a...); }
static uint32_t Dw(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
#define P ThePaths
static void TestW3WorldB() {
    using namespace w2; using namespace w3w;
    const auto Pos = [](Rng& rr) { return CVector(FlP(rr, 3500.f), FlP(rr, 3500.f), FlP(rr, 100.f)); };
    // ---- node predicates / counters
    ROW("CPathFind::ThisNodeHasToBeSwitchedOff", 0x44D3E0, 200) {
        const int a = r.below(NAREA), n = r.below(NNODE);
        IC(return B8(P.ThisNodeHasToBeSwitchedOff(NodeP(a, n)));, return (uint64_t)PFn<uint8_t>(0x44D3E0, (void*)NodeP(a, n)););
    } ENDROW;
    ROW("CPathFind::IsAreaNodesAvailable", 0x420AA0, 300) {
        const int a = r.below(NAREA), n = r.below(NNODE);
        IC(return B8(P.IsAreaNodesAvailable(CNodeAddress{ (uint16)a, (uint16)n }));, return (uint64_t)PFn<uint8_t>(0x420AA0, NA(a, n)););
    } ENDROW;
    ROW("CPathFind::HaveRequestedNodesBeenLoaded", 0x450DB0, 300) {
        Tw tw([](Rng& rr) { const float x = FlP(rr, 3500.f), y = FlP(rr, 3500.f), w = rr.f01() * 2500.f; P.m_loadAreaRequestMinX = x; P.m_loadAreaRequestMaxX = x + w; P.m_loadAreaRequestMinY = y; P.m_loadAreaRequestMaxY = y + w; if (rr.below(10) == 0) P.m_loadAreaRequestMinX = Fl(rr, 4000.f, 1); });
        IC(return B8(P.HaveRequestedNodesBeenLoaded());, return (uint64_t)PFn<uint8_t>(0x450DB0););
    } ENDROW;
    ROW("CPathFind::AreNodesLoadedForArea", 0x44DD10, 800) {
        const float x = r.below(10) == 0 ? Fl(r, 5000.f, 1) : FlP(r, 3500.f), y = r.below(10) == 0 ? Fl(r, 5000.f, 1) : FlP(r, 3500.f), w = r.below(4) ? r.f01() * 2500.f : FlP(r, 5000.f), h = r.below(4) ? r.f01() * 2500.f : FlP(r, 5000.f);
        Tw tw([](Rng& rr) { const int pct = (int)rr.below(101); for (int a = 0; a < 64; ++a) if ((int)rr.below(100) >= pct) P.m_pPathNodes[a] = nullptr; });
        IC(return B8(P.AreNodesLoadedForArea(x, x + w, y, y + h));, return (uint64_t)PFn<uint8_t>(0x44DD10, x, x + w, y, y + h););
    } ENDROW;
    ROW("CPathFind::CountNeighboursToBeSwitchedOff", 0x4504F0, 400) {
        const int a = r.below(NAREA), n = r.below(NNODE);
        IC(return (uint64_t)P.CountNeighboursToBeSwitchedOff(*NodeP(a, n));, return (uint64_t)PFn<uint32_t>(0x4504F0, (void*)NodeP(a, n)););
    } ENDROW;
    ROW("CPathFind::TidyUpNodeSwitchesAfterMission", 0x44D3B0, 100) {
        Tw tw([](Rng& rr) { P.m_nNumNodeSwitches = rr.below(5) ? rr.below(64) : rr.below(70); });
        IC(P.TidyUpNodeSwitchesAfterMission(); return 0;, PFn<void>(0x44D3B0); return 0;);
    } ENDROW;
    // ---- path-finding list
    ROW("CPathFind::AddNodeToList", 0x44D1E0, 400) {
        const int a = r.below(NAREA), n = r.below(NNODE), dist = r.below(4) ? (int)r.below(20000) : (int)r.below(70000);
        Tw tw([](Rng& rr) { for (int b = 0; b < 512; ++b) P.m_pathFindHashTable[b] = rr.below(3) == 0 ? NodeP(rr.below(NAREA), rr.below(NNODE)) : nullptr; P.m_totalNumNodesInPathFindHashTable = rr.below(100); });
        IC(P.AddNodeToList(NodeP(a, n), dist); return 0;, PFn<void>(0x44D1E0, (void*)NodeP(a, n), dist); return 0;);
    } ENDROW;
    ROW("CPathFind::RemoveNodeFromList", 0x44D1B0, 300) {
        const int mode = r.below(3);   // 0: head of a bucket with a successor, 1: middle, 2: tail
        Tw tw([mode](Rng& rr) {
            CPathNode* A = NodeP(0, 0); CPathNode* X = NodeP(0, 1); CPathNode* C = NodeP(0, 2);
            CPathNode** slot = reinterpret_cast<CPathNode**>(S(oBuf));
            *slot = mode == 0 ? X : A;
            auto setNext = [](CPathNode* n, CPathNode* v) { std::memcpy(reinterpret_cast<uint8_t*>(n), &v, 4); };
            auto setPrev = [](CPathNode* n, void* v) { std::memcpy(reinterpret_cast<uint8_t*>(n) + 4, &v, 4); };
            if (mode == 0) { setPrev(X, slot); setNext(X, C); setPrev(C, X); setNext(C, nullptr); }
            else { setPrev(A, slot); setNext(A, X); setPrev(X, A); setNext(X, mode == 1 ? C : nullptr); setPrev(C, X); setNext(C, nullptr); }
            P.m_totalNumNodesInPathFindHashTable = 1 + rr.below(100);
        });
        IC(P.RemoveNodeFromList(NodeP(0, 1)); return 0;, PFn<void>(0x44D1B0, (void*)NodeP(0, 1)); return 0;);
    } ENDROW;
    ROW("CPathFind::Init", 0x44D080, 60) {
        Tw tw([](Rng& rr) { FillGarbage(rr, 0x96F02C, 4); P.m_nNumNodeSwitches = rr.u32(); P.m_loadAreaRequestPending = rr.below(2); });
        IC(P.Init(); return 0;, PFn<void>(0x44D080); return 0;);
    } ENDROW;
    ROW("CPathFind::ReInit", 0x44E4E0, 60) {
        Tw tw([](Rng& rr) { P.m_nNumNodeSwitches = rr.u32(); P.m_loadAreaRequestPending = rr.below(2); });
        IC(P.ReInit(); return 0;, PFn<void>(0x44E4E0); return 0;);
    } ENDROW;
    ROW("CPathNode::GetPosition", 0x420A10, 300) {
        const int a = r.below(NAREA), n = r.below(NNODE);
        IC(*reinterpret_cast<CVector*>(S(oOut)) = NodeP(a, n)->GetPosition(); return 0;, oracle::Fn<CVector* __fastcall(CPathNode*, int, CVector*)>(0x420A10)(NodeP(a, n), 0, reinterpret_cast<CVector*>(S(oOut))); return 0;);
    } ENDROW;
    // ---- interiors
    ROW("CPathFind::ReturnInteriorNodeIndex", 0x451300, 400) {
        const uint32_t intId = r.below(3) ? (r.below(4) | (r.below(5) << 16)) : r.u32();
        const int16_t nodeId = (int16_t)r.below(70);
        Tw tw([&](Rng& rr) { if (rr.below(2)) P.m_interiorIDs[rr.below(8)] = intId; });
        IC(const CNodeAddress a = P.ReturnInteriorNodeIndex(0, intId, nodeId); uint32_t v; std::memcpy(&v, &a, 4); return (uint64_t)v;,
           CNodeAddress a = CNodeAddress((uint16)0x1111, (uint16)0x2222); oracle::Fn<CNodeAddress* __fastcall(CPathFind*, int, CNodeAddress*, uint32_t, int16_t)>(0x451300)(&P, 0, &a, intId, nodeId); uint32_t v; std::memcpy(&v, &a, 4); return (uint64_t)v;);
    } ENDROW;
    ROW("CPathFind::SetPathsNeededAtPosition", 0x44DCD0, 80) {
        const CVector pos = Pos(r);
        IC(P.SetPathsNeededAtPosition(pos); return 0;, PFn<void>(0x44DCD0, (const CVector*)&pos); return 0;);
    } ENDROW;
    ROW("CPathFind::StartNewInterior", 0x44DE80, 200) {
        const int num = r.below(1000);
        Tw tw([](Rng& rr) { for (int k = 0; k < 8; ++k) if (rr.below(2)) P.m_interiorIDs[k] = rr.below(100); });
        IC(P.StartNewInterior(num); return 0;, PFn<void>(0x44DE80, num); return 0;);
    } ENDROW;
    ROW("CPathFind::AddNodeToNewInterior", 0x450E90, 300) {
        const CVector pos = Pos(r); const int dw = r.below(2); int8_t c[6]; for (auto& v : c) v = (int8_t)(r.below(4) ? (int)r.below(60) : (int)r.below(256));
        Tw tw([](Rng& rr) { CPathFind::NumNodesGiven = rr.below(60); CPathFind::NewInteriorSlot = rr.below(8); });
        IC(const CNodeAddress a = P.AddNodeToNewInterior(pos.x, pos.y, pos.z, dw != 0, c[0], c[1], c[2], c[3], c[4], c[5]); uint32_t v; std::memcpy(&v, &a, 4); return (uint64_t)v;,
           CNodeAddress a = CNodeAddress((uint16)0x1111, (uint16)0x2222); oracle::Fn<CNodeAddress* __fastcall(CPathFind*, int, CNodeAddress*, float, float, float, uint32_t, int, int, int, int, int, int)>(0x450E90)(&P, 0, &a, pos.x, pos.y, pos.z, dw, c[0], c[1], c[2], c[3], c[4], c[5]); uint32_t v; std::memcpy(&v, &a, 4); return (uint64_t)v;);
    } ENDROW;
    ROW("CPathFind::RemoveLinksToNewInteriorNode", 0x44DF60, 300) {
        const int idx = r.below(64);
        Tw tw([](Rng& rr) { CPathFind::NumNodesGiven = rr.below(65); for (int i = 0; i < 64 * 6; ++i) reinterpret_cast<int8_t*>(0x96EAC0)[i] = (int8_t)(rr.below(3) ? (int)rr.below(64) : (int)rr.below(256)); });
        IC(P.RemoveLinksToNewInteriorNode(idx); return 0;, PFn<void>(0x44DF60, idx); return 0;);
    } ENDROW;
    ROW("CPathFind::AddInteriorLink", 0x44DED0, 400) {
        const int a = r.below(63), b = r.below(63);   // (an overflowing row writes into the next one: never past the 64 rows)
        Tw tw([](Rng& rr) { for (int i = 0; i < 64 * 6 + 8; ++i) reinterpret_cast<int8_t*>(0x96EAC0)[i] = rr.below(3) ? (int8_t)-1 : (int8_t)rr.below(64); });
        IC(P.AddInteriorLink(a, b); return 0;, PFn<void>(0x44DED0, a, b); return 0;);
    } ENDROW;
    ROW("CPathFind::AddInteriorLinkToExternalNode", 0x44DF30, 200) {
        const int idx = r.below(64); const uint32_t ext = NA(r.below(64), r.below(NNODE));
        Tw tw([](Rng& rr) { CPathFind::NumLinksToExteriorNodes = rr.below(8); });
        IC(P.AddInteriorLinkToExternalNode(idx, CNodeAddress{ (uint16)(ext & 0xFFFF), (uint16)(ext >> 16) }); return 0;, PFn<void>(0x44DF30, idx, ext); return 0;);
    } ENDROW;
    ROW("CPathFind::FindNearestExteriorNodeToInteriorNode", 0x450F30, 500) {
        const int id = r.below(64);
        Tw tw([id](Rng& rr) {   // the interior node sits next to (or far from) a random exterior node
            const int a = rr.below(64), n = rr.below(NNODE);
            const CVector p = NodeP(a, n)->GetPosition();
            const float sp = rr.below(4) ? 1.5f : 30.f;
            reinterpret_cast<float*>(0x96EE80)[id] = p.x + (rr.f01() * 2 - 1) * sp; reinterpret_cast<float*>(0x96ED80)[id] = p.y + (rr.f01() * 2 - 1) * sp; reinterpret_cast<float*>(0x96EC80)[id] = p.z + (rr.f01() * 2 - 1) * sp * 0.5f;
        });
        IC(const CNodeAddress a = P.FindNearestExteriorNodeToInteriorNode(id); return (uint64_t)(a.m_wAreaId == 0xFFFF ? 0xFFFF : ((uint32_t)a.m_wNodeId << 16 | a.m_wAreaId));,   // (the node of "nothing found" is garbage in the exe)
           CNodeAddress a = CNodeAddress((uint16)0x1111, (uint16)0x2222); oracle::Fn<CNodeAddress* __fastcall(CPathFind*, int, CNodeAddress*, int)>(0x450F30)(&P, 0, &a, id); return (uint64_t)(a.m_wAreaId == 0xFFFF ? 0xFFFF : ((uint32_t)a.m_wNodeId << 16 | a.m_wAreaId)););
    } ENDROW;
    // ---- dynamic links
    ROW("CPathFind::AddDynamicLinkBetween2Nodes_For1Node", 0x44E000, 800) {
        const int a = r.below(r.below(4) ? 64 : NAREA), n = r.below(NNODE); const uint32_t second = NA(r.below(NAREA), r.below(NNODE));
        Tw tw([a](Rng& rr) {
            for (int ar = 0; ar < NAREA; ++ar) P.m_anNumAddresses[ar] = rr.below(3) ? NLINK_USED : rr.below(NLINK - 12);
            for (int ar = 0; ar < 64; ++ar) for (int k = 0; k < NUM_DYNAMIC_LINKS_PER_AREA; ++k) { P.m_aDynamicLinksBaseIds[ar][k] = rr.below(3) ? -1 : (int)rr.below(30); P.m_aDynamicLinksIds[ar][k] = rr.below(3) ? -1 : (int)rr.below(30); }
            if (rr.below(3) == 0) PutI16(Node(a, 0), 0x10, (int)rr.below(NLINK));
        });
        IC(P.AddDynamicLinkBetween2Nodes_For1Node(CNodeAddress{ (uint16)a, (uint16)n }, CNodeAddress{ (uint16)(second & 0xFFFF), (uint16)(second >> 16) }); return 0;, PFn<void>(0x44E000, NA(a, n), second); return 0;);
    } ENDROW;
    // ---- bridge lights / regions
    ROW("CPathFind::SetLinksBridgeLights", 0x44D960, 400) {
        const float x0 = FlP(r, 3500.f), y0 = FlP(r, 3500.f), w = r.below(4) ? r.f01() * 3000.f : FlP(r, 3000.f), h = r.below(4) ? r.f01() * 3000.f : FlP(r, 3000.f); const int b = r.below(2);
        Tw tw([](Rng& rr) { for (int a = 0; a < 64; ++a) { P.m_anNumCarPathLinks[a] = rr.below(NLINK + 1); } if (rr.below(10) == 0) { auto* nv = g_pf.navi[rr.below(64)][rr.below(NLINK_USED)]; PutI16(nv, 0, 0x4000); } });
        IC(P.SetLinksBridgeLights(x0, x0 + w, y0, y0 + h, b != 0); return 0;, PFn<void>(0x44D960, x0, x0 + w, y0, y0 + h, (uint32_t)b); return 0;);
    } ENDROW;
    ROW("CPathFind::MarkRegionsForCoors", 0x44DB60, 500) {
        const CVector pos = Pos(r); const float rad = r.below(4) ? r.f01() * 1500.f : FlP(r, 3000.f);
        Tw tw([](Rng& rr) { FillGarbage(rr, 0x96EFD0, 64); for (int i = 0; i < 64; ++i) reinterpret_cast<uint8_t*>(0x96EFD0)[i] &= 1; });
        IC(P.MarkRegionsForCoors(pos, rad); return 0;, PFn<void>(0x44DB60, pos.x, pos.y, pos.z, rad); return 0;);
    } ENDROW;
    ROW("CPathFind::ThisNodeWillLeadIntoADeadEnd", 0x44D310, 600) {
        const int a = r.below(64), n = r.below(NNODE), a2 = r.below(64), n2 = r.below(NNODE);
        IC(return B8(P.ThisNodeWillLeadIntoADeadEnd(NodeP(a, n), NodeP(a2, n2)));, return (uint64_t)PFn<uint8_t>(0x44D310, (void*)NodeP(a, n), (void*)NodeP(a2, n2)););
    } ENDROW;
    // ---- switching roads on / off
    ROW("CPathFind::SwitchOffNodeAndNeighbours", 0x452160, 800) {
        const int a = r.below(64), n = r.below(NNODE), sw = r.below(2), back = r.below(2), useOut2 = r.below(5) != 0;
        IC(CPathNode* o1 = reinterpret_cast<CPathNode*>(0x1111); CPathNode* o2 = reinterpret_cast<CPathNode*>(0x2222); P.SwitchOffNodeAndNeighbours(NodeP(a, n), o1, useOut2 ? &o2 : nullptr, sw != 0, back != 0); return (uint64_t)P32(o1) ^ ((uint64_t)P32(o2) << 20);,
           CPathNode* o1 = reinterpret_cast<CPathNode*>(0x1111); CPathNode* o2 = reinterpret_cast<CPathNode*>(0x2222); PFn<void>(0x452160, (void*)NodeP(a, n), (void*)&o1, (void*)(useOut2 ? &o2 : nullptr), (uint32_t)sw, (uint32_t)back); return (uint64_t)P32(o1) ^ ((uint64_t)P32(o2) << 20););
    } ENDROW;
    const auto Box = [](Rng& rr, float* b) {   // x0 x1 y0 y1 z0 z1 around a random node (so some nodes are inside)
        const CVector p = NodeP(rr.below(64), rr.below(NNODE))->GetPosition();
        const float wx = rr.below(4) ? rr.f01() * 400.f : 3000.f, wy = rr.below(4) ? rr.f01() * 400.f : 3000.f, wz = rr.below(3) ? 90.f : 5.f;
        b[0] = p.x - wx * rr.f01(); b[1] = p.x + wx * rr.f01(); b[2] = p.y - wy * rr.f01(); b[3] = p.y + wy * rr.f01(); b[4] = -wz * 1.2f; b[5] = wz * 1.2f;
        if (rr.below(12) == 0) b[rr.below(6)] = Fl(rr, 3000.f, 1);
    };
    ROW("CPathFind::SwitchRoadsOffInAreaForOneRegion", 0x452820, 700) {
        float b[6]; const int sw = r.below(2), cars = r.below(2), back = r.below(2), area = r.below(64);
        Tw tw([&](Rng& rr) { Box(rr, b); });
        IC(P.SwitchRoadsOffInAreaForOneRegion(b[0], b[1], b[2], b[3], b[4], b[5], sw != 0, cars != 0, area, back != 0); return 0;, PFn<void>(0x452820, b[0], b[1], b[2], b[3], b[4], b[5], (uint32_t)sw, (uint32_t)cars, area, (uint32_t)back); return 0;);
    } ENDROW;
    ROW("CPathFind::SwitchRoadsOffInArea", 0x452C80, 700) {
        float b[6]; const int sw = r.below(2), cars = r.below(2), back = r.below(2);
        Tw tw([&](Rng& rr) {
            Box(rr, b);
            P.m_nNumNodeSwitches = rr.below(65);
            for (int i = 0; i < 64; ++i) { float* sb = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(&P.m_aNodeSwitches[0]) + 0x1C * (size_t)i); for (int k = 0; k < 6; ++k) sb[k] = rr.below(3) ? b[k] + (rr.f01() - 0.5f) * 50.f * (k & 1 ? 1 : -1) : FlP(rr, 3000.f); reinterpret_cast<uint8_t*>(sb)[0x18] = (uint8_t)rr.below(2); reinterpret_cast<uint8_t*>(sb)[0x19] = (uint8_t)rr.below(2); }
        });
        IC(P.SwitchRoadsOffInArea(b[0], b[1], b[2], b[3], b[4], b[5], sw != 0, cars != 0, back != 0); return 0;, PFn<void>(0x452C80, b[0], b[1], b[2], b[3], b[4], b[5], (uint32_t)sw, (uint32_t)cars, (uint32_t)back); return 0;);
    } ENDROW;
    ROW("CPathFind::SwitchPedRoadsOffInArea", 0x452F00, 400) {
        float b[6]; const int sw = r.below(2), back = r.below(2);
        Tw tw([&](Rng& rr) { Box(rr, b); P.m_nNumNodeSwitches = rr.below(65); });
        IC(P.SwitchPedRoadsOffInArea(b[0], b[1], b[2], b[3], b[4], b[5], sw != 0, back != 0); return 0;, PFn<void>(0x452F00, b[0], b[1], b[2], b[3], b[4], b[5], (uint32_t)sw, (uint32_t)back); return 0;);
    } ENDROW;
    ROW("CPathFind::RemoveBadStartNode", 0x44E4F0, 600) {
        const CVector pos = r.below(3) ? NodeP(r.below(64), r.below(NNODE))->GetPosition() + CVector(FlP(r, 30.f), FlP(r, 30.f), 0.f) : Pos(r);
        const int num = r.below(6) ? 2 + (int)r.below(4) : (int)r.below(8);
        Tw tw([](Rng& rr) { for (int k = 0; k < 8; ++k) { const uint32_t v = NA(rr.below(rr.below(8) ? 64 : NAREA), rr.below(NNODE)); std::memcpy(S(oBuf + 0x40 + 4 * (size_t)k), &v, 4); } *reinterpret_cast<int16_t*>(S(oBuf + 0x80)) = 0; });
        IC(*reinterpret_cast<int16_t*>(S(oBuf + 0x80)) = (int16_t)num; P.RemoveBadStartNode(pos, reinterpret_cast<CNodeAddress*>(S(oBuf + 0x40)), reinterpret_cast<int16_t*>(S(oBuf + 0x80))); return 0;,
           *reinterpret_cast<int16_t*>(S(oBuf + 0x80)) = (int16_t)num; PFn<void>(0x44E4F0, pos.x, pos.y, pos.z, (void*)S(oBuf + 0x40), (void*)S(oBuf + 0x80)); return 0;);
    } ENDROW;
}
#undef P
