// ORACLE-W3-INPUT rows B: ControllerConfigManager.cpp (see wave3_oracle_input.inc). The manager is the exe's global (ControlsManager, 0xB70198) with a populated action / key table.
// the exe's GTATranslateShiftKey (0x747CD0, GetKeyState through the IAT) and WinInput::GetMouseSetUp (0x53F2D0, DirectInput) are device calls. The SDL3 port build has neither (AffectPadFromKeyBoard
// skips the shift translation, ReinitControls forces every mouse button on, isForcedMouseBlinding): the exe runs host stubs with the same effect (nothing / a set-up with all seven buttons present)
static CMouseControllerState* __stdcall X_GetMouseSetUp(CMouseControllerState* out) { *out = CMouseControllerState{}; std::memset(out, 1, 7); return out; }   // the exe: out pointer on the stack, `ret 4`
static BOOL __cdecl X_TranslateShiftKey(RsKeyCodes*) { return TRUE; }
static void InstallStubsI() { static bool done = false; if (done) return; done = true; oracle::Patch(0x53F2D0, (void*)&X_GetMouseSetUp); oracle::Patch(0x747CD0, (void*)&X_TranslateShiftKey); }
// protected / private members of the manager: access trick (no layout assumption, no `#define private public`)
#define W3I_ROB(NAME, SIG) struct W3iTag_##NAME { using type = SIG; friend type W2Get(W3iTag_##NAME); }; template struct W2Rob<W3iTag_##NAME, &CControllerConfigManager::NAME>;
W3I_ROB(GetIsKeyBlank, bool (CControllerConfigManager::*)(KeyCode, eControllerType))
W3I_ROB(GetIsKeyboardKeyJustDown, bool (CControllerConfigManager::*)(KeyCode))
W3I_ROB(DeleteMatchingCommonControls, void (CControllerConfigManager::*)(eControllerAction, KeyCode, eControllerType))
W3I_ROB(DeleteMatching3rdPersonControls, void (CControllerConfigManager::*)(eControllerAction, KeyCode, eControllerType))
W3I_ROB(DeleteMatchingVehicleControls, void (CControllerConfigManager::*)(eControllerAction, KeyCode, eControllerType))
W3I_ROB(DeleteMatchingVehicle_3rdPersonControls, void (CControllerConfigManager::*)(eControllerAction, KeyCode, eControllerType))   // (the exe takes three: ret 0xC, the first one unused)
W3I_ROB(DeleteMatching1rstPersonControls, void (CControllerConfigManager::*)(eControllerAction, KeyCode, eControllerType))
W3I_ROB(DeleteMatching1rst3rdPersonControls, void (CControllerConfigManager::*)(eControllerAction, KeyCode, eControllerType))
#define CCM_CALL(NAME, ...) (ControlsManager.*W2Get(W3iTag_##NAME{}))(__VA_ARGS__)

static_assert(offsetof(CMenuManager, m_ControlMethod) == 0xD0, "the exe tests byte 0xBA6818 (FrontEndMenuManager + 0xD0)");
static_assert(offsetof(CMenuManager, m_nTextLanguage) == 0x80, "the exe tests dword 0xBA67C8 (FrontEndMenuManager + 0x80)");
namespace w3ib {
using namespace w3i; using namespace w2;
constexpr int kActions = (int)eControllerAction::CONTROLLER_ACTION_COUNT;
static KeyCode KbKey(Rng& r) {
    switch (r.below(10)) {
    case 0: case 1: case 2: return (KeyCode)rsNULL;                                   // blank
    case 3: case 4: case 5: return (KeyCode)(rsESC + (int)r.below(rsAPPS - rsESC + 1)); // special keys (F keys, arrows, pad keys ...)
    case 6: case 7: return (KeyCode)("AWSDQEZXCVFGHNRY 01234"[r.below(22)]);          // ASCII
    case 8: return (KeyCode)r.below(256);
    default: return (KeyCode)(rsESC + (int)r.below(60));
    }
}
static KeyCode MsKey(Rng& r) { return r.below(4) == 0 ? 0 : (KeyCode)(1 + r.below(7)); }
static KeyCode JoyKey(Rng& r) { return r.below(4) == 0 ? 0 : (KeyCode)(1 + r.below(16)); }
static KeyCode KeyFor(Rng& r, int type) { return type < 2 ? KbKey(r) : type == 2 ? MsKey(r) : JoyKey(r); }
// a key that is in the table (so the "equal key" paths run), else a random one
static KeyCode PickKey(Rng& r, int type) {
    if (r.below(10) < 7) {
        const auto& c = ControlsManager.m_Actions[r.below(kActions)].Keys[type];
        if (r.below(8) != 0) return c.m_uiActionInitiator;
    }
    return KeyFor(r, type);
}
struct PwParams { bool inVeh = false, driving = false; };
static PwParams g_pw;
// the image part of the fixture: table, joystick state, keyboard / mouse state words (0 or 255 like the real handler writes), menu / camera / joystick globals
static void CtlTable(Rng& r) {
    PadWorld(r);
    auto& c = ControlsManager;
    FillGarbage(r, &c, sizeof c);
    for (int a = 0; a < kActions; ++a) {
        for (int t = 0; t < 4; ++t) {
            c.m_Actions[a].Keys[t].m_uiActionInitiator = KeyFor(r, t);
            c.m_Actions[a].Keys[t].m_uiSetOrder = (eContSetOrder)(r.below(3) ? r.below(5) : r.below(7));
        }
        if (r.below(12) == 0) {   // a clean row: no key, no order
            for (int t = 0; t < 4; ++t) { c.m_Actions[a].Keys[t].m_uiActionInitiator = t < 2 ? (KeyCode)rsNULL : 0; c.m_Actions[a].Keys[t].m_uiSetOrder = eContSetOrder::NONE; }
        }
    }
    c.m_bJoyJustInitialised = r.below(2) != 0; c.m_MouseFoundInitSet = r.below(2) != 0;
    for (auto& b : c.m_ButtonStates) b = r.below(2) != 0;
    for (int t = 0; t < 4; ++t) { c.m_bStickL_X_Rgh_Lft_MovementBothDown[t] = r.below(2) != 0; c.m_bStickL_Up_Dwn_MovementBothDown[t] = r.below(2) != 0; c.m_bStickR_X_Rgh_Lft_MovementBothDown[t] = r.below(2) != 0; c.m_bStickR_Up_Dwn_MovementBothDown[t] = r.below(2) != 0; }
    for (int k = 0; k < 32; ++k) { c.m_NewJoyState.rgbButtons[k] = r.below(3) == 0 ? (BYTE)0x80 : (BYTE)(r.below(2) ? 0 : r.u32()); c.m_OldJoyState.rgbButtons[k] = r.below(2) ? c.m_NewJoyState.rgbButtons[k] : (BYTE)(r.below(3) == 0 ? 0x80 : 0); }
    // keyboard state globals: int16 words, 0 or 255 (half of the keys down); mouse state globals: bool bytes
    for (uintptr_t va = 0xB72CB0; va < 0xB72CB0 + 3 * 0x270; va += 2) At<int16_t>(va) = r.below(2) ? 255 : 0;
    for (int k = 0; k < 3; ++k) for (uintptr_t va = 0xB73404 + 0x14 * k; va < 0xB73404 + 0x14 * k + 7; ++va) At<uint8_t>(va) = r.below(2) ? 1 : 0;
    // the menu / camera / joystick world the press handlers read
    FrontEndMenuManager.m_ControlMethod = r.below(2) ? eController::JOYPAD : eController::MOUSE_PLUS_KEYS;
    FrontEndMenuManager.m_bMenuActive = r.below(4) == 0; FrontEndMenuManager.m_nTextLanguage = (int32_t)r.below(5);
    AllValidWinJoys = {};
    AllValidWinJoys.JoyStickNum[0].bJoyAttachedToPort = r.below(2) != 0;
    if (r.below(4) == 0) { AllValidWinJoys.JoyStickNum[0].wVendorID = 0x3427; AllValidWinJoys.JoyStickNum[0].wProductID = 0x1190; } else { AllValidWinJoys.JoyStickNum[0].wVendorID = r.u32() & 0xFFFF; AllValidWinJoys.JoyStickNum[0].wProductID = r.u32() & 0xFFFF; }
    g_pw.inVeh = r.below(2) != 0; g_pw.driving = r.below(3) != 0;
    TheCamera.m_nActiveCam = (uint8_t)r.below(3);
    TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode = r.below(3) == 0 ? (eCamMode)r.below(70) : (r.below(2) ? MODE_1STPERSON : (eCamMode)(MODE_1STPERSON + 20 + r.below(30)));
    if (r.below(10) == 0) TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode = MODE_CAMERA;
}
// the scratch part (fake player ped / vehicle): runs after the scratch block was refilled
static void CtlScratch(Rng& r) { PlayerWorld(r, g_pw.inVeh, g_pw.driving, false); }
static int Act(Rng& r) { return r.below(40) == 0 ? (int)r.below(kActions + 4) - 2 : (int)r.below(kActions); }   // mostly valid ids; a few -2..-1 / just past the end (the table is followed by the joystick-state bytes)
static int ActV(Rng& r) { return (int)r.below(kActions); }
static int Ty(Rng& r) { return (int)r.below(4); }
} // namespace w3ib

#define CCROW(NAME, ADDR, CASES) ROW(NAME, ADDR, CASES) { using namespace w3ib; CtlTable(r); Tu tu([](Rng& rr) { CtlScratch(rr); });

static void TestW3InputB() {
    using namespace w2; using namespace w3i; using namespace w3ib;
    InstallStubsI();
    auto& C = ControlsManager;
    void* const self = &ControlsManager;
    CControllerState* const st = reinterpret_cast<CControllerState*>(S(oStA));
    // ---- joystick button edges, state copies
    CCROW("CControllerConfigManager::GetJoyButtonJustUp", 0x52D1C0, 300)
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)C.GetJoyButtonJustUp(); }, [&]() -> uint64_t { return ExeThis<uint32_t>(0x52D1C0, self); });
    } ENDROW;
    CCROW("CControllerConfigManager::GetJoyButtonJustDown", 0x52D1E0, 300)
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)C.GetJoyButtonJustDown(); }, [&]() -> uint64_t { return ExeThis<uint32_t>(0x52D1E0, self); });
    } ENDROW;
    CCROW("CControllerConfigManager::UpdateJoyButtonState", 0x52F510, 200)
        const int pad = (int)r.below(2);
        return UCase(r, d, [&]() -> uint64_t { C.UpdateJoyButtonState(pad); return 0; }, [&]() -> uint64_t { ExeThis<void, int>(0x52F510, self, pad); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::InitialiseControllerActionNameArray", 0x52D260, 100)
        return UCase(r, d, [&]() -> uint64_t { C.InitialiseControllerActionNameArray(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x52D260, self); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::ClearSimButtonPressCheckers", 0x52DD90, 100)
        return UCase(r, d, [&]() -> uint64_t { C.ClearSimButtonPressCheckers(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x52DD90, self); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::MakeControllerActionsBlank", 0x530500, 100)
        return UCase(r, d, [&]() -> uint64_t { C.MakeControllerActionsBlank(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x530500, self); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::InitDefaultControlConfiguration", 0x530640, 200)
        return UCase(r, d, [&]() -> uint64_t { C.InitDefaultControlConfiguration(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x530640, self); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::Constructor", 0x531EE0, 100)
        return UCase(r, d, [&]() -> uint64_t { ::new (self) CControllerConfigManager(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x531EE0, self); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::InitDefaultControlConfigJoyPad", 0x530B00, 800)
        const uint32_t cnt = r.below(4) == 0 ? r.below(40) : r.below(18);
        return UCase(r, d, [&]() -> uint64_t { C.InitDefaultControlConfigJoyPad(cnt); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t>(0x530B00, self, cnt); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::InitDefaultControlConfigMouse", 0x52F6F0, 800)
        CMouseControllerState* ms = reinterpret_cast<CMouseControllerState*>(S(oOut));
        const bool force = r.below(2) != 0;
        for (size_t i = 0; i < 7; ++i) reinterpret_cast<uint8_t*>(ms)[i] = 1;   // the SDL3 port always binds every button (isForcedMouseBlinding): a set-up with all buttons present is the case both sides agree on
        const std::vector<uint8_t> init(reinterpret_cast<uint8_t*>(ms), reinterpret_cast<uint8_t*>(ms) + sizeof(CMouseControllerState));
        const bool wasInit = C.m_MouseFoundInitSet;
        (void)wasInit;
        return UCase(r, d, [&]() -> uint64_t { std::memcpy(ms, init.data(), init.size()); C.InitDefaultControlConfigMouse(*ms, force); return 0; },
            [&]() -> uint64_t { std::memcpy(ms, init.data(), init.size()); ExeThis<void, void*, int>(0x52F6F0, self, ms, (int)force); return 0; });
    } ENDROW;
    // ---- queries on the table
    CCROW("CControllerConfigManager::GetActionType", 0x52F2F0, 800)
        const int a = (int)r.below(kActions + 8) - 4;
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)C.GetActionType((eControllerAction)a); }, [&]() -> uint64_t { return ExeThis<uint32_t, int>(0x52F2F0, self, a); });
    } ENDROW;
    CCROW("CControllerConfigManager::GetNumOfSettingsForAction", 0x52F4A0, 1000)
        const int a = ActV(r);
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)C.GetNumOfSettingsForAction((eControllerAction)a); }, [&]() -> uint64_t { return ExeThis<uint32_t, int>(0x52F4A0, self, a); });
    } ENDROW;
    CCROW("CControllerConfigManager::GetControllerKeyAssociatedWithAction", 0x52F4F0, 1000)
        const int a = ActV(r), t = Ty(r);
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)C.GetControllerKeyAssociatedWithAction((eControllerAction)a, (eControllerType)t); }, [&]() -> uint64_t { return ExeThis<uint32_t, int, int>(0x52F4F0, self, a, t); });
    } ENDROW;
    CCROW("CControllerConfigManager::GetIsActionAButtonCombo", 0x52F550, 100)
        const int a = ActV(r);
        return UCase(r, d, [&]() -> uint64_t { return B8(C.GetIsActionAButtonCombo((eControllerAction)a)); }, [&]() -> uint64_t { return ExeThis<uint32_t, int>(0x52F550, self, a) & 0xFF; });
    } ENDROW;
    CCROW("CControllerConfigManager::GetMouseButtonAssociatedWithAction", 0x52F580, 500)
        const int a = ActV(r);
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)C.GetMouseButtonAssociatedWithAction((eControllerAction)a); }, [&]() -> uint64_t { return ExeThis<uint32_t, int>(0x52F580, self, a); });
    } ENDROW;
    CCROW("CControllerConfigManager::GetIsKeyBlank", 0x52F2A0, 1500)
        const int t = Ty(r); const KeyCode k = r.below(2) ? KeyFor(r, t) : PickKey(r, t);
        return UCase(r, d, [&]() -> uint64_t { return B8(CCM_CALL(GetIsKeyBlank, k, (eControllerType)t)); }, [&]() -> uint64_t { return ExeThis<uint32_t, uint32_t, int>(0x52F2A0, self, k, t) & 0xFF; });
    } ENDROW;
    // ---- keyboard / mouse state queries
    CCROW("CControllerConfigManager::GetIsKeyboardKeyDown", 0x52DDB0, 3000)
        const KeyCode k = r.below(8) ? PickKey(r, (int)r.below(2)) : (KeyCode)(r.u32() & 0xFFFF);
        return UCase(r, d, [&]() -> uint64_t { return B8(C.GetIsKeyboardKeyDown(k)); }, [&]() -> uint64_t { return ExeThis<uint32_t, uint32_t>(0x52DDB0, self, k) & 0xFF; });
    } ENDROW;
    CCROW("CControllerConfigManager::GetIsKeyboardKeyJustDown", 0x52E450, 3000)
        const KeyCode k = r.below(8) ? PickKey(r, (int)r.below(2)) : (KeyCode)(r.u32() & 0xFFFF);
        return UCase(r, d, [&]() -> uint64_t { return B8(CCM_CALL(GetIsKeyboardKeyJustDown, k)); }, [&]() -> uint64_t { return ExeThis<uint32_t, uint32_t>(0x52E450, self, k) & 0xFF; });
    } ENDROW;
    {
        struct Mf { const char* nm; uint32_t addr; bool (CControllerConfigManager::*pf)(KeyCode); };
        static const Mf mfs[] = { { "CControllerConfigManager::GetIsMouseButtonDown", 0x52EF30, &CControllerConfigManager::GetIsMouseButtonDown },
            { "CControllerConfigManager::GetIsMouseButtonUp", 0x52F020, &CControllerConfigManager::GetIsMouseButtonUp }, { "CControllerConfigManager::GetIsMouseButtonJustUp", 0x52F110, &CControllerConfigManager::GetIsMouseButtonJustUp } };
        for (const auto& m : mfs) {
            const std::string nm = std::string(m.nm) + " 0x" + [&] { char b[16]; std::snprintf(b, sizeof b, "%X", m.addr); return std::string(b); }();
            RunScaledI(nm, [&](Rng& r, std::string& d) -> bool {
                CtlTable(r); Tu tu([](Rng& rr) { CtlScratch(rr); });
                const KeyCode k = (KeyCode)r.below(8);   // 0 = none; 1..7 = the mouse buttons / wheel (anything else is unreachable in the port)
                return UCase(r, d, [&]() -> uint64_t { return B8((C.*m.pf)(k)); }, [&]() -> uint64_t { return ExeThis<uint32_t, uint32_t>(m.addr, self, k) & 0xFF; });
            }, 1500);
        }
    }
    CCROW("CControllerConfigManager::StoreMouseButtonState", 0x52DA30, 700)
        const int b = 1 + (int)r.below(7); const bool s = r.below(2) != 0;
        return UCase(r, d, [&]() -> uint64_t { C.StoreMouseButtonState((RsKeyCodes)b, s); return 0; }, [&]() -> uint64_t { ExeThis<void, int, int>(0x52DA30, self, b, (int)s); return 0; });
    } ENDROW;
    // ---- table edits
    CCROW("CControllerConfigManager::ResetSettingOrder", 0x52F5F0, 2500)
        const int a = ActV(r);
        return UCase(r, d, [&]() -> uint64_t { C.ResetSettingOrder((eControllerAction)a); return 0; }, [&]() -> uint64_t { ExeThis<void, int>(0x52F5F0, self, a); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::SetMouseButtonAssociatedWithAction", 0x52F590, 1500)
        const int a = ActV(r); const KeyCode k = MsKey(r);
        return UCase(r, d, [&]() -> uint64_t { C.SetMouseButtonAssociatedWithAction((eControllerAction)a, k); return 0; }, [&]() -> uint64_t { ExeThis<void, int, uint32_t>(0x52F590, self, a, k); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::SetControllerKeyAssociatedWithAction", 0x530490, 2000)
        const int a = ActV(r), t = Ty(r); const KeyCode k = KeyFor(r, t);
        return UCase(r, d, [&]() -> uint64_t { C.SetControllerKeyAssociatedWithAction((eControllerAction)a, k, (eControllerType)t); return 0; }, [&]() -> uint64_t { ExeThis<void, int, uint32_t, int>(0x530490, self, a, k, t); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::ClearSettingsAssociatedWithAction", 0x52FD70, 2000)
        const int a = ActV(r), t = Ty(r);
        return UCase(r, d, [&]() -> uint64_t { C.ClearSettingsAssociatedWithAction((eControllerAction)a, (eControllerType)t); return 0; }, [&]() -> uint64_t { ExeThis<void, int, int>(0x52FD70, self, a, t); return 0; });
    } ENDROW;
    // ---- DeleteMatching* (action, button, type)
    {
        struct Dm { const char* nm; uint32_t addr; void (*port)(int, KeyCode, int); };
        static const Dm dms[] = {
            { "CControllerConfigManager::DeleteMatchingCommonControls", 0x531670, [](int a, KeyCode k, int t) { CCM_CALL(DeleteMatchingCommonControls, (eControllerAction)a, k, (eControllerType)t); } },
            { "CControllerConfigManager::DeleteMatching3rdPersonControls", 0x531730, [](int a, KeyCode k, int t) { CCM_CALL(DeleteMatching3rdPersonControls, (eControllerAction)a, k, (eControllerType)t); } },
            { "CControllerConfigManager::DeleteMatching1rst3rdPersonControls", 0x5318C0, [](int a, KeyCode k, int t) { CCM_CALL(DeleteMatching1rst3rdPersonControls, (eControllerAction)a, k, (eControllerType)t); } },
            { "CControllerConfigManager::DeleteMatchingVehicleControls", 0x5319D0, [](int a, KeyCode k, int t) { CCM_CALL(DeleteMatchingVehicleControls, (eControllerAction)a, k, (eControllerType)t); } },
            { "CControllerConfigManager::DeleteMatching1rstPersonControls", 0x531C20, [](int a, KeyCode k, int t) { CCM_CALL(DeleteMatching1rstPersonControls, (eControllerAction)a, k, (eControllerType)t); } },
            { "CControllerConfigManager::DeleteMatchingActionInitiators", 0x531C90, [](int a, KeyCode k, int t) { ControlsManager.DeleteMatchingActionInitiators((eControllerAction)a, k, (eControllerType)t); } },
        };
        for (const auto& m : dms) {
            const std::string nm = std::string(m.nm) + " 0x" + [&] { char b[16]; std::snprintf(b, sizeof b, "%X", m.addr); return std::string(b); }();
            RunScaledI(nm, [&](Rng& r, std::string& d) -> bool {
                CtlTable(r); Tu tu([](Rng& rr) { CtlScratch(rr); });
                const int a = ActV(r), t = Ty(r); const KeyCode k = PickKey(r, t);
                return UCase(r, d, [&]() -> uint64_t { m.port(a, k, t); return 0; }, [&]() -> uint64_t { ExeThis<void, int, uint32_t, int>(m.addr, self, a, k, t); return 0; });
            }, 3000);
        }
    }
    CCROW("CControllerConfigManager::DeleteMatchingVehicle_3rdPersonControls", 0x531BC0, 1500)
        const int t = Ty(r); const KeyCode k = PickKey(r, t);
        const int a = ActV(r);
        return UCase(r, d, [&]() -> uint64_t { CCM_CALL(DeleteMatchingVehicle_3rdPersonControls, (eControllerAction)a, k, (eControllerType)t); return 0; }, [&]() -> uint64_t { ExeThis<void, int, uint32_t, int>(0x531BC0, self, a, k, t); return 0; });
    } ENDROW;
    // ---- press handlers that edit a controller state (button, type, state&)
    {
        struct Ah { const char* nm; uint32_t addr; void (CControllerConfigManager::*pf)(KeyCode, eControllerType, CControllerState&); };
        static const Ah ahs[] = {
            { "CControllerConfigManager::AffectControllerStateOn_ButtonDown_Driving", 0x52F7B0, &CControllerConfigManager::AffectControllerStateOn_ButtonDown_Driving },
            { "CControllerConfigManager::AffectControllerStateOn_ButtonDown_FirstPersonOnly", 0x52F9E0, &CControllerConfigManager::AffectControllerStateOn_ButtonDown_FirstPersonOnly },
            { "CControllerConfigManager::AffectControllerStateOn_ButtonDown_ThirdPersonOnly", 0x52FA20, &CControllerConfigManager::AffectControllerStateOn_ButtonDown_ThirdPersonOnly },
            { "CControllerConfigManager::AffectControllerStateOn_ButtonDown_FirstAndThirdPersonOnly", 0x52FAB0, &CControllerConfigManager::AffectControllerStateOn_ButtonDown_FirstAndThirdPersonOnly },
            { "CControllerConfigManager::AffectControllerStateOn_ButtonDown_AllStates", 0x52FCA0, &CControllerConfigManager::AffectControllerStateOn_ButtonDown_AllStates },
            { "CControllerConfigManager::AffectControllerStateOn_ButtonDown_VehicleAndThirdPersonOnly", 0x52FD20, &CControllerConfigManager::AffectControllerStateOn_ButtonDown_VehicleAndThirdPersonOnly },
            { "CControllerConfigManager::AffectControllerStateOn_ButtonUp_All_Player_States", 0x52FD40, &CControllerConfigManager::AffectControllerStateOn_ButtonUp_All_Player_States },
        };
        for (const auto& m : ahs) {
            const std::string nm = std::string(m.nm) + " 0x" + [&] { char b[16]; std::snprintf(b, sizeof b, "%X", m.addr); return std::string(b); }();
            RunScaledI(nm, [&](Rng& r, std::string& d) -> bool {
                CtlTable(r); Tu tu([](Rng& rr) { CtlScratch(rr); FillState(rr, SAt<CControllerState>(oStA)); });
                const int t = Ty(r); const KeyCode k = PickKey(r, t);
                return UCase(r, d, [&]() -> uint64_t { (C.*m.pf)(k, (eControllerType)t, *st); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t, int, void*>(m.addr, self, k, t, st); return 0; });
            }, 3000);
        }
    }
    CCROW("CControllerConfigManager::AffectControllerStateOn_ButtonDown_DebugStuff", 0x52DC10, 20)
        const int t = Ty(r); const KeyCode k = KeyFor(r, t);
        return UCase(r, d, [&]() -> uint64_t { C.AffectControllerStateOn_ButtonDown_DebugStuff(k, (eControllerType)t); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t, int>(0x52DC10, self, k, t); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::AffectControllerStateOn_ButtonUp_DebugStuff", 0x52DD80, 20)
        const int t = Ty(r); const int k = (int)r.u32();
        return UCase(r, d, [&]() -> uint64_t { C.AffectControllerStateOn_ButtonUp_DebugStuff(k, (eControllerType)t); return 0; }, [&]() -> uint64_t { ExeThis<void, int, int>(0x52DD80, self, k, t); return 0; });
    } ENDROW;
    // ---- full press / release (reads the pad, the menu, the player and the camera)
    CCROW("CControllerConfigManager::UpdateJoy_ButtonDown", 0x530ED0, 3000)
        const int t = Ty(r); const KeyCode k = r.below(5) ? PickKey(r, t) : KeyFor(r, t);
        return UCase(r, d, [&]() -> uint64_t { C.UpdateJoy_ButtonDown(k, (eControllerType)t); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t, int>(0x530ED0, self, k, t); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::AffectControllerStateOn_ButtonUp", 0x531070, 3000)
        const int t = Ty(r); const KeyCode k = r.below(5) ? PickKey(r, t) : KeyFor(r, t);
        return UCase(r, d, [&]() -> uint64_t { C.AffectControllerStateOn_ButtonUp(k, (eControllerType)t); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t, int>(0x531070, self, k, t); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::UpdateJoyInConfigMenus_ButtonDown", 0x52DAB0, 1500)
        const int pad = (int)r.below(2); const KeyCode k = r.below(8) == 0 ? 0 : (KeyCode)(1 + r.below(16));
        return UCase(r, d, [&]() -> uint64_t { C.UpdateJoyInConfigMenus_ButtonDown(k, pad); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t, int>(0x52DAB0, self, k, pad); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::UpdateJoyInConfigMenus_ButtonUp", 0x52DC20, 1500)
        const int pad = (int)r.below(2); const KeyCode k = r.below(8) == 0 ? 0 : (KeyCode)(1 + r.below(16));
        return UCase(r, d, [&]() -> uint64_t { C.UpdateJoyInConfigMenus_ButtonUp(k, pad); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t, int>(0x52DC20, self, k, pad); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::AffectPadFromMouse", 0x5314A0, 1500)
        return UCase(r, d, [&]() -> uint64_t { C.AffectPadFromMouse(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x5314A0, self); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::AffectPadFromKeyBoard", 0x531140, 1500)
        return UCase(r, d, [&]() -> uint64_t { C.AffectPadFromKeyBoard(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x531140, self); return 0; });
    } ENDROW;
    CCROW("CControllerConfigManager::ReinitControls", 0x531F20, 200)
        AllValidWinJoys.JoyStickNum[0].bJoyAttachedToPort = false;   // (an attached pad: the exe asks DirectInput for the button count, the SDL3 port assumes 16)
        return UCase(r, d, [&]() -> uint64_t { C.ReinitControls(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x531F20, self); return 0; });
    } ENDROW;
}
