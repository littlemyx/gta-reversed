// ORACLE-W2: allocator-backed rows. Included by wave1_oracle_test.cpp when W2_ONLY is defined (executable wave2_oracle_test; the generated families live in wave2_oracle_*.inc).
#include "wave2_oracle_common.inc"

#ifdef W3_VEH_ONLY
#include "wave3_oracle_veh.inc"      // ORACLE-W3-VEH: vehicle rows (wave3_vehicle_oracle_test)
#elif defined(W3_PED_ONLY)
#include "wave3_oracle_ped.inc"      // ORACLE-W3-PED: ped rows (wave3_ped_oracle_test)
#elif defined(W3_EV_ONLY)
#include "wave3_oracle_ev.inc"        // ORACLE-W3-EVENTS: game_sa/Events rows (wave3_events_oracle_test)
#elif defined(W3_TASKS_ONLY)
#include "wave3_oracle_tasks.inc"     // ORACLE-W3-TASKS: game_sa/Tasks/TaskTypes rows (wave3_tasks_oracle_test)
#elif defined(W3_GP_ONLY)
#include "wave3_oracle_gp.inc"       // ORACLE-W3-GAMEPLAY: gameplay root-file rows (wave3_gameplay_oracle_test)
#elif defined(W3_ANIM_ONLY)
#include "wave3_oracle_anim.inc"     // ORACLE-W3-ANIM: game_sa/Animation + RpAnimBlendPlugin rows (wave3_anim_oracle_test)
#elif defined(W3_MODELS_ONLY)
#include "wave3_oracle_models.inc"   // ORACLE-W3-MODELS: game_sa/Models rows (wave3_models_oracle_test)
#elif defined(W3_AUDIO_ONLY)
#include "wave3_oracle_audio.inc"    // ORACLE-W3-AUDIO: game_sa/Audio/Entities + Audio root rows (wave3_audio_oracle_test)
#elif defined(W3_ENTITY_ONLY)
#include "wave3_oracle_entity.inc"    // ORACLE-W3-ENTITY: game_sa/Entity base-class rows (wave3_entity_oracle_test)
#elif defined(W3_INTERIOR_ONLY)
#include "wave3_oracle_interior.inc"  // ORACLE-W3-INTERIOR: game_sa/Interior rows (wave3_interior_oracle_test)
#elif defined(W3_CAMERA_ONLY)
#include "wave3_oracle_camera.inc"     // ORACLE-W3-CAMERA: camera root files rows (wave3_camera_oracle_test)
#elif defined(W3_WORLD_ONLY)
#include "wave3_oracle_world.inc"       // ORACLE-W3-WORLD: world / physics root-file rows (wave3_world_oracle_test)
#elif defined(W3_SCRIPTS_ONLY)
#include "wave3_oracle_scripts.inc"     // ORACLE-W3-SCRIPTS: game_sa/Scripts rows (wave3_scripts_oracle_test)
#elif defined(W3_UI_ONLY)
#include "wave3_oracle_ui.inc"          // ORACLE-W3-UI: HUD / radar / text root-file rows (wave3_ui_oracle_test)
#elif defined(W3_INPUT_ONLY)
#include "wave3_oracle_input.inc"        // ORACLE-W3-INPUT: ControllerConfigManager + Pad rows (wave3_input_oracle_test)
#elif defined(W3_AI_ONLY)
#include "wave3_oracle_ai.inc"          // ORACLE-W3-AI: CarCtrl / PedIntelligence / Cover / Object rows (wave3_ai_oracle_test)
#elif defined(W3_FX_ONLY)
#include "wave3_oracle_fx.inc"          // ORACLE-W3-EFFECTS: explosion / fire / glass / rope / crane / plant ... root-file rows (wave3_fx_oracle_test)
#else
#if __has_include("wave2_oracle_gen_ctor.inc")
#include "wave2_oracle_gen_ctor.inc"      // GENERATED (tools/gen_w2_rows.py): task / event constructors
#define W2_HAVE_CTOR 1
#endif
#if __has_include("wave2_oracle_gen_clone.inc")
#include "wave2_oracle_gen_clone.inc"     // GENERATED: Clone / CloneEditable
#define W2_HAVE_CLONE 1
#endif

#if __has_include("wave2_oracle_gen_func.inc")
#include "wave2_oracle_gen_func.inc"      // GENERATED (tools/gen_w2_funcs.py): task / event member and static functions
#define W2_HAVE_FUNC 1
#include "wave2_oracle_edge.inc"         // hand-maintained edge-input rows (W2-OPEN)
#endif
#endif // !W3_VEH_ONLY && !W3_PED_ONLY && !W3_EV_ONLY && !W3_TASKS_ONLY && !W3_GP_ONLY

static void TestW2Infra() {
    // self-test of the import redirection: the exe's CRT entry points, an exe IAT slot and the port's operator new all hand out arena memory in the same deterministic sequence
    using namespace oracle::alloc;
    Begin(7);
    void* a = oracle::Fn<void* __cdecl(size_t)>(0x824257)(24);        // exe malloc
    void* b = oracle::Fn<void* __cdecl(size_t)>(0x82119A)(40);        // exe operator new
    void* c = new int[4];                                              // port operator new[] (host)
    oracle::Fn<void __cdecl(void*)>(0x82413F)(a);                      // exe free
    void* d = oracle::Fn<void* __cdecl(size_t)>(0x824257)(8);
    // an exe IAT slot (the CRT's HeapAlloc import) now holds the arena stub: calling through it returns arena memory
    void* e = nullptr;
    for (uintptr_t slot = 0x858000; slot < 0x858378; slot += 4)
        if (*reinterpret_cast<void**>(slot) == reinterpret_cast<void*>(&SHeapAlloc)) { e = (*reinterpret_cast<LPVOID (WINAPI**)(HANDLE, DWORD, SIZE_T)>(slot))(nullptr, 0, 12); break; }
    End();
    const bool ok = InArena(a) && InArena(b) && InArena(c) && InArena(d) && InArena(e) && (uint8_t*)b == (uint8_t*)a + 16 + 32 && (uint8_t*)c == (uint8_t*)b + 16 + 48 && (uint8_t*)d == (uint8_t*)c + 16 + 16 && (uint8_t*)e == (uint8_t*)d + 16 + 16;
    delete[] static_cast<int*>(c);
    std::printf("w2 infra: exe malloc/new/free + port new share one arena sequence (a=%p b=%p c=%p d=%p): %s\n", a, b, c, d, ok ? "ok" : "FAIL");
    if (!ok) std::exit(5);
    // the two random generators agree: the exe's rand() (0x821B1E on the fake _getptd block) and the port's game LCG
    bool rok = true; int pr[6], er[6];
    SeedBoth(12345); for (int i = 0; i < 6; ++i) pr[i] = rand();
    SeedBoth(12345); for (int i = 0; i < 6; ++i) er[i] = oracle::Fn<int __cdecl()>(0x821B1E)();
    for (int i = 0; i < 6; ++i) rok &= pr[i] == er[i];
    std::printf("w2 infra: rand streams port %d %d %d exe %d %d %d: %s\n", pr[0], pr[1], pr[2], er[0], er[1], er[2], rok ? "ok" : "FAIL");
    if (!rok) std::exit(5);
}

static void TestW2() {
    TestW2Infra();
#ifdef W3_VEH_ONLY
    using namespace w2;
    rf::Init();
    g_tp.Init(); g_ep.Init();
    g_onFault = &w2::OnFault;
    TestW3Veh();
    return;
#endif
#ifdef W3_PED_ONLY
    using namespace w2;
    rf::Init();
    g_tp.Init(); g_ep.Init();
    g_onFault = &w2::OnFault;
    TestW3Ped();
    return;
#endif
#ifdef W3_EV_ONLY
    using namespace w2;
    rf::Init();
    g_tp.Init(); g_ep.Init();
    g_onFault = &w2::OnFault;
    TestW3Ev();
    return;
#endif
#ifdef W3_TASKS_ONLY
    using namespace w2;
    rf::Init();
    g_tp.Init(); g_ep.Init();
    g_onFault = &w2::OnFault;
    TestW3Tasks();
    return;
#endif
#ifdef W3_GP_ONLY
    using namespace w2;
    rf::Init();
    g_tp.Init(); g_ep.Init();
    g_onFault = &w2::OnFault;
    TestW3Gp();
    return;
#endif
#ifdef W3_ANIM_ONLY
    using namespace w2;
    g_onFault = &w3a::OnFaultA;
    TestW3Anim();
    return;
#endif
#ifdef W3_MODELS_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); };
    TestW3ModelsAll();
    return;
#endif
#ifdef W3_AUDIO_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); };
    TestW3AudioAll();
    return;
#endif
#ifdef W3_ENTITY_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); };
    TestW3EntityAll();
    return;
#endif
#ifdef W3_INTERIOR_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); };
    TestW3InteriorAll();
    return;
#endif
#ifdef W3_CAMERA_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); };
    TestW3CameraAll();
    return;
#endif
#ifdef W3_WORLD_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); };
    TestW3WorldAll();
    return;
#endif
#ifdef W3_SCRIPTS_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); g_tp.Leave(); g_ep.Leave(); rf::Leave(); };
    TestW3ScriptsAll();
    return;
#endif
#ifdef W3_UI_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); };
    TestW3UiAll();
    return;
#endif
#ifdef W3_FX_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); };
    TestW3FxAll();
    return;
#endif
#ifdef W3_AI_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); rf::Leave(); };
    TestW3AiAll();
    return;
#endif
#ifdef W3_INPUT_ONLY
    using namespace w2;
    g_onFault = +[] { oracle::alloc::End(); };
    TestW3InputAll();
    return;
#endif
    using namespace w2;
    rf::Init();
    g_tp.Init(); g_ep.Init();
    g_onFault = &w2::OnFault;
#ifdef W2_HAVE_CTOR
    TestW2Ctors();
#endif
#ifdef W2_HAVE_CLONE
    TestW2Clones();
#endif
#ifdef W2_HAVE_FUNC
    TestW2Funcs();
    TestW2Edge();
#endif
}
