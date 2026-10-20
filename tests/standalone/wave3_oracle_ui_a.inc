// ORACLE-W3-UI rows A: Messages.cpp (CMessages brief / big / previous-brief arrays). See wave3_oracle_ui.inc.
namespace w3ua {
using namespace w3u; using namespace w2;
static GxtChar* MsgStr(Rng& r) { return PickStr(r); }
static void PutMsg(Rng& r, tMessage& m, bool valid) {
    FillGarbage(r, &m, sizeof m);
    m.Text = valid ? MsgStr(r) : nullptr;
    m.StringToInsert = r.below(3) ? nullptr : MsgStr(r);
    m.VisibleTime = r.below(6) == 0 ? r.u32() : r.below(8000);
    m.CreatedAtMS = r.below(6) == 0 ? r.u32() : CTimer::m_snTimeInMilliseconds - r.below(9000);
    for (auto& n : m.NumbersToInsert) n = r.below(3) ? (int32)r.below(2000) - 5 : (int32)r.u32();
    m.Flags = (uint16)r.u32(); m.PreviousBrief = (uint8)r.below(2);
}
template<size_t N> static void PutArr(Rng& r, std::array<tMessage, N>& a) {
    const int k = r.below(4) == 0 ? (int)N : (int)r.below(N + 1);
    for (size_t i = 0; i < N; ++i) PutMsg(r, a[i], (int)i < k);
}
static void PutPrev(Rng& r, tPreviousBrief& p, bool valid) {
    FillGarbage(r, &p, sizeof p);
    p.Text = valid ? MsgStr(r) : nullptr;
    p.StringToInsert = r.below(3) ? nullptr : MsgStr(r);
    for (auto& n : p.NumbersToInsert) n = r.below(3) ? (int32)r.below(10) - 1 : (int32)r.u32();
}
static void PutAllMsgs(Rng& r) {
    CTimer::m_snTimeInMilliseconds = r.below(8) == 0 ? r.u32() : r.below(1000000);
    for (auto& b : CMessages::BIGMessages) PutArr(r, b.Stack);
    PutArr(r, CMessages::BriefMessages);
    const int k = r.below(4) == 0 ? 20 : (int)r.below(21);
    for (int i = 0; i < 20; ++i) PutPrev(r, CMessages::PreviousBriefs[i], i < k);
}
// dead data: a message whose Text is null is unused (the exe clears only the text pointer, the port wipes the whole entry), and the padding bytes of a live one (after Flags, after PreviousBrief) have no reader
static void NormMsgs() {
    auto nm = [](tMessage& m) { auto* b = reinterpret_cast<uint8_t*>(&m); if (!m.Text) { std::memset(b, 0, sizeof m); return; } b[2] = b[3] = 0; b[0x2D] = b[0x2E] = b[0x2F] = 0; };
    for (auto& b : CMessages::BIGMessages) for (auto& m : b.Stack) { nm(m); reinterpret_cast<uint8_t*>(&m)[0x2C] = 0; }   // the PreviousBrief byte of a big message is never read (the exe adders do not write it)
    for (auto& m : CMessages::BriefMessages) nm(m);
    for (auto& p : CMessages::PreviousBriefs) if (!p.Text) std::memset(&p, 0, sizeof p);
}
static void CutAtNul() { uint8_t* o = S(oOut); size_t i = 0; while (i < 0x400 && o[i]) ++i; if (i < 0x400) std::memset(o + i, 0, 0x400 - i); }   // the bytes after the terminator are dead data
static void PutHudStrings(Rng& r) {
    FillGarbage(r, CHud::m_BigMessage, sizeof(CHud::m_BigMessage));
    for (auto& f : CHud::BigMessageX) f = Fl(r, 600.f, 6);
}
} // namespace w3ua

static void TestW3UiA() {
    using namespace w2; using namespace w3u; using namespace w3ua;
    ROW("CMessages::Init", 0x69EE00, 100) {
        Tu tu([](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::Init(); NormMsgs(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x69EE00)(); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::ClearMessages", 0x69DCD0, 200) {
        const bool flag = r.below(2);
        Tu tu([](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::ClearMessages(flag); NormMsgs(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x69DCD0)(flag ? 1 : 0); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::ClearSmallMessagesOnly", 0x69DD30, 100) {
        Tu tu([](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::ClearSmallMessagesOnly(); NormMsgs(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x69DD30)(); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::ClearPreviousBriefArray", 0x69DE70, 100) {
        Tu tu([](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::ClearPreviousBriefArray(); NormMsgs(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x69DE70)(); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::AddToPreviousBriefArray", 0x69DD50, 1500) {
        const GxtChar* text = PickStr(r); const GxtChar* str = r.below(2) ? nullptr : PickStr(r);
        int32 n[6]; for (auto& x : n) x = r.below(3) ? -1 : (int32)r.below(10);
        const int dup = (int)r.below(4);   // 0: an existing entry equals the new one (at a random slot), else random
        const int dupAt = (int)r.below(22);
        Tu tu([&](Rng& rr) {
            PutAllMsgs(rr);
            if (dup == 0 && dupAt < 20) {
                auto& p = CMessages::PreviousBriefs[dupAt]; p.Text = text; p.StringToInsert = str; for (int i = 0; i < 6; ++i) p.NumbersToInsert[i] = n[i];
            }
        });
        return UCase(r, d, [&]() -> uint64_t { CMessages::AddToPreviousBriefArray(text, n[0], n[1], n[2], n[3], n[4], n[5], str); NormMsgs(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, int, int, int, int, int, int, const GxtChar*)>(0x69DD50)(text, n[0], n[1], n[2], n[3], n[4], n[5], str); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::ClearThisPrint", 0x69EA30, 1500) {
        const GxtChar* text = r.below(10) == 0 ? Str(3) : PickStr(r);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::ClearThisPrint(text); NormMsgs(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*)>(0x69EA30)(text); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::ClearThisBigPrint", 0x69EBE0, 1500) {
        const GxtChar* text = r.below(10) == 0 ? Str(3) : PickStr(r);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::ClearThisBigPrint(text); NormMsgs(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*)>(0x69EBE0)(text); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::ClearThisPrintBigNow", 0x69ED80, 700) {
        const int style = (int)r.below(NUM_MESSAGE_STYLES);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); PutHudStrings(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::ClearThisPrintBigNow((eMessageStyle)style); NormMsgs(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x69ED80)(style); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::ClearAllMessagesDisplayedByGame", 0x69EDC0, 200) {
        const bool flag = r.below(2);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); PutHudStrings(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::ClearAllMessagesDisplayedByGame(flag); NormMsgs(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x69EDC0)(flag ? 1 : 0); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::Process", 0x69EE60, 1500) {
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::Process(); NormMsgs(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x69EE60)(); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::InsertStringInString", 0x69E040, 1500) {
        GxtChar* into = S(oOut); GxtChar* rep = r.below(12) == 0 ? nullptr : PickStr(r);
        const int srcK = (int)r.below(kNStr);
        Tu tu([&](Rng& rr) { std::memset(S(oOut), 0xA5, 0x400); std::strcpy((char*)S(oOut), kStr[srcK]); (void)rr; });
        return UCase(r, d, [&]() -> uint64_t { CMessages::InsertStringInString(into, rep); CutAtNul(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(GxtChar*, const GxtChar*)>(0x69E040)(into, rep); CutAtNul(); return 0; });
    } ENDROW;
    // brief / big message adders (class C in the coverage map)
    ROW("CMessages::AddMessageQ", 0x69F0B0, 1500) {
        const GxtChar* text = PickStr(r); const uint32_t tm = r.below(8000); const uint16_t fl = (uint16_t)r.below(8); const bool pb = r.below(2);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::AddMessageQ(text, tm, fl, pb); NormMsgs(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, uint32_t, int, int)>(0x69F0B0)(text, tm, fl, pb ? 1 : 0); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::AddMessageJump", 0x69F1E0, 1500) {
        const GxtChar* text = PickStr(r); const uint32_t tm = r.below(8000); const uint16_t fl = (uint16_t)r.below(8); const bool pb = r.below(2);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::AddMessageJump(text, tm, fl, pb); NormMsgs(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, uint32_t, int, int)>(0x69F1E0)(text, tm, fl, pb ? 1 : 0); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::AddMessageWithStringQ", 0x69E800, 1500) {
        const GxtChar* text = PickStr(r); GxtChar* s = r.below(4) ? PickStr(r) : nullptr; const uint32_t tm = r.below(8000); const uint16_t fl = (uint16_t)r.below(8); const bool pb = r.below(2);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::AddMessageWithStringQ(text, tm, fl, s, pb); NormMsgs(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, uint32_t, int, GxtChar*, int)>(0x69E800)(text, tm, fl, s, pb ? 1 : 0); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::AddMessageJumpQWithString", 0x69E950, 1500) {
        const GxtChar* text = PickStr(r); GxtChar* s = r.below(4) ? PickStr(r) : nullptr; const uint32_t tm = r.below(8000); const uint16_t fl = (uint16_t)r.below(8); const bool pb = r.below(2);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::AddMessageJumpQWithString(text, tm, fl, s, pb); NormMsgs(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, uint32_t, int, GxtChar*, int)>(0x69E950)(text, tm, fl, s, pb ? 1 : 0); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::AddMessageWithNumberQ", 0x69E360, 1500) {
        const GxtChar* text = PickStr(r); const uint32_t tm = r.below(8000); const uint16_t fl = (uint16_t)r.below(8); const bool pb = r.below(2);
        int32 n[6]; for (auto& x : n) x = r.below(3) ? -1 : (int32)r.below(100000);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::AddMessageWithNumberQ(text, tm, fl, n[0], n[1], n[2], n[3], n[4], n[5], pb); NormMsgs(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, uint32_t, int, int, int, int, int, int, int, int)>(0x69E360)(text, tm, fl, n[0], n[1], n[2], n[3], n[4], n[5], pb ? 1 : 0); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::AddMessageJumpQWithNumber", 0x69E4E0, 1500) {
        const GxtChar* text = PickStr(r); const uint32_t tm = r.below(8000); const uint16_t fl = (uint16_t)r.below(8); const bool pb = r.below(2);
        int32 n[6]; for (auto& x : n) x = r.below(3) ? -1 : (int32)r.below(100000);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::AddMessageJumpQWithNumber(text, tm, fl, n[0], n[1], n[2], n[3], n[4], n[5], pb); NormMsgs(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, uint32_t, int, int, int, int, int, int, int, int)>(0x69E4E0)(text, tm, fl, n[0], n[1], n[2], n[3], n[4], n[5], pb ? 1 : 0); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::AddBigMessageQ", 0x69F370, 1500) {
        const GxtChar* text = PickStr(r); const uint32_t tm = r.below(8000); const int style = (int)r.below(NUM_MESSAGE_STYLES);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::AddBigMessageQ(text, tm, (eMessageStyle)style); NormMsgs(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, uint32_t, int)>(0x69F370)(text, tm, style); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::AddBigMessageWithNumber", 0x69E5F0, 1500) {
        const GxtChar* text = PickStr(r); const uint32_t tm = r.below(8000); const int style = (int)r.below(NUM_MESSAGE_STYLES);
        int32 n[6]; for (auto& x : n) x = r.below(3) ? -1 : (int32)r.below(100000);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::AddBigMessageWithNumber(text, tm, (eMessageStyle)style, n[0], n[1], n[2], n[3], n[4], n[5]); NormMsgs(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, uint32_t, int, int, int, int, int, int, int)>(0x69E5F0)(text, tm, style, n[0], n[1], n[2], n[3], n[4], n[5]); NormMsgs(); return 0; });
    } ENDROW;
    ROW("CMessages::AddBigMessageWithNumberQ", 0x69E6E0, 1500) {
        const GxtChar* text = PickStr(r); const uint32_t tm = r.below(8000); const int style = (int)r.below(NUM_MESSAGE_STYLES);
        int32 n[6]; for (auto& x : n) x = r.below(3) ? -1 : (int32)r.below(100000);
        Tu tu([&](Rng& rr) { PutAllMsgs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CMessages::AddBigMessageWithNumberQ(text, tm, (eMessageStyle)style, n[0], n[1], n[2], n[3], n[4], n[5]); NormMsgs(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, uint32_t, int, int, int, int, int, int, int)>(0x69E6E0)(text, tm, style, n[0], n[1], n[2], n[3], n[4], n[5]); NormMsgs(); return 0; });
    } ENDROW;
}
