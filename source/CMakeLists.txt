cmake_minimum_required(VERSION 4.2.0 FATAL_ERROR)

include(FetchContent)

#--------------------------------------------------------------------
# Game features
#--------------------------------------------------------------------
option(${RE_PROJECT}_WITH_OPENAL "Use OpenAL" OFF)
option(${RE_PROJECT}_USE_SDL3 "Use SDL3" OFF)
option(${RE_PROJECT}_STANDALONE "Build standalone executable" OFF)
option(${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY "Dump hooks only" OFF)
# Portable (non-Windows / non-x86) build of the runnable standalone exe, preset PortableMac (.notes/STREAM_C_PLAN.md, slice A-1 + C-0). Needs STANDALONE=ON (the detached run
# configuration). OFF (default): nothing below changes, every compile command and every Windows build artefact stays as it was (the Win32 x86 MSVC build is the fidelity reference).
# ON: defines NOTSA_PORTABLE=1 on the game target and
#   * the hook registry is not built: reversiblehooks/*.cpp, reversiblehooks/ReversibleHook/*.cpp, the hook UI (toolsmenu/DebugModules/HooksDebugModule/*.cpp), dllmain.cpp (ASI entry),
#     PluginBase.cpp (GetModuleHandle based plugin-sdk base address). The RH_Scoped* macros are already no-ops in the detached run build (reversiblehooks/NoHookRegistry.h; NOTSA_PORTABLE
#     implies NOTSA_NO_HOOK_REGISTRY) and ReversibleHooks.h includes only reversiblehooks/PortableStub.h (empty RHManager, HookInstallOptions::HS) instead of the registry headers.
#     InjectHooksMain.cpp stays: it calls the (empty) X::InjectHooks() and the non-hook part (script command handler registration, CommandLine::PostHooksInjected).
#   * standalone/Fixups.cpp keeps only the portable part (Log / Fatal / InstallDiagnostics, std C++); its Win32 / SEH / CRT-debug code is `#if !NOTSA_PORTABLE`.
#   * the exe-oracle / shim unit tests (tests/standalone, EXCLUDE_FROM_ALL, they map gta_sa_compact.exe) are not defined, GTASA_IMAGE_DIAG / GTASA_VERIFY_GLOBALS are rejected.
#   * the Windows system libraries (d3d9 ddraw dxguid strmiids Winmm dsound), /SUBSYSTEM and the .exe suffix are not applied; warnings are off (-w) and not errors (stream C-3 audits them).
option(${RE_PROJECT}_PORTABLE "Portable (macOS arm64) build: hook registry / exe-oracle / Win32 startup compiled out, see source/CMakeLists.txt" OFF)
option(${RE_PROJECT}_FPE_TRAPS "Dev (portable macOS arm64 only): run with floating-point exception traps (INVALID / DIVBYZERO / OVERFLOW) and log every distinct trap site, see source/portable/fpe_traps.h" OFF)
if (${RE_PROJECT}_FPE_TRAPS AND NOT ${RE_PROJECT}_PORTABLE)
    message(FATAL_ERROR "${RE_PROJECT}_FPE_TRAPS needs ${RE_PROJECT}_PORTABLE=ON")
endif()
if (${RE_PROJECT}_PORTABLE)
    if (NOT ${RE_PROJECT}_STANDALONE OR ${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY)
        message(FATAL_ERROR "${RE_PROJECT}_PORTABLE requires ${RE_PROJECT}_STANDALONE=ON and ${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY=OFF (the detached run configuration)")
    endif()
endif()

if (${RE_PROJECT}_USE_SDL3)
    add_compile_definitions(NOTSA_USE_SDL3)
endif()
if (${RE_PROJECT}_STANDALONE)
    add_compile_definitions(NOTSA_STANDALONE)
endif()
# Test-only env-driven key injector (NOTSA_STANDALONE_INPUT) in SDLWrapper.cpp for the ASI/DLL build (the standalone run build always has it)
option(${RE_PROJECT}_INPUT_INJECT "Enable the NOTSA_STANDALONE_INPUT key injector in the DLL build" OFF)
if (${RE_PROJECT}_INPUT_INJECT)
    add_compile_definitions(NOTSA_INPUT_INJECT) # also #undefs TRACY_ENABLE in StdInc.h (no profiler server -> events queue forever)
endif()
# RR-1 (.notes/RR_PLAN.md): deterministic record/replay state-hash harness (per-frame hashes of player / peds / vehicles / RNG / timer, trace file, virtual clock).
# OFF by default => every TU is byte-identical to a build without it. The Windows reference host builds StandaloneRelease with -DGTASA_STATEHASH=ON to produce traces.
option(${RE_PROJECT}_STATEHASH "Compile the NOTSA_STATEHASH deterministic state-hash harness hooks" OFF)
if (${RE_PROJECT}_STATEHASH)
    add_compile_definitions(NOTSA_STATEHASH)
endif()
if (${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY)
    if (NOT ${RE_PROJECT}_STANDALONE)
        message(FATAL_ERROR "${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY requires ${RE_PROJECT}_STANDALONE=ON")
    endif()
    add_compile_definitions(NOTSA_STANDALONE_DUMP_HOOKS_ONLY)
endif()
# Standalone exe that is meant to RUN (not just dump the hooks table). F3: it needs nothing of the original exe (no data image, no extracted files, no fixed base)
if (${RE_PROJECT}_STANDALONE AND NOT ${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY)
    add_compile_definitions(NOTSA_STANDALONE_RUN)
    set(${RE_PROJECT}_STANDALONE_RUN_BUILD ON)
    # D7 (.notes/PHASE2_PLAN.md): the original is x87 code (no SSE) running at PC=24 (D3D9 CreateDevice without FPU_PRESERVE). Everything compiled
    # in this directory (game code, shim, librw, shim unit tests) uses x87 + strict FP: no SSE/SSE2 codegen, no /fp:fast, no FMA contraction.
    if (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        # MAC-WIN64: x64 has no x87 code generation (SSE2 is the baseline, /arch:IA32 does not exist); the x87 sites are the lifted portable branches (NOTSA_PORTABLE, MAC-F4a)
        add_compile_options(/fp:precise)
        add_compile_options(/wd4716)
    elseif (MSVC)
        add_compile_options(/arch:IA32 /fp:precise)
        add_compile_options(/wd4716) # NOTSA_UNREACHABLE is a logging fall-through in the run build (Base.h): stub functions that consist only of it would otherwise be 'must return a value' errors
    endif()
    # StandaloneRelease (CMakePresets.json): optimised config of the same exe. /O2 /Ob2 /DNDEBUG come from CMAKE_CXX_FLAGS_RELEASE, -MT from CMAKE_MSVC_RUNTIME_LIBRARY (top CMakeLists.txt).
    # /fp:precise forbids reassociation/contraction and /arch:IA32 keeps x87, so under PC=24 every op still rounds to 24 bits whether it lives in a register or a spill slot.
    # No /GL (no LTCG: keeps per-object disassembly comparable), no /fp:fast; ICF/REF off: hooks key on function addresses, folded functions would alias.
    option(${RE_PROJECT}_STANDALONE_RELEASE "Optimised (/O2) standalone config, see CMakePresets.json" OFF)
    if (MSVC AND ${RE_PROJECT}_STANDALONE_RELEASE)
        # debug info: /Z7 (GTASA_NO_EDIT_AND_CONTINUE=ON from the toolchain) + linker /DEBUG -> gta_reversed.pdb
        add_link_options(/DEBUG /INCREMENTAL:NO /OPT:NOREF /OPT:NOICF)
    endif()
    # Original gta_sa.exe (1.0 US / compact): a DEV-ONLY input, never needed to configure, build or run the exe (slices F3/G1 of .notes/DETACH_DATA_PLAN.md). It is empty by
    # default and no user-facing preset sets it. Only two dev configurations look at it: GTASA_IMAGE_DIAG (preset StandaloneDetached) extracts original_data.bin from it, and
    # the exe-oracle tests read it at TEST run time from the environment (RW_EXE_ORACLE; tools/standalone/run_all_tests.sh sets it when the file exists, otherwise those tests
    # exit 77 and are reported as SKIP). Without a path nothing here depends on it.
    set(${RE_PROJECT}_ORACLE_EXE "" CACHE FILEPATH "Dev only: path of the original gta_sa.exe (1.0 US compact). Used by GTASA_IMAGE_DIAG to extract the data image; never needed to build or run the standalone exe")
    if (${RE_PROJECT}_ORACLE_EXE AND NOT EXISTS "${${RE_PROJECT}_ORACLE_EXE}")
        message(FATAL_ERROR "${RE_PROJECT}_ORACLE_EXE does not exist: ${${RE_PROJECT}_ORACLE_EXE}")
    endif()
endif()

# Stream F (.notes/STREAM_F_PLAN.md): float-only translation units of the run build are compiled for SSE2 instead of the x87. They are written with
# notsa::fp (Core/Fp.h): plain float arithmetic, bit-identical to the x87 at PC=24 unless an intermediate leaves the float exponent range, in which case the
# F24 model takes over. No PCH (the PCH is built with /arch:IA32; /arch must match), no unity build.
if (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4 AND ${RE_PROJECT}_STANDALONE_RUN_BUILD)
    set_source_files_properties(game_sa/Collision/CollisionPrimitives.cpp PROPERTIES COMPILE_OPTIONS "/arch:SSE2" SKIP_UNITY_BUILD_INCLUSION ON SKIP_PRECOMPILE_HEADERS ON)
endif()

# RenderWare backend. OFF: the game's own RW headers/wrappers (they forward to the original exe's RW by address).
# ON: the RW C API is provided by source/fakerw over vendor/librw (git submodule, pinned). Implied ON by the runnable standalone build
# (STANDALONE=ON, not DUMP_HOOKS_ONLY); the hook-dump build and the ASI build keep the real RW headers. See .notes/P2B_SHIM_PLAN.md.
option(${RE_PROJECT}_RW_LIBRW "Use librw (source/fakerw) instead of the original RenderWare" OFF)
option(${RE_PROJECT}_RW_LIBRW_OPT_OUT "Do not imply RW_LIBRW from a runnable STANDALONE build" OFF)
if (${RE_PROJECT}_STANDALONE AND NOT ${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY AND NOT ${RE_PROJECT}_RW_LIBRW_OPT_OUT)
    set(${RE_PROJECT}_RW_LIBRW ON CACHE BOOL "Use librw (source/fakerw) instead of the original RenderWare" FORCE)
endif()

#--------------------------------------------------------------------
# Build options
#--------------------------------------------------------------------
option(${RE_PROJECT}_UNITY_BUILD "Use Unity/Jumbo build" OFF)
option(${RE_PROJECT}_WITH_SANITIZERS "Use Sanitizers (better crash log)" OFF)
option(${RE_PROJECT}_WITH_LTO "Use Link Time Optimization" OFF)
option(${RE_PROJECT}_WITH_CLEO_COMMANDS "Enable CLEO script commands" OFF)
option(${RE_PROJECT}_WITH_SCRIPT_COMMAND_HOOKS "Create script command hooks in the UI (Command hooks are installed regardless)" OFF)

if(${RE_PROJECT}_WITH_SCRIPT_COMMAND_HOOKS)
    add_compile_definitions(NOTSA_WITH_SCRIPT_COMMAND_HOOKS)
endif()

if (NOT WIN32 AND NOT ${RE_PROJECT}_WITH_OPENAL)
    set(${RE_PROJECT}_WITH_OPENAL ON)
endif()

#--------------------------------------------------------------------
# Gather source files, and create the target
#--------------------------------------------------------------------

#
# Regular source files
# Module scanning is disabled for these, so we can unity build
# If any of them contain module imports the compiler should error out
#
file(
    GLOB_RECURSE SOURCE_FILES_WITHOUT_MODULES_LIST
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h*"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c*"
)
list(APPEND SOURCE_FILES_WITHOUT_MODULES_LIST StdInc.cpp)
if (${RE_PROJECT}_WITH_OPENAL)
    # TODO: Remove dsound specific sources
else()
    file(
        GLOB_RECURSE OPENAL_FILES
        LIST_DIRECTORIES false
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/game_sa/Audio/OpenAL/*.c*"
        "${CMAKE_CURRENT_SOURCE_DIR}/game_sa/Audio/OpenAL/*.h*"
    )
    list(REMOVE_ITEM SOURCE_FILES_WITHOUT_MODULES_LIST ${OPENAL_FILES})
endif()

if (${RE_PROJECT}_RW_LIBRW)
    # The wrappers in game_sa/RenderWare/rw/*.cpp forward to the exe's RenderWare by address; with librw the API comes from source/fakerw
    file(
        GLOB_RECURSE RW_WRAPPER_FILES
        LIST_DIRECTORIES false
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/game_sa/RenderWare/rw/*.c*"
    )
    list(REMOVE_ITEM SOURCE_FILES_WITHOUT_MODULES_LIST ${RW_WRAPPER_FILES})
    # WindowedMode.cpp is a copy of RW's D3D9 device layer (hooked into the exe's RenderWare internals); librw owns the device/swap chain instead
    list(REMOVE_ITEM SOURCE_FILES_WITHOUT_MODULES_LIST app/platform/win/WindowedMode.cpp)
endif()

if (${RE_PROJECT}_PORTABLE)
    # Hook registry, hook UI, ASI entry point and the plugin-sdk base address are not part of the portable exe (documented at the option above)
    file(
        GLOB_RECURSE PORTABLE_EXCLUDED_FILES
        LIST_DIRECTORIES false
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reversiblehooks/*.c*"
        "${CMAKE_CURRENT_SOURCE_DIR}/toolsmenu/DebugModules/HooksDebugModule/*.c*"
    )
    list(APPEND PORTABLE_EXCLUDED_FILES dllmain.cpp PluginBase.cpp)
    if (NOT WIN32)
        # MAC-W0: Win32-only translation units that have a portable counterpart in source/portable/*.cpp (oswrapper_win.cpp -> portable/oswrapper_portable.cpp)
        list(APPEND PORTABLE_EXCLUDED_FILES oswrapper/oswrapper_win.cpp)
        # MAC-W0: Win32 overlapped-I/O CdStream -> portable/cdstream_portable.cpp; Media Foundation / WMA user-track decoders -> portable/audio_decoders_portable.cpp
        list(APPEND PORTABLE_EXCLUDED_FILES game_sa/CdStreamInfo.cpp game_sa/Audio/Loaders/AEMFDecoder.cpp game_sa/Audio/Loaders/AEWMADecoder.cpp)
    else()
        # MAC-WIN64: the portable build on Windows (x64 MSVC) uses the real Windows headers and the original Win32 OS services (oswrapper_win.cpp, overlapped CdStream, Media Foundation /
        # WMA decoders, DirectSound), so the POSIX / OpenAL stand-ins in source/portable are not part of it. Everything else in source/portable (SDL3 app layer, GL3 render) is shared.
        list(APPEND PORTABLE_EXCLUDED_FILES portable/oswrapper_portable.cpp portable/cdstream_portable.cpp portable/audio_decoders_portable.cpp portable/dsound_al.cpp
             portable/app_debug_posix.cpp portable/link_aliases.cpp portable/app_main.cpp portable/statehash.cpp)
    endif()
    # MAC-W0a (app/): Win32-only app/platform TUs replaced by portable/app_*.cpp
    set(PORTABLE_EXCLUDED_APP_FILES app/platform/win/VideoPlayer/VideoPlayer.cpp app/platform/win/VideoModeSelectDialog.cpp)
    if (NOT WIN32)
        list(APPEND PORTABLE_EXCLUDED_APP_FILES extensions/debug.cpp) # DbgHelp symbolication -> portable/app_debug_posix.cpp (Windows keeps the original: x64 DbgHelp)
    endif()
    list(APPEND PORTABLE_EXCLUDED_FILES ${PORTABLE_EXCLUDED_APP_FILES})
    # MAC-R2 (stream R, .notes/STREAM_R_PLAN.md): the D3D9 fixed-function render facade has no GL counterpart; librw's gl3 pipelines replace it.
    # Symbols still referenced from kept TUs are stubbed in portable/render/rw_stubs_gl3.cpp (honest Unsupported + failure), see tools/portable/rw_symbol_audit.py.
    set(PORTABLE_EXCLUDED_RENDER_FILES
        standalone/rw/pipeline.cpp standalone/rw/pipeline_matfx.cpp standalone/rw/rwd3d_ff.cpp
        standalone/rw/pipeline_skin.cpp standalone/rw/pipeline_skin_cpu.cpp standalone/rw/skin_vs.cpp
        standalone/rw/im2d_im3d.cpp
        game_sa/RenderWare/D3DResourceSystem.cpp game_sa/RenderWare/D3DTextureBuffer.cpp game_sa/RenderWare/D3DIndexDataBuffer.cpp)
    # MAC-R3a: platform.cpp is the D3D9 Reset watch -> portable/render/platform_gl3.cpp (engine.cpp stays, its D3D parts are guarded; twin: portable/render/engine_gl3.cpp)
    list(APPEND PORTABLE_EXCLUDED_RENDER_FILES standalone/rw/platform.cpp)
    list(APPEND PORTABLE_EXCLUDED_FILES ${PORTABLE_EXCLUDED_RENDER_FILES})
    # MAC-BUILD1: the ImGui D3D9 backend needs <d3d9.h>; the GL3 build drives ImGui through the SDL3 (+ GL) bindings instead
    list(APPEND PORTABLE_EXCLUDED_FILES libs/imgui/bindings/imgui_impl_dx9.cpp)
    list(REMOVE_ITEM SOURCE_FILES_WITHOUT_MODULES_LIST ${PORTABLE_EXCLUDED_FILES})
else()
    # source/portable/*.cpp are the portable-only implementations (MAC-W0); the Windows builds never see them (headers there are fine: they are never compiled on their own)
    file(
        GLOB_RECURSE PORTABLE_ONLY_FILES
        LIST_DIRECTORIES false
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/portable/*.c*"
    )
    list(REMOVE_ITEM SOURCE_FILES_WITHOUT_MODULES_LIST ${PORTABLE_ONLY_FILES})
    if (${RE_PROJECT}_STATEHASH)
        # RR-3: the state-hash harness is the one source/portable/ TU that also builds into the Win32 StandaloneRelease variant (the portable-only parts of it are #ifdef NOTSA_PORTABLE).
        # Nothing is added when the option is OFF.
        list(APPEND SOURCE_FILES_WITHOUT_MODULES_LIST portable/statehash.cpp)
    endif()
endif()

#
# Source files that should be scanned for modules
#
set(SOURCE_FILES_WITH_MODULES_LIST) # Empty for now, we dont use modules yet

#
# Module source files
#
file(
    GLOB_RECURSE CXX_MODULE_FILES_LIST
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ixx"
)

if (${RE_PROJECT}_STANDALONE)
    add_executable(${RE_PROJECT_LIB_NAME} WIN32)
else()
    add_library(${RE_PROJECT_LIB_NAME} MODULE)
endif()
if (CXX_MODULE_FILES_LIST)
    target_sources(${RE_PROJECT_LIB_NAME} PRIVATE FILE_SET cxx_modules TYPE CXX_MODULES FILES ${CXX_MODULE_FILES_LIST})
endif()
if (SOURCE_FILES_WITH_MODULES_LIST)
    set_source_files_properties(${SOURCE_FILES_WITH_MODULES_LIST} PROPERTIES CXX_SCAN_FOR_MODULES ON)
    target_sources(${RE_PROJECT_LIB_NAME} PRIVATE FILE_SET cxx_modules FILES ${SOURCE_FILES_WITH_MODULES_LIST})
endif()
if (SOURCE_FILES_WITHOUT_MODULES_LIST)
    set_source_files_properties(${SOURCE_FILES_WITHOUT_MODULES_LIST} PROPERTIES CXX_SCAN_FOR_MODULES OFF)
    target_sources(${RE_PROJECT_LIB_NAME} PRIVATE ${SOURCE_FILES_WITHOUT_MODULES_LIST})
endif()
# (StandaloneRelease links non-incrementally: ILT thunks would change every function address a hook/fixup registers)
if (NOT ${RE_PROJECT}_STANDALONE_RELEASE)
    target_link_options(${RE_PROJECT_LIB_NAME} PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/INCREMENTAL>"
    )
endif()
if (${RE_PROJECT}_PORTABLE)
    # Provisional portable flag set (C-8 decides the final one): the front-end flags the census (tools/standalone/clang_census.py) uses, plus the codegen flags that keep the
    # original's semantics (no FMA contraction, MSVC bit-field layout, signed char, no TBAA/overflow tricks). Warnings are not errors here (C-3 audits them).
    if (MSVC)
        # MAC-WIN64: cl.exe x64. /fp:precise is the default (SSE2, no x87 on x64: the x87 sites take the portable lifted branches), /Zc:preprocessor like the Win32 build, warnings off like -w
        target_compile_options(${RE_PROJECT_LIB_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/MP;/Zc:preprocessor;/w;/EHsc;/bigobj>")
    else()
    target_compile_options(${RE_PROJECT_LIB_NAME} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-fms-extensions;-mms-bitfields;-fno-delayed-template-parsing;-ffp-contract=off;-fsigned-char;-fno-strict-aliasing;-fwrapv;-w>"
    )
    endif()
else()
target_compile_options(${RE_PROJECT_LIB_NAME} PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/MP;/Zc:preprocessor;/W3;/wd4200;/wd4251;/wd4275;/wd4996;/wd26495;/wd26812;/wd28159>"
    "$<$<OR:$<CXX_COMPILER_ID:Clang,GCC>,$<CXX_COMPILER_ID:GNU>>:-Wall;-Wno-switch>"
)
endif()

#--------------------------------------------------------------------
# The run build has no original data image (slices A2..F3 of .notes/DETACH_DATA_PLAN.md)
#--------------------------------------------------------------------
# Every NOTSA_GLOBAL* (Base.h) of the run exe is a real C++ variable with the exe's initial value (generated by tools/standalone/codemod_globals.py): NOTSA_DETACHED_GLOBALS is always
# defined for the exe target. The DLL/ASI build and the exe-oracle tests keep the old `auto& x = StaticRef<T>(0xADDR)` form (the oracle tests add NOTSA_ORACLE_ADDRESS_GLOBALS: they
# must see the REAL original storage at the original VA, they map the original exe themselves).
# Dev-only options (OFF in StandaloneRelease / StandaloneRW, ON in StandaloneDetached):
#   GTASA_VERIFY_GLOBALS  register every global and dump it at boot (NOTSA_VERIFY_GLOBALS=<file>), checked by tools/standalone/verify_globals.py against original_data.bin
#                         (the exe fopen()s original_data.bin itself for the D2 table checks; nothing is mapped)
#   GTASA_IMAGE_DIAG      map the original data image at its original address (linked at /BASE:0x400000 /FIXED with the 9 MB placeholder, original_data.bin/json next to the
#                         exe) for the stray-access census / poison diagnostics (source/standalone/ImageLoad.cpp, ImageDiag.cpp). Proves the run code touches none of it.
option(${RE_PROJECT}_VERIFY_GLOBALS "Dev: register every detached global and dump it at boot (NOTSA_VERIFY_GLOBALS=<file>), checked by tools/standalone/verify_globals.py" OFF)
option(${RE_PROJECT}_IMAGE_DIAG "Dev: map the original data image (fixed base + placeholder) for the census / poison diagnostics" OFF)
if (${RE_PROJECT}_VERIFY_GLOBALS OR ${RE_PROJECT}_IMAGE_DIAG)
    if (NOT ${RE_PROJECT}_STANDALONE_RUN_BUILD)
        message(FATAL_ERROR "${RE_PROJECT}_VERIFY_GLOBALS / ${RE_PROJECT}_IMAGE_DIAG need the runnable standalone build (${RE_PROJECT}_STANDALONE=ON, not DUMP_HOOKS_ONLY)")
    endif()
endif()
if (${RE_PROJECT}_STANDALONE_RUN_BUILD)
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE NOTSA_DETACHED_GLOBALS)
endif()
if (${RE_PROJECT}_PORTABLE)
    if (${RE_PROJECT}_VERIFY_GLOBALS OR ${RE_PROJECT}_IMAGE_DIAG)
        message(FATAL_ERROR "${RE_PROJECT}_VERIFY_GLOBALS / ${RE_PROJECT}_IMAGE_DIAG are dev configurations of the Windows run exe (original data image); not available with ${RE_PROJECT}_PORTABLE")
    endif()
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE NOTSA_PORTABLE=1)
    # MAC-FPTRAP: FP-exception trap audit (FPCR.IOE / DZE / OFE on the main + worker threads, SIGILL handler logs PC + backtrace, see source/portable/fpe_traps.h). Dev only, never ON in a shipped build.
    if (${RE_PROJECT}_FPE_TRAPS)
        target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE NOTSA_FPE_TRAPS=1)
    endif()
    # P-2k: the 64-bit width guard of the typed save/stream templates (FileRecord.hpp) is an ERROR in the portable build: `layout_diff.py guard` lists 0 offenders after P-2b..j,
    # so a new SaveDataToWorkBuffer/LoadDataFromWorkBuffer/RwStreamRead<T> with a pointer/long/size_t/ptrdiff_t fails to compile instead of writing 8 bytes into a 4-byte slot.
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE NOTSA_FILE_SCALAR_STRICT)
    # MAC-C3: falling off the end of a non-void function / lambda is UB that clang exploits (the path becomes `unreachable` and execution runs into the next function); MSVC
    # (and so the exe) returns whatever is in EAX / ST0. -Wreturn-type is an ERROR here (0 sites after MAC-C3: NOTSA_UNREACHABLE_RET, Base.h), and -fno-strict-return makes a
    # path the compiler cannot see (e.g. a template instantiation) return an unspecified value instead of being miscompiled.
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${RE_PROJECT_LIB_NAME} PRIVATE -Werror=return-type -fno-strict-return)
        # The game is full of MSVC-only type punning (-Wundefined-reinterpret-cast: FileLoader col buffers `reinterpret_cast<T*&>(it)++`, int<->float script words, GangWars/Group12 raw bits):
        # MSVC has no type-based alias analysis, clang -O2 has. -fno-strict-aliasing keeps the exe's semantics.
        target_compile_options(${RE_PROJECT_LIB_NAME} PRIVATE -fno-strict-aliasing)
    endif()
endif()
if (${RE_PROJECT}_VERIFY_GLOBALS)
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE NOTSA_VERIFY_GLOBALS)
endif()
if (${RE_PROJECT}_IMAGE_DIAG)
    if (NOT ${RE_PROJECT}_ORACLE_EXE)
        message(FATAL_ERROR "${RE_PROJECT}_IMAGE_DIAG is a dev-only configuration and needs -D${RE_PROJECT}_ORACLE_EXE=<gta_sa.exe> (original_data.bin is extracted from it). The run exe (StandaloneRelease / StandaloneRW) does not need any exe")
    endif()
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE NOTSA_IMAGE_DIAG)
endif()

if (${RE_PROJECT}_STANDALONE) 
    set(TARGET_SUFFIX ".exe")
else()
    set(TARGET_SUFFIX ".asi")
endif()
set_target_properties(${RE_PROJECT_LIB_NAME}
    PROPERTIES
    SUFFIX "${TARGET_SUFFIX}"
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/"
    COMPILE_WARNING_AS_ERROR ON
)
if (${RE_PROJECT}_PORTABLE)
    if (WIN32)
        set_target_properties(${RE_PROJECT_LIB_NAME} PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
        target_link_options(${RE_PROJECT_LIB_NAME} PRIVATE "/SUBSYSTEM:WINDOWS")
    else()
        set_target_properties(${RE_PROJECT_LIB_NAME} PROPERTIES SUFFIX "" COMPILE_WARNING_AS_ERROR OFF)
    endif()
else()
target_link_options(${RE_PROJECT_LIB_NAME} PRIVATE "/SUBSYSTEM:WINDOWS")
endif()

if (${RE_PROJECT}_STANDALONE AND NOT ${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY)
    # F3: a normal exe image. Default image base, ASLR (/DYNAMICBASE) and a relocation table: nothing in the run code depends on a fixed address any more (the census proves it
    # touches no original address; there is no /FIXED, no 9 MB placeholder, no /BASE:0x400000). /INCREMENTAL:NO is kept (reproducible link, no ILT thunks).
    option(${RE_PROJECT}_ASLR "Run exe: link with /DYNAMICBASE (ASLR). OFF: /DYNAMICBASE:NO, the exe always loads at the MSVC default base 0x400000 like the original (bit-reproducible addresses for debugging)" ON)
    if (${RE_PROJECT}_ASLR)
        target_link_options(${RE_PROJECT_LIB_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/INCREMENTAL:NO;/DYNAMICBASE>")
    else()
        target_link_options(${RE_PROJECT_LIB_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/INCREMENTAL:NO;/DYNAMICBASE:NO>")
    endif()
    # Keep the exe/pdb/map out of <repo>/bin/<config>, where the DLL build puts gta_reversed.asi/.pdb
    set_target_properties(${RE_PROJECT_LIB_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # python: only the dev side needs it: make_orig_pad.py writes the placeholder object of the exe-oracle test targets (EXCLUDE_FROM_ALL), extract_exe_data.py (GTASA_IMAGE_DIAG) needs
    # `unicorn` (pip install unicorn) to replay the exe's static initialisers; pass -D${RE_PROJECT}_PYTHON=<venv python>. The run exe (target ${RE_PROJECT_LIB_NAME}) builds without python.
    find_program(${RE_PROJECT}_PYTHON NAMES python3 python NO_CMAKE_FIND_ROOT_PATH)
    if (NOT ${RE_PROJECT}_PYTHON)
        if (${RE_PROJECT}_IMAGE_DIAG)
            message(FATAL_ERROR "python3 not found (needed by GTASA_IMAGE_DIAG to extract the data image)")
        endif()
        message(STATUS "python3 not found: the run exe builds, the exe-oracle *_test targets need it (pass -D${RE_PROJECT}_PYTHON=<python>)")
        set(${RE_PROJECT}_PYTHON python)
    endif()
    # The placeholder object (plain `.text` section, see tools/standalone/make_orig_pad.py) MUST be the first object on the link line of every target that maps the original
    # image at its original VA: the exe-oracle tests (below) and, with GTASA_IMAGE_DIAG, the exe. The default run exe does NOT link it.
    set(STANDALONE_PAD_OBJ "${CMAKE_BINARY_DIR}/orig_image_pad.obj")
    add_custom_command(
        OUTPUT "${STANDALONE_PAD_OBJ}"
        COMMAND "${${RE_PROJECT}_PYTHON}" -I "${PROJECT_SOURCE_DIR}/tools/standalone/make_orig_pad.py" "${STANDALONE_PAD_OBJ}"
        DEPENDS "${PROJECT_SOURCE_DIR}/tools/standalone/make_orig_pad.py"
        COMMENT "Writing the original-image placeholder object"
        VERBATIM
    )
    set_source_files_properties("${STANDALONE_PAD_OBJ}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)

    if (${RE_PROJECT}_IMAGE_DIAG)
        # Dev-only census config: the original data range (0x858000 ... 0xCB0000) must be at its original VA. A VirtualAlloc at startup is NOT reliable (the process heap /
        # NLS mappings already sit in low memory, see .notes/P2A_DESIGN.md), so the main image itself covers it: linked at the original base with the placeholder. No relocations.
        target_link_options(${RE_PROJECT_LIB_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text>")
        target_sources(${RE_PROJECT_LIB_NAME} PRIVATE "${STANDALONE_PAD_OBJ}")
        get_target_property(_all_sources ${RE_PROJECT_LIB_NAME} SOURCES)
        list(REMOVE_ITEM _all_sources "${STANDALONE_PAD_OBJ}")
        set_property(TARGET ${RE_PROJECT_LIB_NAME} PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_all_sources})

        set(STANDALONE_DATA_DIR "${CMAKE_BINARY_DIR}/standalone_data")
        set(STANDALONE_DATA_OUT
            "${STANDALONE_DATA_DIR}/original_data.bin"
            "${STANDALONE_DATA_DIR}/original_data.json"
            "${STANDALONE_DATA_DIR}/data_pointers.bin"
            "${STANDALONE_DATA_DIR}/data_pointers.txt")
        add_custom_command(
            OUTPUT ${STANDALONE_DATA_OUT}
            COMMAND "${${RE_PROJECT}_PYTHON}" -I "${PROJECT_SOURCE_DIR}/tools/standalone/extract_exe_data.py"
                    "${${RE_PROJECT}_ORACLE_EXE}" "${STANDALONE_DATA_DIR}"
            DEPENDS "${PROJECT_SOURCE_DIR}/tools/standalone/extract_exe_data.py" "${PROJECT_SOURCE_DIR}/tools/standalone/initterm_delta.py" "${PROJECT_SOURCE_DIR}/.notes/symbols.txt" "${${RE_PROJECT}_ORACLE_EXE}"
            COMMENT "Extracting the original data image from ${${RE_PROJECT}_ORACLE_EXE}"
            VERBATIM
        )
        add_custom_target(${RE_PROJECT_LIB_NAME}_exe_data DEPENDS ${STANDALONE_DATA_OUT})
        add_dependencies(${RE_PROJECT_LIB_NAME} ${RE_PROJECT_LIB_NAME}_exe_data)
        # Next to the exe, where the dev loader and tools/standalone/verify_globals.py / census.py look for them
        add_custom_command(TARGET ${RE_PROJECT_LIB_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${STANDALONE_DATA_DIR}/original_data.bin" "${STANDALONE_DATA_DIR}/original_data.json" "${STANDALONE_DATA_DIR}/data_pointers.bin" "$<TARGET_FILE_DIR:${RE_PROJECT_LIB_NAME}>"
            VERBATIM
        )
    endif()
endif()

if (${RE_PROJECT}_RW_LIBRW)
    # librw as a static library built with the same (static) CRT. Options are forced before add_subdirectory (cache vars of the subproject).
    # NOTE: must come BEFORE the add_dependency() calls below: that macro leaks junk tokens into the directory-wide compile options.
    if (${RE_PROJECT}_PORTABLE)
        # librw offers NULL / GL3 off Windows. The portable build renders through librw's GL3 backend (RW_GL3, public define of the librw target) with SDL3 as the window/GL
        # context library (stream W-1); the platform glue itself is not part of this slice. The D3D9 defines (RW_D3D9 / USE_D3D9 / WITH_D3D) are not set at all.
        set(LIBRW_PLATFORM GL3 CACHE STRING "" FORCE)
        set(LIBRW_GL3_GFXLIB SDL3 CACHE STRING "" FORCE)
    else()
        set(LIBRW_PLATFORM D3D9 CACHE STRING "" FORCE)
    endif()
    set(LIBRW_TOOLS OFF CACHE BOOL "" FORCE)
    set(LIBRW_INSTALL OFF CACHE BOOL "" FORCE)
    set(LIBRW_EXAMPLES OFF CACHE BOOL "" FORCE)
    if (NOT EXISTS "${PROJECT_SOURCE_DIR}/vendor/librw/CMakeLists.txt")
        message(FATAL_ERROR "${RE_PROJECT}_RW_LIBRW needs the librw submodule: git submodule update --init vendor/librw")
    endif()
    add_subdirectory("${PROJECT_SOURCE_DIR}/vendor/librw" "${CMAKE_BINARY_DIR}/vendor/librw")
    # librw is third-party code: never -Werror (COMPILE_WARNING_AS_ERROR is set per target on ${RE_PROJECT_LIB_NAME} only)
    set_target_properties(librw PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    if (${RE_PROJECT}_PORTABLE)
        # C-8 (MAC-FLOOP1): librw's matrix / quaternion / animation math must not be FMA-contracted either (clang's default is -ffp-contract=on: arm64 fuses a*b+c inside one statement, the exe's x87 rounds twice)
        target_compile_options(librw PRIVATE "$<$<COMPILE_LANGUAGE:C,CXX>:-ffp-contract=off>")
    endif()

    # Portable build: no exe-oracle / shim unit tests (they map gta_sa_compact.exe at its original VAs and link Windows libraries; they would also pollute compile_commands.json)
    if (NOT ${RE_PROJECT}_PORTABLE)
    # P2B-01 unit test (defined here, before add_dependency() leaks junk into the directory compile options) of the RW math/stream shim (not part of `all`): ninja rw_math_stream_test
    if (EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/rw_math_stream_test.cpp")
        add_executable(rw_math_stream_test EXCLUDE_FROM_ALL
            "${PROJECT_SOURCE_DIR}/tests/standalone/rw_math_stream_test.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/math.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/rwmath_tables.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/rtquat.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/stream.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/light.cpp"
        )
        target_include_directories(rw_math_stream_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw")
        target_compile_definitions(rw_math_stream_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
        target_include_directories(rw_math_stream_test BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fakerw")
        target_link_libraries(rw_math_stream_test PRIVATE librw)
        # the exe-oracle part of the test maps gta_sa_compact.exe at its own addresses (0x401000..0xCB0000): keep the test image out of that range
        target_link_options(rw_math_stream_test PRIVATE /BASE:0x20000000 /DYNAMICBASE:NO /FIXED:NO)
    endif()

    # (defined here, BEFORE add_dependency() below: that macro calls add_compile_options(<pkg> PRIVATE ...) at directory scope, which every later target inherits as bogus sources)
    # P2B-02b unit test of the frame / camera layer (Wine-runnable, no game data): frame.cpp + camera.cpp + librw (defines RwEngineInstance itself), a hidden window, no PCH
    add_executable(rw_frame_camera_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_frame_camera_test.cpp"
        standalone/rw/frame.cpp
        standalone/rw/camera.cpp
    )
    target_compile_features(rw_frame_camera_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_frame_camera_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(rw_frame_camera_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_frame_camera_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_frame_camera_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)

    # Unit test of the engine layer (Wine-runnable, no game data): engine.cpp + rwglobals.cpp + librw, a hidden window, no PCH
    add_executable(rw_engine_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_engine_test.cpp"
        standalone/rw/engine.cpp
        standalone/rw/rwglobals.cpp
    )
    target_compile_features(rw_engine_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_engine_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(rw_engine_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_engine_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_engine_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)

    # P2B-02c unit test of the render-state mapper + RwD3D9 fixed-function API (not part of `all`): ninja rw_renderstate_test. No PCH; uses a hidden window and
    # librw's D3D9 device when one can be created (Wine/wined3d is fine), otherwise only the device-less checks run.
    if (EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/rw_renderstate_test.cpp")
        add_executable(rw_renderstate_test EXCLUDE_FROM_ALL
            "${PROJECT_SOURCE_DIR}/tests/standalone/rw_renderstate_test.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/renderstate.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/rwd3d_ff.cpp"
        )
        target_compile_features(rw_renderstate_test PRIVATE cxx_std_23)
        target_compile_definitions(rw_renderstate_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
        target_include_directories(rw_renderstate_test BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fakerw")
        target_link_libraries(rw_renderstate_test PRIVATE librw d3d9 user32)
        set_target_properties(rw_renderstate_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()

    # P2B-03a unit test of raster.cpp + image.cpp (Wine-runnable, no game data needed); camera/frame are linked for the Im2D raster render check
    add_executable(rw_raster_image_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_raster_image_test.cpp"
        standalone/rw/raster.cpp
        standalone/rw/image.cpp
        standalone/rw/frame.cpp
        standalone/rw/camera.cpp
    )
    target_compile_features(rw_raster_image_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_raster_image_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(rw_raster_image_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_raster_image_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_raster_image_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)

    # P2B-04a/b unit test of the light / world / material / geometry layer (Wine-runnable; optional SA DFF paths on the command line)
    add_executable(rw_world_geometry_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_world_geometry_test.cpp"
        standalone/rw/light.cpp
        standalone/rw/world.cpp
        standalone/rw/material.cpp
        standalone/rw/geometry.cpp
        standalone/rw/geometry_strip.cpp
    )
    target_compile_features(rw_world_geometry_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_world_geometry_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(rw_world_geometry_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_world_geometry_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_world_geometry_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    # P2B-04b follow-up: bit-exact check of the exe's tristrip generator (0x7591D0) port against index streams dumped from the exe under Unicorn
    # (tools/standalone/strip_unicorn.py); Wine-runnable, no game data
    add_executable(rw_strip_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_strip_test.cpp"
        standalone/rw/light.cpp
        standalone/rw/world.cpp
        standalone/rw/material.cpp
        standalone/rw/geometry.cpp
        standalone/rw/geometry_strip.cpp
    )
    target_compile_features(rw_strip_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_strip_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(rw_strip_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_strip_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_strip_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    # P2B-03b unit test of texture.cpp + texdict.cpp (Wine-runnable; optional argv[1] = SA `models` directory for the real TXD checks)
    add_executable(rw_texture_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_texture_test.cpp"
        standalone/rw/texture.cpp
        standalone/rw/texdict.cpp
        standalone/rw/rwtexdict.cpp
        standalone/rw/raster.cpp
        standalone/rw/image.cpp
        standalone/rw/frame.cpp
        standalone/rw/camera.cpp
    )
    target_compile_features(rw_texture_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_texture_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_texture_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_texture_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_texture_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)


    # P2B-04c unit test of the atomic / clump / plugin-registration layer (Wine-runnable; optional SA DFF paths on the command line)
    add_executable(rw_atomic_clump_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_atomic_clump_test.cpp"
        standalone/rw/atomic.cpp
        standalone/rw/clump.cpp
        standalone/rw/plugins.cpp
        standalone/rw/frame.cpp
        standalone/rw/light.cpp
        standalone/rw/world.cpp
        standalone/rw/material.cpp
        standalone/rw/geometry.cpp
        standalone/rw/geometry_strip.cpp
    )
    target_compile_features(rw_atomic_clump_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_atomic_clump_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(rw_atomic_clump_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_atomic_clump_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_atomic_clump_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    # P2B-09 unit test of platform.cpp (device restore callback on Reset, gamma ramp through the engine's device); Wine-runnable, no game data
    add_executable(rw_platform_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_platform_test.cpp"
        standalone/rw/platform.cpp
        standalone/rw/renderstate.cpp   # NotsaRwRenderState_OnEngineStarted (called by platform.cpp)
        standalone/rw/rwd3d_ff.cpp
        standalone/rw/engine.cpp
        standalone/rw/rwglobals.cpp
        standalone/rw/raster.cpp
        standalone/rw/image.cpp
        standalone/rw/frame.cpp
        standalone/rw/camera.cpp
    )
    target_compile_features(rw_platform_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_platform_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_platform_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_platform_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_platform_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)


    # P2B-05a unit test of the skin / hanim layer (Wine-runnable; optional SA DFF paths on the command line)
    add_executable(rw_skin_hanim_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_skin_hanim_test.cpp"
        standalone/rw/skin.cpp
        standalone/rw/hanim.cpp
        standalone/rw/atomic.cpp
        standalone/rw/clump.cpp
        standalone/rw/plugins.cpp
        standalone/rw/frame.cpp
        standalone/rw/math.cpp
        standalone/rw/stream.cpp
        standalone/rw/light.cpp
        standalone/rw/world.cpp
        standalone/rw/material.cpp
        standalone/rw/geometry.cpp
        standalone/rw/geometry_strip.cpp
    )
    target_compile_features(rw_skin_hanim_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_skin_hanim_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_skin_hanim_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_skin_hanim_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_skin_hanim_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)

    # P2B-05c unit test of the RtAnim / RtQuat layer (Wine-runnable; optional SA DFF paths on the command line)
    add_executable(rw_rtanim_rtquat_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_rtanim_rtquat_test.cpp"
        standalone/rw/rtanim.cpp
        standalone/rw/rtquat.cpp
        standalone/rw/skin.cpp
        standalone/rw/hanim.cpp
        standalone/rw/atomic.cpp
        standalone/rw/clump.cpp
        standalone/rw/plugins.cpp
        standalone/rw/frame.cpp
        standalone/rw/math.cpp
        standalone/rw/stream.cpp
        standalone/rw/light.cpp
        standalone/rw/world.cpp
        standalone/rw/material.cpp
        standalone/rw/geometry.cpp
        standalone/rw/geometry_strip.cpp
    )
    target_compile_features(rw_rtanim_rtquat_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_rtanim_rtquat_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_rtanim_rtquat_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_rtanim_rtquat_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_rtanim_rtquat_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)

    # P2B-07 unit test of the RxPipeline façade + stock AllInOne callbacks (needs a D3D9 HAL device: Wine/wined3d is fine; optional SA DFF/TXD paths on the command line)
    add_executable(rw_pipeline_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_pipeline_test.cpp"
        standalone/rw/pipeline.cpp
        standalone/rw/renderstate.cpp
        standalone/rw/rwd3d_ff.cpp
        standalone/rw/rwglobals.cpp
        standalone/rw/atomic.cpp
        standalone/rw/clump.cpp
        standalone/rw/plugins.cpp
        standalone/rw/frame.cpp
        standalone/rw/camera.cpp
        standalone/rw/light.cpp
        standalone/rw/world.cpp
        standalone/rw/material.cpp
        standalone/rw/geometry.cpp
        standalone/rw/geometry_strip.cpp
        standalone/rw/raster.cpp
        standalone/rw/image.cpp
        standalone/rw/texture.cpp
        standalone/rw/texdict.cpp
    )
    target_compile_features(rw_pipeline_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_pipeline_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_pipeline_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_pipeline_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_pipeline_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    # P2B-23 unit test of the MatFX AllInOne pipeline on the façade (env map / dual / UV transform; needs a D3D9 HAL device: Wine/wined3d is fine)
    add_executable(rw_matfx_pipeline_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_matfx_pipeline_test.cpp"
        standalone/rw/pipeline.cpp
        standalone/rw/pipeline_matfx.cpp
        standalone/rw/matfx.cpp
        standalone/rw/uvanim.cpp
        standalone/rw/renderstate.cpp
        standalone/rw/rwd3d_ff.cpp
        standalone/rw/rwglobals.cpp
        standalone/rw/atomic.cpp
        standalone/rw/clump.cpp
        standalone/rw/plugins.cpp
        standalone/rw/math.cpp
        standalone/rw/rwmath_tables.cpp
        standalone/rw/stream.cpp
        standalone/rw/frame.cpp
        standalone/rw/camera.cpp
        standalone/rw/light.cpp
        standalone/rw/world.cpp
        standalone/rw/material.cpp
        standalone/rw/geometry.cpp
        standalone/rw/geometry_strip.cpp
        standalone/rw/raster.cpp
        standalone/rw/image.cpp
        standalone/rw/texture.cpp
        standalone/rw/texdict.cpp
    )
    target_compile_features(rw_matfx_pipeline_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_matfx_pipeline_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_matfx_pipeline_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_matfx_pipeline_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_matfx_pipeline_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    # P2B-24 unit test of the skin pipeline on the façade (peds: vs_1_1 path / CPU skinning); needs a D3D9 HAL device
    add_executable(rw_skin_pipeline_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_skin_pipeline_test.cpp"
        standalone/rw/pipeline.cpp
        standalone/rw/pipeline_skin.cpp
        standalone/rw/pipeline_skin_cpu.cpp
        standalone/rw/skin_vs.cpp
        standalone/rw/skin.cpp
        standalone/rw/hanim.cpp
        standalone/rw/renderstate.cpp
        standalone/rw/rwd3d_ff.cpp
        standalone/rw/rwglobals.cpp
        standalone/rw/atomic.cpp
        standalone/rw/clump.cpp
        standalone/rw/plugins.cpp
        standalone/rw/math.cpp
        standalone/rw/rwmath_tables.cpp
        standalone/rw/stream.cpp
        standalone/rw/frame.cpp
        standalone/rw/camera.cpp
        standalone/rw/light.cpp
        standalone/rw/world.cpp
        standalone/rw/material.cpp
        standalone/rw/geometry.cpp
        standalone/rw/geometry_strip.cpp
        standalone/rw/raster.cpp
        standalone/rw/image.cpp
        standalone/rw/texture.cpp
        standalone/rw/texdict.cpp
    )
    target_compile_features(rw_skin_pipeline_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_skin_pipeline_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_skin_pipeline_test BEFORE PRIVATE fakerw)
    target_include_directories(rw_skin_pipeline_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(rw_skin_pipeline_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_skin_pipeline_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    # P2B-24a differential test of the skin vertex-shader composer (skin_vs.cpp) against the exe's own composer (exe-oracle machinery of game_oracle_test: placeholder image + env RW_EXE_ORACLE);
    # the device part needs a D3D9 HAL device (Wine/wined3d is fine) and d3dcompiler_47.dll
    add_executable(rw_skin_vs_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_skin_vs_test.cpp"
        standalone/rw/skin_vs.cpp
    )
    target_compile_features(rw_skin_vs_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_skin_vs_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_skin_vs_test BEFORE PRIVATE fakerw)
    target_include_directories(rw_skin_vs_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(rw_skin_vs_test PRIVATE librw d3d9 user32)
    target_sources(rw_skin_vs_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_rsv_sources rw_skin_vs_test SOURCES)
    list(REMOVE_ITEM _rsv_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET rw_skin_vs_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_rsv_sources})
    target_link_options(rw_skin_vs_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(rw_skin_vs_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)

    # P2B-24b differential test of the skin HW render path (pipeline_skin_core.h: bone matrices, world-matrix module, lights, the whole 0x7C8060) against the exe's own machine
    # code (exe-oracle machinery of game_oracle_test: placeholder image + env RW_EXE_ORACLE); needs no D3D device (the device is a fake COM object)
    add_executable(rw_skin_hw_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_skin_hw_oracle_test.cpp"
    )
    target_compile_features(rw_skin_hw_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_skin_hw_oracle_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_skin_hw_oracle_test BEFORE PRIVATE fakerw)
    target_include_directories(rw_skin_hw_oracle_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw")
    target_link_libraries(rw_skin_hw_oracle_test PRIVATE librw d3d9 user32)
    target_sources(rw_skin_hw_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_rsh_sources rw_skin_hw_oracle_test SOURCES)
    list(REMOVE_ITEM _rsh_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET rw_skin_hw_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_rsh_sources})
    target_link_options(rw_skin_hw_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(rw_skin_hw_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # P2B-24c differential test of the skin CPU path (pipeline_skin_cpu_core.h: SSE / x87 bone matrices, the eight skinning kernels, the dispatch) against the exe's own machine code
    # (exe-oracle machinery of game_oracle_test: placeholder image + env RW_EXE_ORACLE); needs no D3D device
    add_executable(rw_skin_cpu_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_skin_cpu_oracle_test.cpp"
    )
    target_compile_features(rw_skin_cpu_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_skin_cpu_oracle_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_skin_cpu_oracle_test BEFORE PRIVATE fakerw)
    target_include_directories(rw_skin_cpu_oracle_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw")
    target_link_libraries(rw_skin_cpu_oracle_test PRIVATE librw d3d9 user32)
    target_sources(rw_skin_cpu_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_rsc_sources rw_skin_cpu_oracle_test SOURCES)
    list(REMOVE_ITEM _rsc_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET rw_skin_cpu_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_rsc_sources})
    target_link_options(rw_skin_cpu_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(rw_skin_cpu_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # P2B-08 unit test of the game's four custom pipelines (CCustomBuildingPipeline / DNPipeline / CCustomCarEnvMapPipeline + plugins) on the façade: the REAL game
    # TUs are compiled into the test with the game's compile settings and PCH (StdInc.h), the few game-side symbols they call (CMemoryMgr, g_fx, ...) and the data
    # image globals they read (StaticRef at fixed addresses) are stubbed by the test. Needs a D3D9 HAL device (Wine/wined3d is fine); optional SA DFF paths on the command line.
    add_executable(rw_custom_pipelines_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_custom_pipelines_test.cpp"
        game_sa/Pipelines/CustomBuilding/CustomBuildingPipeline.cpp
        game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.cpp
        game_sa/Pipelines/CustomBuilding/CustomBuildingRenderer.cpp
        game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp
        game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline_EnvMat.cpp
        game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline_EnvAtm.cpp
        game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline_SpecMat.cpp
        game_sa/Plugins/PipelinePlugin/PipelinePlugin.cpp
        standalone/rw/engine.cpp
        standalone/rw/platform.cpp
        standalone/rw/stream.cpp
        standalone/rw/math.cpp
        standalone/rw/pipeline.cpp
        standalone/rw/renderstate.cpp
        standalone/rw/rwd3d_ff.cpp
        standalone/rw/rwglobals.cpp
        standalone/rw/atomic.cpp
        standalone/rw/clump.cpp
        standalone/rw/plugins.cpp
        standalone/rw/frame.cpp
        standalone/rw/camera.cpp
        standalone/rw/light.cpp
        standalone/rw/world.cpp
        standalone/rw/material.cpp
        standalone/rw/geometry.cpp
        standalone/rw/geometry_strip.cpp
        standalone/rw/raster.cpp
        standalone/rw/image.cpp
        standalone/rw/texture.cpp
        standalone/rw/texdict.cpp
    )
    target_compile_features(rw_custom_pipelines_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_custom_pipelines_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(rw_custom_pipelines_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(rw_custom_pipelines_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(rw_custom_pipelines_test PRIVATE StdInc.cpp) # own PCH: REUSE_FROM would make the test depend on the whole game target
    target_link_libraries(rw_custom_pipelines_test PRIVATE librw spdlog::spdlog Tracy::TracyClient d3d9 user32)
    # the data image globals (StaticRef at 0x8D.... / 0xC0....) need the exe image itself to cover the original data range: the same 9 MB placeholder as the game exe
    # (a VirtualAlloc there fails: Wine/Windows map NLS tables and the heap in low memory), made writable by the test at startup
    target_sources(rw_custom_pipelines_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_rcp_sources rw_custom_pipelines_test SOURCES)
    list(REMOVE_ITEM _rcp_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET rw_custom_pipelines_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_rcp_sources})
    target_link_options(rw_custom_pipelines_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>") # the PCH object drags in vtables of game classes whose methods are not linked here
    set_target_properties(rw_custom_pipelines_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # P2E game-function differential test (exe-oracle): the REAL game TUs of the numerically sensitive ports (CCollision / CGeneral / CVector / CMatrix / CQuaternion /
    # CAnimBlendNode) are compiled into the test with the game's flags (x87 /arch:IA32) and PCH; the test maps gta_sa_compact.exe (env RW_EXE_ORACLE) at its original
    # addresses inside the 9 MB placeholder image and calls the exe's own machine code next to the C++ port with random inputs (bit-exact compare, PC=24 and PC=53).
    add_executable(game_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/game_oracle_test.cpp"
        game_sa/Collision/Collision.cpp
        game_sa/Collision/CollisionPrimitives.cpp
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Core/Quaternion.cpp
        game_sa/Animation/AnimBlendNode.cpp
    )
    target_compile_features(game_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(game_oracle_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(game_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(game_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(game_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(game_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(game_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_go_sources game_oracle_test SOURCES)
    list(REMOVE_ITEM _go_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET game_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_go_sources})
    target_link_options(game_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(game_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # P2E-2 physics/animation/curves/pathfind/timecycle differential test (exe-oracle, same machinery as game_oracle_test; the game TUs are compiled in)
    add_executable(physics_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/physics_oracle_test.cpp"
        game_sa/Collision/Collision.cpp
        game_sa/Collision/CollisionPrimitives.cpp
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Vector2D.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Core/Quaternion.cpp
        game_sa/Animation/AnimBlendNode.cpp
        game_sa/Curves.cpp
        game_sa/Entity/Physical.cpp
        game_sa/Collision/ColourSet.cpp
        game_sa/Collision/Box.cpp
        game_sa/Collision/Sphere.cpp
        game_sa/Shadows.cpp
        game_sa/Core/Vector2D.cpp
        game_sa/Entity/Vehicle/Vehicle.cpp
        game_sa/Entity/Vehicle/Trailer.cpp
        game_sa/Collision/ColSphere.cpp
        game_sa/Collision/ColBox.cpp
        game_sa/Collision/ColLine.cpp
        game_sa/Collision/ColTriangle.cpp
        game_sa/Collision/Sphere.cpp
        game_sa/Collision/BoundingBox.cpp
        game_sa/Collision/ColPoint.cpp
        game_sa/Weapon.cpp
        game_sa/Entity/Placeable.cpp
        game_sa/CullZones.cpp
        game_sa/Weather.cpp
        game_sa/TimeCycle.cpp
        game_sa/Clock.cpp
        game_sa/Timer.cpp
        game_sa/CarCtrl.cpp
        game_sa/PathFind.cpp
        game_sa/TrafficLights.cpp
        game_sa/Weather.cpp
        game_sa/TimeCycle.cpp
        game_sa/Clock.cpp
        game_sa/Timer.cpp
        standalone/FtolExe.cpp
    )
    target_compile_features(physics_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(physics_oracle_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(physics_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(physics_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(physics_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(physics_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(physics_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_po_sources physics_oracle_test SOURCES)
    list(REMOVE_ITEM _po_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET physics_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_po_sources})
    target_link_options(physics_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(physics_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # P2E-4 world differential test (audio math, radar/HUD/sprites, water/weather/fire/explosion/cull zones/population/garages/pickups, trains/boats/helis/planes/ropes); same machinery as physics_oracle_test
    add_executable(world_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/world_oracle_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Vector2D.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Core/Quaternion.cpp
        game_sa/Audio/AEAudioUtility.cpp
        game_sa/Audio/Entities/AEVehicleAudioEntity.cpp
        game_sa/Audio/Entities/AECollisionAudioEntity.cpp
        game_sa/Maths.cpp
        game_sa/WaterLevel.cpp
        game_sa/CullZones.cpp
        game_sa/Weather.cpp
        game_sa/Pickups.cpp
        game_sa/TrainNode.cpp
        game_sa/Collision/ColPoint.cpp
        game_sa/Entity/Vehicle/Train.cpp
        game_sa/Entity/Vehicle/Boat.cpp
        game_sa/Collision/Collision.cpp
        game_sa/Collision/CollisionPrimitives.cpp
        game_sa/Radar.cpp
        game_sa/MenuManager.cpp
        game_sa/Camera.cpp
        game_sa/Cam.cpp
        game_sa/HudColours.cpp
        game_sa/Font.cpp
        game_sa/Entity/Placeable.cpp
        game_sa/Sprite.cpp
        game_sa/Sprite2d.cpp
        game_sa/PointLights.cpp
        game_sa/Scene.cpp
        game_sa/Skidmark.cpp
        game_sa/Skidmarks.cpp
        game_sa/Coronas.cpp
        game_sa/RegisteredCorona.cpp
        game_sa/Clouds.cpp
        game_sa/Core/Rect.cpp
        standalone/FtolExe.cpp
    )
    target_compile_features(world_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(world_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)   # the run build's own definitions (NOTSA_STANDALONE_RUN, USE_DSOUND, ...): the audio / HUD headers need them
    target_include_directories(world_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(world_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(world_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(world_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(world_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_wo_sources world_oracle_test SOURCES)
    list(REMOVE_ITEM _wo_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET world_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_wo_sources})
    target_link_options(world_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(world_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # P2E-5 game-logic differential test (wanted/crime, decision makers, population/car choice, scanners, zones/cheats/stats); same machinery as world_oracle_test
    add_executable(logic_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/logic_oracle_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Vector2D.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Core/Quaternion.cpp
        game_sa/Crime.cpp
        game_sa/Wanted.cpp
        game_sa/Stats.cpp
        game_sa/GameLogic.cpp
        game_sa/Pools.cpp
        game_sa/World_Helper.cpp
        game_sa/Entity/Ped/Ped.cpp
        game_sa/Entity/Ped/PlayerPed.cpp
        game_sa/Entity/Vehicle/Vehicle.cpp
        game_sa/GangWars.cpp
        game_sa/3dMarkers.cpp   # logic_oracle_notsa.inc: the marker reset of C3dMarkers::Init
        game_sa/Darkel.cpp
        game_sa/Game.cpp
        game_sa/Collision/Collision.cpp
        game_sa/Collision/CollisionPrimitives.cpp
        game_sa/Collision/ColPoint.cpp
        game_sa/PedType.cpp
        game_sa/PedIntelligence.cpp
        game_sa/Tasks/TaskManager.cpp
        game_sa/Tasks/Task.cpp
        # --- sources needed by logic_oracle_dm.inc
        game_sa/DecisionMakers/Decision.cpp
        game_sa/DecisionMakers/DecisionMaker.cpp
        game_sa/DecisionMakers/DecisionMakerTypes.cpp
        game_sa/DecisionMakers/DecisionMakerTypesFileLoader.cpp
        game_sa/DecisionMakers/DecisionSimple.cpp
        # --- sources needed by logic_oracle_pop.inc
        game_sa/Population.cpp
        game_sa/Gangs.cpp
        game_sa/CarCtrl.cpp
        game_sa/Core/Rect.cpp
        game_sa/PopCycle.cpp
        game_sa/LoadedCarGroup.cpp
        game_sa/Clock.cpp
        game_sa/Cheat.cpp
        game_sa/CullZones.cpp
        game_sa/ModelIndices.cpp
        game_sa/ActiveOccluder.cpp
        game_sa/Streaming.cpp
        game_sa/StreamingInfo.cpp
        game_sa/Scripts/TheScripts.cpp
        # --- sources needed by logic_oracle_scan.inc
        game_sa/PedGeometryAnalyser.cpp
        game_sa/Collision/ColSphere.cpp
        game_sa/Entity/Placeable.cpp
        game_sa/Entity/Entity.cpp
        game_sa/EntityScanner.cpp
        game_sa/PedScanner.cpp
        game_sa/VehicleScanner.cpp
        game_sa/References.cpp
        # --- sources needed by logic_oracle_fixes.inc (FIX-AUDIT: functions that carry a notsa::bugfixes switch; the run build has the switch OFF = the exe's behaviour)
        game_sa/CustomCarPlateMgr.cpp
        game_sa/Animation/AnimBlendSequence.cpp
        # --- sources needed by logic_oracle_misc.inc
        game_sa/Collision/Box.cpp
        game_sa/Collision/Sphere.cpp
        game_sa/Core/KeyGen.cpp
        game_sa/TheZones.cpp
        game_sa/Zone.cpp
        game_sa/Acquaintance.cpp
        game_sa/Ropes.cpp
        game_sa/Rope.cpp
        game_sa/ModelIndices.cpp
        game_sa/Collision/Collision.cpp
        game_sa/LoadedCarGroup.cpp
        game_sa/Models/BaseModelInfo.cpp
        game_sa/TxdStore.cpp
        game_sa/Core/PtrNodeDoubleLink.cpp
        game_sa/Core/PtrNodeSingleLink.cpp
        game_sa/TimeCycle.cpp
        game_sa/Weather.cpp
        game_sa/Timer.cpp
        standalone/FtolExe.cpp
        # --- sources needed by logic_oracle_taskrefs.inc (VT-OVERRIDES: task constructor / destructor reference bookkeeping, CCopPed::RemoveCriminalToKill)
        game_sa/Animation/AnimBlendAssociation.cpp
        game_sa/Entity/Ped/CopPed.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleChoking.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleStealthKill.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleCarFallOut.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleCarGetIn.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleCarJumpOut.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleCarCloseDoorFromInside.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleCarCloseDoorFromOutside.cpp
        game_sa/Tasks/TaskTypes/TaskComplexEvasiveDiveAndGetUp.cpp
    )
    target_compile_features(logic_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(logic_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)   # the run build's own definitions (NOTSA_STANDALONE_RUN, USE_DSOUND, ...): the audio / HUD headers need them
    target_include_directories(logic_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(logic_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(logic_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(logic_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(logic_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_lo_sources logic_oracle_test SOURCES)
    list(REMOVE_ITEM _lo_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET logic_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_lo_sources})
    target_link_options(logic_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(logic_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W1 wave 1: ported functions without an exe-oracle row so far (surface table, camera, AI decisions, events, path nodes ...); harness = logic_oracle_test's, TUs listed per area below
    add_executable(wave1_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Vector2D.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Core/Quaternion.cpp
        game_sa/Timer.cpp
        game_sa/Weather.cpp
        standalone/FtolExe.cpp
        # --- wave1_oracle_surface.inc
        game_sa/SurfaceInfos_c.cpp
        # --- wave1_oracle_camera.inc
        game_sa/Camera.cpp
        # --- wave1_oracle_peds.inc
        game_sa/Entity/Ped/Ped.cpp
        # --- wave1_oracle_ai.inc
        game_sa/PlayerRelationshipRecorder.cpp
        game_sa/StuckCarCheck.cpp
        game_sa/PedGroupMembership.cpp
        game_sa/CarAI.cpp
        # --- wave1_oracle_entities.inc
        game_sa/Entity/Placeable.cpp
        game_sa/Events/EventDanger.cpp
        game_sa/Events/EventCarUpsideDown.cpp
        game_sa/Events/EventChatPartner.cpp
        game_sa/Events/EventDraggedOutCar.cpp
        game_sa/Events/EventGotKnockedOverByCar.cpp
        game_sa/Events/EventCopCarBeingStolen.cpp
        game_sa/Events/EventBuildingCollision.cpp
    )
    # tiny constant-returning overrides (GetTaskType / GetEventPriority / ... ~440 rows, ~280 TUs) live in their own executable: tests/standalone/tools/gen_const_rows.py regenerates the rows and the source list
    include("${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_consts_sources.cmake")
    add_executable(wave1_consts_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Vector2D.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Core/Quaternion.cpp
        game_sa/Timer.cpp
        game_sa/Weather.cpp
        standalone/FtolExe.cpp
        ${W1_CONST_SOURCES}
    )
    target_compile_definitions(wave1_consts_oracle_test PRIVATE W1_CONSTS_ONLY)
    foreach(_w1t wave1_oracle_test wave1_consts_oracle_test)
        target_compile_features(${_w1t} PRIVATE cxx_std_23)
        target_compile_definitions(${_w1t} PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
        target_include_directories(${_w1t} PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
        target_compile_options(${_w1t} PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
        target_precompile_headers(${_w1t} PRIVATE StdInc.cpp)
        target_link_libraries(${_w1t} PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
        target_sources(${_w1t} PRIVATE "${STANDALONE_PAD_OBJ}")
        get_target_property(_w1_sources_${_w1t} ${_w1t} SOURCES)
        list(REMOVE_ITEM _w1_sources_${_w1t} "${STANDALONE_PAD_OBJ}")
        set_property(TARGET ${_w1t} PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w1_sources_${_w1t}})
        target_link_options(${_w1t} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
        set_target_properties(${_w1t} PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    endforeach()
    # ORACLE-W2: allocator-backed rows (tasks / events: constructors, Clone, sub-task factories ...): same harness source as wave 1 with W2_ONLY; the exe's CRT heap / lock imports are redirected to a
    # deterministic arena shared with the port (tests/standalone/oracle_alloc.h); translation units: wave2_oracle_sources.cmake (+ wave2_oracle_gen_sources.cmake written by tools/gen_w2_rows.py)
    include("${PROJECT_SOURCE_DIR}/tests/standalone/wave2_oracle_sources.cmake")
    if(EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/wave2_oracle_gen_sources.cmake")
        include("${PROJECT_SOURCE_DIR}/tests/standalone/wave2_oracle_gen_sources.cmake")
    endif()
    if(EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/wave2_oracle_gen_func_sources.cmake")
        include("${PROJECT_SOURCE_DIR}/tests/standalone/wave2_oracle_gen_func_sources.cmake")
    endif()
    add_executable(wave2_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        ${W2_GEN_SOURCES}
        ${W2_FUNC_SOURCES}
    )
    # LINK-CALLEES: callee translation units the port reaches from the rows of this target (was: unresolved -> port fault at 0x400000, rest of the function not compared)
    target_sources(wave2_oracle_test PRIVATE game_sa/Weapon.cpp game_sa/WeaponInfo.cpp game_sa/Audio/Entities/AEPedSpeechAudioEntity.cpp game_sa/DamageManager.cpp game_sa/CarEnterExit.cpp game_sa/Tasks/TaskTypes/TaskSimpleCarSetPedInAsDriver.cpp)
    target_compile_definitions(wave2_oracle_test PRIVATE W2_ONLY)
    target_compile_features(wave2_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave2_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave2_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave2_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave2_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave2_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave2_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w2_sources wave2_oracle_test SOURCES)
    list(REMOVE_ITEM _w2_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave2_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w2_sources})
    target_link_options(wave2_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave2_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-VEH: exe-oracle rows of game_sa/Entity/Vehicle (A + B functions) on the W2 harness; rows: tests/standalone/wave3_oracle_veh.inc (+ wave3_oracle_gen_veh.inc written by tools/gen_w3_veh.py)
    if(EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_veh_sources.cmake")
        include("${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_veh_sources.cmake")
    endif()
    add_executable(wave3_vehicle_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        ${W3_VEH_SOURCES}
    )
    # LINK-CALLEES: callee translation units the port reaches from the rows of this target (was: unresolved -> port fault at 0x400000, rest of the function not compared)
    target_sources(wave3_vehicle_oracle_test PRIVATE game_sa/Pad.cpp game_sa/Stats.cpp game_sa/Garages.cpp game_sa/Ropes.cpp)
    target_compile_definitions(wave3_vehicle_oracle_test PRIVATE W2_ONLY W3_VEH_ONLY)
    target_compile_features(wave3_vehicle_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_vehicle_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_vehicle_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_vehicle_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_vehicle_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_vehicle_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_vehicle_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3v_sources wave3_vehicle_oracle_test SOURCES)
    list(REMOVE_ITEM _w3v_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_vehicle_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3v_sources})
    target_link_options(wave3_vehicle_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP;/SAFESEH:NO>")
    set_target_properties(wave3_vehicle_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-PED: exe-oracle rows of game_sa/Entity/Ped (A + B functions) on the W2 harness; rows: tests/standalone/wave3_oracle_ped.inc (+ wave3_oracle_gen_ped.inc written by tools/gen_w3_ped.py)
    if(EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_ped_sources.cmake")
        include("${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_ped_sources.cmake")
    endif()
    add_executable(wave3_ped_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        ${W3_PED_SOURCES}
        game_sa/WeaponEffects.cpp   # CPlayerPed::ClearWeaponTarget -> CWeaponEffects::ClearCrossHair: the unit was not linked, every player-type case ended as an agreed fault
        game_sa/TimeCycle.cpp   # CPhysical::GetLightingFromCol (CPlayerPed::IsHidden) calls the CTimeCycle ambient getters: without the unit the port side called 0x400000 and every non-vehicle case ended as an agreed fault
    )
    # LINK-CALLEES: callee translation units the port reaches from the rows of this target (was: unresolved -> port fault at 0x400000, rest of the function not compared)
    target_sources(wave3_ped_oracle_test PRIVATE game_sa/Radar.cpp)
    target_compile_definitions(wave3_ped_oracle_test PRIVATE W2_ONLY W3_PED_ONLY)
    target_compile_features(wave3_ped_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_ped_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_ped_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_ped_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_ped_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_ped_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_ped_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3p_sources wave3_ped_oracle_test SOURCES)
    list(REMOVE_ITEM _w3p_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_ped_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3p_sources})
    target_link_options(wave3_ped_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP;/SAFESEH:NO>")
    set_target_properties(wave3_ped_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-EVENTS: exe-oracle rows of game_sa/Events (A + B functions) on the W2 harness (all W2 translation units + the ped ones); rows: tests/standalone/wave3_oracle_ev.inc
    # ORACLE-C-EVENTS: class C rows (generated: wave3_oracle_gen_events.inc by tools/gen_w3_tasks.py --sub game_sa/Events --prefix events) use the task fixture, so the task translation units are linked as well
    if(EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_tasks_sources.cmake")
        include("${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_tasks_sources.cmake")
    endif()
    if(EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_events_sources.cmake")
        include("${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_events_sources.cmake")
    endif()
    add_executable(wave3_events_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        ${W2_GEN_SOURCES}
        ${W2_FUNC_SOURCES}
        ${W3_PED_SOURCES}
        ${W3_TASKS_SOURCES}
        ${W3_EVENTS_SOURCES}
        game_sa/WeaponEffects.cpp
        game_sa/TimeCycle.cpp
        game_sa/Events/GroupEventHandler.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleDead.cpp
        game_sa/Events/EventScriptedAttractor.cpp
        game_sa/Events/EventScanner.cpp
    )
    # LINK-CALLEES: callee translation units the port reaches from the rows of this target (was: unresolved -> port fault at 0x400000, rest of the function not compared)
    target_sources(wave3_events_oracle_test PRIVATE game_sa/PedType.cpp game_sa/CullZones.cpp game_sa/VehicleScanner.cpp)
    target_compile_definitions(wave3_events_oracle_test PRIVATE W2_ONLY W3_EV_ONLY)
    target_compile_features(wave3_events_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_events_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_events_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_events_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_events_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_events_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_events_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3e_sources wave3_events_oracle_test SOURCES)
    list(REMOVE_ITEM _w3e_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_events_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3e_sources})
    target_link_options(wave3_events_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP;/SAFESEH:NO>")
    set_target_properties(wave3_events_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-TASKS: exe-oracle rows of game_sa/Tasks/TaskTypes (A + B functions) on the W2 harness (all W2 translation units + the ped / events ones); rows: tests/standalone/wave3_oracle_tasks.inc (+ wave3_oracle_gen_tasks.inc written by tools/gen_w3_tasks.py)
    if(EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_tasks_sources.cmake")
        include("${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_tasks_sources.cmake")
    endif()
    add_executable(wave3_tasks_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        ${W2_GEN_SOURCES}
        ${W2_FUNC_SOURCES}
        ${W3_PED_SOURCES}
        ${W3_TASKS_SOURCES}
        game_sa/WeaponEffects.cpp
        game_sa/TimeCycle.cpp
        game_sa/Events/GroupEventHandler.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleDead.cpp
        game_sa/Events/EventScriptedAttractor.cpp
        game_sa/Events/EventScanner.cpp
    )
    # LINK-CALLEES: callee translation units the port reaches from the rows of this target (was: unresolved -> port fault at 0x400000, rest of the function not compared)
    target_sources(wave3_tasks_oracle_test PRIVATE game_sa/PedType.cpp game_sa/CullZones.cpp game_sa/VehicleScanner.cpp)
    target_compile_definitions(wave3_tasks_oracle_test PRIVATE W2_ONLY W3_TASKS_ONLY)
    target_compile_features(wave3_tasks_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_tasks_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_tasks_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_tasks_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_tasks_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_tasks_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_tasks_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3t_sources wave3_tasks_oracle_test SOURCES)
    list(REMOVE_ITEM _w3t_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_tasks_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3t_sources})
    target_link_options(wave3_tasks_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP;/SAFESEH:NO>")
    set_target_properties(wave3_tasks_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-COLL: exe-oracle rows of game_sa/Collision + game_sa/Core (class A/B functions) on the game_oracle_test machinery (tests/standalone/wave3_coll_oracle_*.inc, selected by W3_COLL_ONLY)
    add_executable(wave3_coll_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/game_oracle_test.cpp"
        game_sa/Collision/Collision.cpp
        game_sa/Collision/CollisionPrimitives.cpp
        game_sa/Collision/CollisionData.cpp
        game_sa/Collision/Box.cpp
        game_sa/Collision/Sphere.cpp
        game_sa/Collision/ColLine.cpp
        game_sa/Collision/ColSphere.cpp
        game_sa/Collision/ColTrianglePlane.cpp
        game_sa/Collision/ColModel.cpp
        game_sa/Collision/ColStore.cpp
        game_sa/Collision/ColAccel.cpp
        game_sa/Core/EntryInfoList.cpp
        game_sa/Core/EntryInfoNode.cpp
        game_sa/Core/KeyGen.cpp
        game_sa/Collision/ColModel.cpp
        game_sa/Collision/ColStore.cpp
        game_sa/Collision/ColAccel.cpp
        game_sa/Core/EntryInfoList.cpp
        game_sa/Core/EntryInfoNode.cpp
        game_sa/Core/KeyGen.cpp
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Vector2D.cpp
        game_sa/Core/Rect.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Core/MatrixLink.cpp
        game_sa/Core/MatrixLinkList.cpp
        game_sa/Core/Quaternion.cpp
        game_sa/Animation/AnimBlendNode.cpp
    )
    target_compile_features(wave3_coll_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_coll_oracle_test PRIVATE NOTSA_RW_LIBRW RW_D3D9 W3_COLL_ONLY NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_coll_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_coll_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_coll_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_coll_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_coll_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3c_sources wave3_coll_oracle_test SOURCES)
    list(REMOVE_ITEM _w3c_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_coll_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3c_sources})
    target_link_options(wave3_coll_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(wave3_coll_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-GAMEPLAY: exe-oracle rows of the gameplay root files (A + B functions) on the W2 harness; rows: tests/standalone/wave3_oracle_gp.inc (+ wave3_oracle_gen_gp.inc written by tools/gen_w3_gp.py)
    if(EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_gp_sources.cmake")
        include("${PROJECT_SOURCE_DIR}/tests/standalone/wave3_oracle_gen_gp_sources.cmake")
    endif()
    add_executable(wave3_gameplay_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        ${W3_GP_SOURCES}
        game_sa/TimeCycle.cpp
    )
    # LINK-CALLEES: callee translation units the port reaches from the rows of this target (was: unresolved -> port fault at 0x400000, rest of the function not compared)
    target_sources(wave3_gameplay_oracle_test PRIVATE game_sa/Radar.cpp game_sa/TheZones.cpp game_sa/Audio/Entities/AEDoorAudioEntity.cpp game_sa/Models/VehicleModelInfo.cpp)
    target_compile_definitions(wave3_gameplay_oracle_test PRIVATE W2_ONLY W3_GP_ONLY)
    target_compile_features(wave3_gameplay_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_gameplay_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_gameplay_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_gameplay_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_gameplay_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_gameplay_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_gameplay_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3g_sources wave3_gameplay_oracle_test SOURCES)
    list(REMOVE_ITEM _w3g_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_gameplay_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3g_sources})
    target_link_options(wave3_gameplay_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_gameplay_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-ANIM: exe-oracle rows of game_sa/Animation + Plugins/RpAnimBlendPlugin (A + B functions) on the W2 harness; rows: tests/standalone/wave3_oracle_anim.inc
    add_executable(wave3_anim_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/Animation/AnimBlendHierarchy.cpp
        game_sa/Animation/AnimBlendSequence.cpp
        game_sa/Animation/AnimBlendClumpData.cpp
        game_sa/Animation/AnimBlendAssocGroup.cpp
        game_sa/Animation/AnimBlendStaticAssociation.cpp
        game_sa/Animation/AnimBlendNode.cpp
        game_sa/Plugins/NodeNamePlugin/NodeName.cpp
    )
    target_compile_definitions(wave3_anim_oracle_test PRIVATE W2_ONLY W3_ANIM_ONLY)
    target_compile_features(wave3_anim_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_anim_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_anim_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_anim_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_anim_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_anim_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_anim_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3a_sources wave3_anim_oracle_test SOURCES)
    list(REMOVE_ITEM _w3a_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_anim_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3a_sources})
    target_link_options(wave3_anim_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_anim_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-IK: exe-oracle rows of game_sa/Ragdoll (IKChain, BoneNode, BoneNodeManager, IKChainManager) on bone-node / ped-hierarchy fixtures; rows: tests/standalone/wave3_oracle_ik*.inc
    add_executable(wave3_ik_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        "${PROJECT_SOURCE_DIR}/tests/standalone/oracle_fast.cpp"
        ${W2_BASE_SOURCES}
        game_sa/Ragdoll/IKChain.cpp
        game_sa/Ragdoll/BoneNode.cpp
        game_sa/Ragdoll/BoneNodeManager.cpp
        game_sa/Tasks/TaskManager.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleIKChain.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleIKManager.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleIKLookAt.cpp
        game_sa/Tasks/TaskTypes/TaskSimpleIKPointArm.cpp
        standalone/rw/math.cpp
        standalone/rw/rwmath_tables.cpp
        standalone/rw/rtquat.cpp
        standalone/rw/hanim.cpp
    )
    set_source_files_properties("${PROJECT_SOURCE_DIR}/tests/standalone/oracle_fast.cpp" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    target_compile_definitions(wave3_ik_oracle_test PRIVATE ORACLE_FAST_HELPERS)
    target_compile_definitions(wave3_ik_oracle_test PRIVATE W2_ONLY W3_IK_ONLY)
    target_compile_features(wave3_ik_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_ik_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_ik_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_ik_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_ik_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_ik_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_ik_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3k_sources wave3_ik_oracle_test SOURCES)
    list(REMOVE_ITEM _w3k_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_ik_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3k_sources})
    target_link_options(wave3_ik_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_ik_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-MODELS: exe-oracle rows of game_sa/Models (A + B functions) on model-info fixtures; rows: tests/standalone/wave3_oracle_models*.inc
    add_executable(wave3_models_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/Models/BaseModelInfo.cpp
        game_sa/Models/AtomicModelInfo.cpp
        game_sa/Models/ClumpModelInfo.cpp
        game_sa/Models/DamageAtomicModelInfo.cpp
        game_sa/Models/LodAtomicModelInfo.cpp
        game_sa/Models/LodTimeModelInfo.cpp
        game_sa/Models/TimeModelInfo.cpp
        game_sa/Models/TimeInfo.cpp
        game_sa/Models/PedModelInfo.cpp
        game_sa/Models/WeaponModelInfo.cpp
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave3_models_vehicle_tu.cpp"
        game_sa/Models/ModelInfo.cpp
        game_sa/VisibilityPlugins.cpp
        game_sa/cHandlingDataMgr.cpp
        game_sa/TxdStore.cpp
        game_sa/Cheat.cpp
        game_sa/Plugins/NodeNamePlugin/NodeName.cpp
        game_sa/Plugins/PipelinePlugin/PipelinePlugin.cpp
        game_sa/Plugins/TwoDEffectPlugin/2dEffect.cpp
        game_sa/Pipelines/CustomBuilding/CustomBuildingRenderer.cpp
        game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.cpp
        game_sa/Fx/CarFXRenderer.cpp
        game_sa/RwHelper.cpp
    )
    target_compile_definitions(wave3_models_oracle_test PRIVATE W2_ONLY W3_MODELS_ONLY)
    target_compile_features(wave3_models_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_models_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_models_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_models_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_models_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_models_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_models_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3m_sources wave3_models_oracle_test SOURCES)
    list(REMOVE_ITEM _w3m_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_models_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3m_sources})
    target_link_options(wave3_models_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_models_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-AUDIO: exe-oracle rows of game_sa/Audio/Entities + game_sa/Audio root (A + B functions) on the W2 harness; rows: tests/standalone/wave3_oracle_audio*.inc
    add_executable(wave3_audio_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/Audio/AEAudioEnvironment.cpp
        game_sa/Audio/AEAudioUtility.cpp
        game_sa/Audio/AESmoothFadeThread.cpp
        game_sa/Audio/AESound.cpp
        game_sa/Audio/AEStreamThread.cpp
        game_sa/Audio/AudioEngine.cpp
        game_sa/Audio/AudioZones.cpp
        game_sa/Audio/Entities/AEAudioEntity.cpp
        game_sa/Audio/Entities/AECollisionAudioEntity.cpp
        game_sa/Audio/Entities/AEDoorAudioEntity.cpp
        game_sa/Audio/Entities/AEExplosionAudioEntity.cpp
        game_sa/Audio/Entities/AEFireAudioEntity.cpp
        game_sa/Audio/Entities/AEFrontendAudioEntity.cpp
        game_sa/Audio/Entities/AEGlobalWeaponAudioEntity.cpp
        game_sa/Audio/Entities/AEPedAudioEntity.cpp
        game_sa/Audio/Entities/AEPedSpeechAudioEntity.cpp
        game_sa/Audio/Entities/AEPedWeaponAudioEntity.cpp
        game_sa/Audio/Entities/AEPedlessSpeechAudioEntity.cpp
        game_sa/Audio/Entities/AEPoliceScannerAudioEntity.cpp
        game_sa/Audio/Entities/AEScriptAudioEntity.cpp
        game_sa/Audio/Entities/AETwinLoopSoundEntity.cpp
        game_sa/Audio/Entities/AEVehicleAudioEntity.cpp
        game_sa/Audio/Entities/AEWaterCannonAudioEntity.cpp
        game_sa/Audio/Entities/AEWeaponAudioEntity.cpp
        game_sa/Audio/Entities/AEWeatherAudioEntity.cpp
        game_sa/Collision/Box.cpp
        game_sa/Collision/Sphere.cpp
        game_sa/CompressedBox.cpp

    )
    target_compile_definitions(wave3_audio_oracle_test PRIVATE W2_ONLY W3_AUDIO_ONLY)
    target_compile_features(wave3_audio_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_audio_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_audio_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_audio_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_audio_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_audio_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_audio_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3au_sources wave3_audio_oracle_test SOURCES)
    list(REMOVE_ITEM _w3au_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_audio_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3au_sources})
    target_link_options(wave3_audio_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_audio_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-AUDIO2: the same harness + the rows of game_sa/Audio/Hardware/AEAudioHardware.cpp (its real TU linked, the recording stubs of CAEAudioHardware are left out of this build): tests/standalone/wave3_oracle_audiohw.inc
    add_executable(wave3_audiohw_oracle_test EXCLUDE_FROM_ALL ${_w3au_sources}
        game_sa/Audio/Hardware/AEAudioHardware.cpp
        game_sa/Audio/Loaders/AEMP3BankLoader.cpp
    )
    target_compile_definitions(wave3_audiohw_oracle_test PRIVATE W2_ONLY W3_AUDIO_ONLY W3_AUDIOHW)
    target_compile_features(wave3_audiohw_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_audiohw_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_audiohw_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_audiohw_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_audiohw_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_audiohw_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    set_property(TARGET wave3_audiohw_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3au_sources} game_sa/Audio/Hardware/AEAudioHardware.cpp game_sa/Audio/Loaders/AEMP3BankLoader.cpp)
    target_link_options(wave3_audiohw_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_audiohw_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-AUDLOAD: the same harness + the rows of game_sa/Audio/Loaders (the loader TUs linked for real; no file IO is reached by the rows): tests/standalone/wave3_oracle_audload.inc
    set(_W3AL_LOADERS game_sa/Audio/Loaders/AEBankLoader.cpp game_sa/Audio/Loaders/AEDataStream.cpp game_sa/Audio/Loaders/AEMFDecoder.cpp game_sa/Audio/Loaders/AEMP3BankLoader.cpp
        game_sa/Audio/Loaders/AEMP3TrackLoader.cpp game_sa/Audio/Loaders/AEStreamTransformer.cpp game_sa/Audio/Loaders/AEStreamingDecoder.cpp game_sa/Audio/Loaders/AETrackLoader.cpp
        game_sa/Audio/Loaders/AEVorbisDecoder.cpp game_sa/Audio/Loaders/AEWMADecoder.cpp game_sa/Audio/Loaders/AEWaveDecoder.cpp)
    add_executable(wave3_audload_oracle_test EXCLUDE_FROM_ALL ${_w3au_sources} ${_W3AL_LOADERS})
    target_compile_definitions(wave3_audload_oracle_test PRIVATE W2_ONLY W3_AUDIO_ONLY W3_AUDLOAD)
    target_compile_features(wave3_audload_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_audload_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_audload_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_audload_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_audload_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_audload_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    set_property(TARGET wave3_audload_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3au_sources} ${_W3AL_LOADERS})
    target_link_options(wave3_audload_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_audload_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-ENTITY: exe-oracle rows of game_sa/Entity (CEntity / CPhysical / CPlaceable / CBuilding / CAnimatedBuilding, A + B functions) on entity fixtures; rows: tests/standalone/wave3_oracle_entity*.inc
    set(_W3E_BASE ${W2_BASE_SOURCES})
    list(REMOVE_ITEM _W3E_BASE game_sa/Entity/Entity.cpp)
    add_executable(wave3_entity_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${_W3E_BASE}
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave3_entity_tu.cpp"
        game_sa/Entity/Physical.cpp
        game_sa/Entity/Building.cpp
        game_sa/Entity/AnimatedBuilding.cpp
        game_sa/Entity/Object/Object.cpp
        game_sa/Entity/Dummy/Dummy.cpp
        game_sa/TimeCycle.cpp
        game_sa/Glass.cpp
        game_sa/Camera.cpp
        game_sa/Escalator.cpp
        game_sa/Escalators.cpp
        game_sa/TrafficLights.cpp
        game_sa/Models/BaseModelInfo.cpp
        standalone/rw/math.cpp
        standalone/rw/rwmath_tables.cpp
        game_sa/Clock.cpp
        game_sa/SimpleTransform.cpp
        game_sa/common.cpp
        game_sa/ModelIndices.cpp
        game_sa/ActiveOccluder.cpp
        game_sa/Core/Rect.cpp
        game_sa/Core/MatrixLink.cpp
        game_sa/Core/MatrixLinkList.cpp
    )
    target_compile_definitions(wave3_entity_oracle_test PRIVATE W2_ONLY W3_ENTITY_ONLY)
    target_compile_features(wave3_entity_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_entity_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_entity_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_entity_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_entity_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_entity_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_entity_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3en_sources wave3_entity_oracle_test SOURCES)
    list(REMOVE_ITEM _w3en_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_entity_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3en_sources})
    target_link_options(wave3_entity_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_entity_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-INTERIOR: exe-oracle rows of game_sa/Interior (Interior_c / InteriorGroup_c / InteriorManager_c / Furniture*, A + B functions) on interior fixtures; rows: tests/standalone/wave3_oracle_interior*.inc
    set(_W3I_BASE ${W2_BASE_SOURCES})
    add_executable(wave3_interior_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${_W3I_BASE}
        game_sa/Interior/Interior_c.cpp
        game_sa/Interior/InteriorGroup_c.cpp
        game_sa/Interior/InteriorManager_c.cpp
        game_sa/Interior/FurnitureGroup_c.cpp
        game_sa/Interior/FurnitureSubGroup_c.cpp
        game_sa/Interior/FurnitureManager_c.cpp
        game_sa/Interior/Furniture_c.cpp
        game_sa/Collision/Box.cpp
        game_sa/PathFind.cpp
    )
    target_compile_definitions(wave3_interior_oracle_test PRIVATE W2_ONLY W3_INTERIOR_ONLY)
    target_compile_features(wave3_interior_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_interior_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_interior_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_interior_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_interior_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_interior_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_interior_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3in_sources wave3_interior_oracle_test SOURCES)
    list(REMOVE_ITEM _w3in_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_interior_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3in_sources})
    target_link_options(wave3_interior_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_interior_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-CAMERA: exe-oracle rows of the camera root files (Cam.cpp, Camera.cpp, IdleCam.cpp; A + B functions) on CCamera / CCam fixtures; rows: tests/standalone/wave3_oracle_camera*.inc
    set(_W3C_BASE ${W2_BASE_SOURCES})
    add_executable(wave3_camera_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${_W3C_BASE}
        game_sa/Cam.cpp
        game_sa/IdleCam.cpp
        game_sa/Pad.cpp
        game_sa/Scene.cpp
        game_sa/Collision/ColSphere.cpp
    )
    # LINK-CALLEES: CEntity::TransformFromObjectSpace (matrix-less entities) calls the free function TransformPoint (common.cpp); was unresolved -> port fault at 0x400000
    target_sources(wave3_camera_oracle_test PRIVATE game_sa/common.cpp)
    target_compile_definitions(wave3_camera_oracle_test PRIVATE W2_ONLY W3_CAMERA_ONLY)
    target_compile_features(wave3_camera_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_camera_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_camera_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_camera_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_camera_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_camera_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_camera_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3ca_sources wave3_camera_oracle_test SOURCES)
    list(REMOVE_ITEM _w3ca_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_camera_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3ca_sources})
    target_link_options(wave3_camera_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_camera_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-SCRIPTS: exe-oracle rows of game_sa/Scripts (CRunningScript / CTheScripts / ScriptsForBrains / Scripted2dEffects helpers, A + B functions) on script-buffer fixtures; rows: tests/standalone/wave3_oracle_scripts*.inc
    set(_W3S_BASE ${W2_BASE_SOURCES})
    add_executable(wave3_scripts_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${_W3S_BASE}
        game_sa/Scripts/RunningScript.cpp
        game_sa/Scripts/TheScripts.cpp
        game_sa/ScriptsForBrains.cpp
        game_sa/Scripts/Scripted2dEffects.cpp
        game_sa/Pad.cpp
    )
    target_compile_definitions(wave3_scripts_oracle_test PRIVATE W2_ONLY W3_SCRIPTS_ONLY)
    target_compile_features(wave3_scripts_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_scripts_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_scripts_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_scripts_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_scripts_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_scripts_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_scripts_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3sc_sources wave3_scripts_oracle_test SOURCES)
    list(REMOVE_ITEM _w3sc_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_scripts_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3sc_sources})
    target_link_options(wave3_scripts_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_scripts_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-WORLD: exe-oracle rows of the world / physics root files (World, World_Helper, PathFind, TheZones, CullZones, Timer, TimeCycle, common, GridRef, Draw; A + B functions) on world / path-node fixtures; rows: tests/standalone/wave3_oracle_world*.inc
    set(_W3W_BASE ${W2_BASE_SOURCES})
    add_executable(wave3_world_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${_W3W_BASE}
        game_sa/World.cpp
        game_sa/PathFind.cpp
        game_sa/TheZones.cpp
        game_sa/CullZones.cpp
        game_sa/TimeCycle.cpp
        game_sa/common.cpp
        game_sa/GridRef.cpp
        game_sa/Draw.cpp
        game_sa/Collision/Collision.cpp
        game_sa/Shadows.cpp
        game_sa/WaterLevel.cpp
        game_sa/RenderBuffer.cpp
        game_sa/Core/Rect.cpp
    )
    target_compile_definitions(wave3_world_oracle_test PRIVATE W2_ONLY W3_WORLD_ONLY)
    target_compile_features(wave3_world_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_world_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_world_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_world_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_world_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_world_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_world_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3wo_sources wave3_world_oracle_test SOURCES)
    list(REMOVE_ITEM _w3wo_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_world_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3wo_sources})
    target_link_options(wave3_world_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_world_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-UI: exe-oracle rows of the HUD / radar / text root files (Messages, Hud, Radar, Font, Sprite2d, Sprite, MenuManager, Text; A + B functions); rows: tests/standalone/wave3_oracle_ui*.inc
    set(_W3U_BASE ${W2_BASE_SOURCES})
    add_executable(wave3_ui_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${_W3U_BASE}
        game_sa/Messages.cpp
        game_sa/Hud.cpp
        game_sa/Radar.cpp
        game_sa/Font.cpp
        game_sa/Sprite2d.cpp
        game_sa/Sprite.cpp
        game_sa/MenuManager.cpp
        game_sa/Frontend/MenuManager_Input.cpp
        game_sa/Pad.cpp
        game_sa/Text/Text.cpp
        game_sa/Text/KeyArray.cpp
        game_sa/Text/Data.cpp
        game_sa/Text/MissionTextOffsets.cpp
    )
    # LINK-CALLEES: callee translation units the port reaches from the rows of this target (was: unresolved -> port fault at 0x400000, rest of the function not compared)
    target_sources(wave3_ui_oracle_test PRIVATE game_sa/GxtChar.cpp game_sa/HudColours.cpp game_sa/ControllerConfigManager.cpp)
    target_compile_definitions(wave3_ui_oracle_test PRIVATE W2_ONLY W3_UI_ONLY)
    target_compile_features(wave3_ui_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_ui_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_ui_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_ui_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_ui_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_ui_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_ui_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3ui_sources wave3_ui_oracle_test SOURCES)
    list(REMOVE_ITEM _w3ui_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_ui_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3ui_sources})
    target_link_options(wave3_ui_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_ui_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-VIS: exe-oracle rows of game_sa/VisibilityPlugins.cpp + app/app_light.cpp (A + B functions) with stub-recorded RW calls; rows: tests/standalone/wave3_oracle_vis.inc
    add_executable(wave3_vis_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/VisibilityPlugins.cpp
        game_sa/ModelIndices.cpp
        app/app_light.cpp
    )
    target_compile_definitions(wave3_vis_oracle_test PRIVATE W2_ONLY W3_VIS_ONLY)
    target_compile_features(wave3_vis_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_vis_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_vis_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_vis_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_vis_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_vis_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_vis_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3vis_sources wave3_vis_oracle_test SOURCES)
    list(REMOVE_ITEM _w3vis_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_vis_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3vis_sources})
    target_link_options(wave3_vis_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_vis_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-RENDER: exe-oracle rows of game_sa/Renderer.cpp + PostEffects.cpp + GrassRenderer.cpp + Plugins/TwoDEffectPlugin/2dEffect.cpp (A + B functions) with stub-recorded RW calls; rows: tests/standalone/wave3_oracle_render.inc
    add_executable(wave3_render_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/Renderer.cpp
        game_sa/PostEffects.cpp
        game_sa/GrassRenderer.cpp
        game_sa/Plugins/TwoDEffectPlugin/2dEffect.cpp
        game_sa/Clock.cpp
        game_sa/PPTriPlantBuffer.cpp
        game_sa/ModelIndices.cpp
        app/app_light.cpp
    )
    target_compile_definitions(wave3_render_oracle_test PRIVATE W2_ONLY W3_RENDER_ONLY)
    target_compile_features(wave3_render_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_render_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_render_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_render_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_render_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_render_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_render_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3render_sources wave3_render_oracle_test SOURCES)
    list(REMOVE_ITEM _w3render_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_render_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3render_sources})
    target_link_options(wave3_render_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/FORCE:MULTIPLE;/MAP>")
    set_target_properties(wave3_render_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-CARENV: exe-oracle rows of game_sa/Pipelines/CustomCarEnvMap (car paint env-map / specular plugin data, A + B functions) on fixture blocks; rows: tests/standalone/wave3_oracle_carenv.inc
    add_executable(wave3_carenv_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp
        game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline_EnvMat.cpp
        game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline_EnvAtm.cpp
        game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline_SpecMat.cpp
        game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.cpp
    )
    target_compile_definitions(wave3_carenv_oracle_test PRIVATE W2_ONLY W3_CARENV_ONLY)
    target_compile_features(wave3_carenv_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_carenv_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_carenv_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_carenv_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_carenv_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_carenv_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_carenv_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3carenv_sources wave3_carenv_oracle_test SOURCES)
    list(REMOVE_ITEM _w3carenv_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_carenv_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3carenv_sources})
    target_link_options(wave3_carenv_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_carenv_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-GP2: exe-oracle rows of game_sa/TrafficLights.cpp + Restart.cpp + Shopping.cpp + Attractors/PedAttractor.cpp (A + B functions); rows: tests/standalone/wave3_oracle_gp2.inc
    add_executable(wave3_gp2_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/TrafficLights.cpp
        game_sa/Restart.cpp
        game_sa/Shopping.cpp
        game_sa/Attractors/PedAttractor.cpp
        game_sa/ModelIndices.cpp
        game_sa/Core/Rect.cpp
    )
    target_compile_definitions(wave3_gp2_oracle_test PRIVATE W2_ONLY W3_GP2_ONLY)
    target_compile_features(wave3_gp2_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_gp2_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_gp2_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_gp2_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_gp2_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_gp2_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_gp2_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3gp2_sources wave3_gp2_oracle_test SOURCES)
    list(REMOVE_ITEM _w3gp2_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_gp2_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3gp2_sources})
    target_link_options(wave3_gp2_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_gp2_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-MISC: exe-oracle rows of game_sa/Localisation.cpp + game_sa/TagManager.cpp (A + B functions); rows: tests/standalone/wave3_oracle_misc.inc
    add_executable(wave3_misc_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/Localisation.cpp
        game_sa/TagManager.cpp
        app/platform/platform.cpp
        app/platform/win/WinPs.cpp
        game_sa/Pipelines/CustomBuilding/CustomBuildingRenderer.cpp
        game_sa/Core/Rect.cpp
        game_sa/VisibilityPlugins.cpp
        game_sa/ModelIndices.cpp
    )
    target_compile_definitions(wave3_misc_oracle_test PRIVATE W2_ONLY W3_MISC_ONLY)
    target_compile_features(wave3_misc_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_misc_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_misc_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_misc_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_misc_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_misc_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_misc_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3misc_sources wave3_misc_oracle_test SOURCES)
    list(REMOVE_ITEM _w3misc_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_misc_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3misc_sources})
    target_link_options(wave3_misc_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_misc_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-STREAM: exe-oracle rows of game_sa/IplStore.cpp + TxdStore.cpp + Streaming.cpp (A + B functions); rows: tests/standalone/wave3_oracle_stream.inc
    add_executable(wave3_stream_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/IplStore.cpp
        game_sa/TxdStore.cpp
        game_sa/TxdStore_Plugin.cpp
        game_sa/Core/Rect.cpp
        game_sa/Population.cpp
        game_sa/LoadedCarGroup.cpp
    )
    target_compile_definitions(wave3_stream_oracle_test PRIVATE W2_ONLY W3_STREAM_ONLY)
    target_compile_features(wave3_stream_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_stream_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_stream_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_stream_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_stream_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_stream_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_stream_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3stream_sources wave3_stream_oracle_test SOURCES)
    list(REMOVE_ITEM _w3stream_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_stream_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3stream_sources})
    target_link_options(wave3_stream_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_stream_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-REPLAY: exe-oracle rows of game_sa/Replay.cpp (A + B functions: replay buffer packets, pool conversion tables, player info); rows: tests/standalone/wave3_oracle_replay.inc
    add_executable(wave3_replay_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/Replay.cpp
        game_sa/ReplayBlockType.cpp
        game_sa/World_Helper.cpp
        game_sa/PlayerInfo.cpp
        game_sa/CompressedMatrixNotAligned.cpp
    )
    target_compile_definitions(wave3_replay_oracle_test PRIVATE W2_ONLY W3_REPLAY_ONLY)
    target_compile_features(wave3_replay_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_replay_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_replay_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_replay_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_replay_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_replay_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_replay_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3replay_sources wave3_replay_oracle_test SOURCES)
    list(REMOVE_ITEM _w3replay_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_replay_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3replay_sources})
    target_link_options(wave3_replay_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_replay_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-FX: exe-oracle rows of game_sa/Fx (FxSystem, FxManager, FxMemoryPool, FxSphere, FxFrustumInfo, FxSystemBP, FxEmitterBP) on fixture blocks; rows: tests/standalone/wave3_oracle_fxs.inc
    file(GLOB _W3X_BP_SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/game_sa/Fx/Blueprints/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/game_sa/Fx/Blueprints/Interp/*.cpp")
    add_executable(wave3_fxs_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${W2_BASE_SOURCES}
        game_sa/Fx/FxSystem.cpp
        game_sa/Fx/FxManager.cpp
        game_sa/Fx/FxMemoryPool.cpp
        game_sa/Fx/FxFrustumInfo.cpp
        game_sa/Fx/FxSystemBP.cpp
        game_sa/Fx/FxEmitterBP.cpp
        game_sa/Fx/FxPrimBP.cpp
        game_sa/Fx/FxPrim.cpp
        game_sa/Fx/FxEmitterPrt.cpp
        # ORACLE-W3-FXBP: the per-prim info blueprints + key curves, FxInfoManager (AddFxInfo), Fx.cpp (RotateVecAboutVec), CarFXRenderer, the sine LUT (Maths.cpp); rows: tests/standalone/wave3_oracle_fxbp.inc
        ${_W3X_BP_SOURCES}
        game_sa/Fx/FxInfoManager.cpp
        game_sa/Fx/Fx.cpp
        game_sa/Fx/CarFXRenderer.cpp
        game_sa/Maths.cpp
        game_sa/Plugins/PipelinePlugin/PipelinePlugin.cpp
        standalone/rw/math.cpp
        standalone/rw/rwmath_tables.cpp
    )
    target_compile_definitions(wave3_fxs_oracle_test PRIVATE W2_ONLY W3_FXS_ONLY)
    target_compile_features(wave3_fxs_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_fxs_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_fxs_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_fxs_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_fxs_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_fxs_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_fxs_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3x_sources wave3_fxs_oracle_test SOURCES)
    list(REMOVE_ITEM _w3x_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_fxs_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3x_sources})
    target_link_options(wave3_fxs_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_fxs_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-EFFECTS: exe-oracle rows of the world-effect root files (Explosion, Fire, FireManager, CreepingFire, Glass, Rope(s), Cranes, Bridge, Birds, Skidmarks, WaterCannon(s), Coronas, Clouds, Plant*, ProjectileInfo; A + B functions); rows: tests/standalone/wave3_oracle_fx*.inc
    set(_W3F_BASE ${W2_BASE_SOURCES})
    add_executable(wave3_fx_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${_W3F_BASE}
        game_sa/Explosion.cpp
        game_sa/Fire.cpp
        game_sa/FireManager.cpp
        game_sa/CreepingFire.cpp
        game_sa/Glass.cpp
        game_sa/FallingGlassPane.cpp
        game_sa/Rope.cpp
        game_sa/Ropes.cpp
        game_sa/Cranes.cpp
        game_sa/Bridge.cpp
        game_sa/Birds.cpp
        game_sa/Skidmarks.cpp
        game_sa/WaterCannon.cpp
        game_sa/WaterCannons.cpp
        game_sa/Coronas.cpp
        game_sa/Clouds.cpp
        game_sa/PlantMgr.cpp
        game_sa/PlantSurfPropMgr.cpp
        game_sa/PPTriPlantBuffer.cpp
        game_sa/ProjectileInfo.cpp
        game_sa/TimeCycle.cpp
        game_sa/SurfaceInfos_c.cpp
        game_sa/Collision/Box.cpp
    )
    target_compile_definitions(wave3_fx_oracle_test PRIVATE W2_ONLY W3_FX_ONLY)
    target_compile_features(wave3_fx_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_fx_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_fx_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_fx_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_fx_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_fx_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_fx_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3fx_sources wave3_fx_oracle_test SOURCES)
    list(REMOVE_ITEM _w3fx_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_fx_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3fx_sources})
    target_link_options(wave3_fx_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_fx_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-INPUT: exe-oracle rows of ControllerConfigManager.cpp + Pad.cpp (A + B functions); rows: tests/standalone/wave3_oracle_input*.inc
    set(_W3I_BASE ${W2_BASE_SOURCES})
    add_executable(wave3_input_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        "${PROJECT_SOURCE_DIR}/tests/standalone/oracle_fast.cpp"
        ${_W3I_BASE}
        game_sa/ControllerConfigManager.cpp
        game_sa/Pad.cpp
        game_sa/ControllerState.cpp
        game_sa/KeyboardState.cpp
        game_sa/MouseControllerState.cpp
        game_sa/GxtChar.cpp
        game_sa/MenuManager.cpp
    )
    # INPUT-HANG: the harness helpers that dominate a case (4 MB image compare, FillGarbage of the scratch block) are optimised (pragmas) in their own TU; the /Od target took 18 min (run_all_tests timeout 15)
    set_source_files_properties("${PROJECT_SOURCE_DIR}/tests/standalone/oracle_fast.cpp" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    target_compile_definitions(wave3_input_oracle_test PRIVATE ORACLE_FAST_HELPERS)
    target_compile_definitions(wave3_input_oracle_test PRIVATE W2_ONLY W3_INPUT_ONLY)
    target_compile_features(wave3_input_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_input_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_input_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_input_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_input_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_input_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_input_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3in_sources wave3_input_oracle_test SOURCES)
    list(REMOVE_ITEM _w3in_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_input_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3in_sources})
    target_link_options(wave3_input_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_input_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # ORACLE-W3-AI: exe-oracle rows of CarCtrl.cpp, PedIntelligence.cpp, Cover.cpp, Entity/Object/Object.cpp (A + B functions); rows: tests/standalone/wave3_oracle_ai*.inc
    set(_W3A_BASE ${W2_BASE_SOURCES})
    add_executable(wave3_ai_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/wave1_oracle_test.cpp"
        ${_W3A_BASE}
        game_sa/CarCtrl.cpp
        game_sa/PedIntelligence.cpp
        game_sa/Cover.cpp
        game_sa/CoverPoint.cpp
        game_sa/Entity/Object/Object.cpp
        game_sa/Entity/Physical.cpp
        game_sa/PedType.cpp
        game_sa/Acquaintance.cpp
        game_sa/Ropes.cpp
        game_sa/Rope.cpp
        game_sa/ModelIndices.cpp
        game_sa/Collision/Collision.cpp
        game_sa/LoadedCarGroup.cpp
        game_sa/Models/BaseModelInfo.cpp
        game_sa/TxdStore.cpp
        game_sa/Core/PtrNodeDoubleLink.cpp
        game_sa/Core/PtrNodeSingleLink.cpp
        game_sa/TimeCycle.cpp
        game_sa/Tasks/TaskManager.cpp
    )
    target_compile_definitions(wave3_ai_oracle_test PRIVATE W2_ONLY W3_AI_ONLY)
    target_compile_features(wave3_ai_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(wave3_ai_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)
    target_include_directories(wave3_ai_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(wave3_ai_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(wave3_ai_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(wave3_ai_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(wave3_ai_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_w3ai_sources wave3_ai_oracle_test SOURCES)
    list(REMOVE_ITEM _w3ai_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET wave3_ai_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_w3ai_sources})
    target_link_options(wave3_ai_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED;/MAP>")
    set_target_properties(wave3_ai_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # TRAP-AUDIT: the wave3 oracle tests (not wave2_oracle_test: its ControlSubTask rows die with STATUS_INVALID_EXCEPTION_HANDLER under this) see NOTSA_UNREACHABLE like the run build (Fixups::UnreachableHit stub in wave1_oracle_test.cpp) instead of the Debug MessageBox/exit(3)
    foreach (_t wave3_ai_oracle_test wave3_anim_oracle_test wave3_audio_oracle_test wave3_camera_oracle_test wave3_carenv_oracle_test wave3_coll_oracle_test wave3_entity_oracle_test wave3_events_oracle_test wave3_fx_oracle_test wave3_fxs_oracle_test wave3_gameplay_oracle_test wave3_gp2_oracle_test wave3_ik_oracle_test wave3_input_oracle_test wave3_interior_oracle_test wave3_misc_oracle_test wave3_models_oracle_test wave3_ped_oracle_test wave3_scripts_oracle_test wave3_stream_oracle_test wave3_tasks_oracle_test wave3_ui_oracle_test wave3_vehicle_oracle_test wave3_vis_oracle_test wave3_world_oracle_test)
        if (TARGET ${_t})
            target_compile_definitions(${_t} PRIVATE W2_UNREACHABLE_RUNBUILD)
        endif()
    endforeach()
    # x87 intrinsic fidelity oracle: inline fsin/fcos/fpatan/fptan/fsqrt/fyl2x of the exe vs the ports' math, and the CRT entry points the exe calls (pow, asin, acos, tan, floor, ceil, modf)
    add_executable(x87_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/x87_oracle_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Core/Quaternion.cpp
        game_sa/Core/Vector2D.cpp
        game_sa/Cover.cpp
        game_sa/Curves.cpp
        game_sa/cHandlingDataMgr.cpp
        game_sa/Cam.cpp
        game_sa/Camera.cpp
        game_sa/Radar.cpp
        game_sa/MenuManager.cpp
        game_sa/Checkpoint.cpp
        game_sa/Checkpoints.cpp
        game_sa/Entity/Placeable.cpp
        game_sa/Maths.cpp
        game_sa/IdleCam.cpp
        game_sa/CoverPoint.cpp
        game_sa/PathFind.cpp
        game_sa/Ragdoll/BoneNode.cpp
        game_sa/Entity/Ped/Ped.cpp
        game_sa/PostEffects.cpp
        game_sa/TimeCycle.cpp
        game_sa/Hud.cpp
        game_sa/Attractors/PedShelterAttractor.cpp
        game_sa/Font.cpp
        standalone/FtolExe.cpp
    )
    target_compile_features(x87_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(x87_oracle_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(x87_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES> "${PROJECT_SOURCE_DIR}/tests/standalone")
    target_compile_options(x87_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(x87_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(x87_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(x87_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_x87_sources x87_oracle_test SOURCES)
    list(REMOVE_ITEM _x87_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET x87_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_x87_sources})
    target_link_options(x87_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(x87_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # CPool fidelity oracle: the exe's CPool<CPlayerPed> instance (New/Delete/IsObjectValid machine code) vs Core/Pool.h under NOTSA_STANDALONE_RUN, random op sequences
    add_executable(pool_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/pool_oracle_test.cpp"
    )
    target_compile_features(pool_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(pool_oracle_test PRIVATE NOTSA_RW_LIBRW RW_D3D9 NOTSA_STANDALONE_RUN)
    target_include_directories(pool_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(pool_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(pool_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(pool_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(pool_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_po_sources pool_oracle_test SOURCES)
    list(REMOVE_ITEM _po_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET pool_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_po_sources})
    target_link_options(pool_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(pool_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # Audio fade oracle: CAESmoothFadeThread::Service (0x4EED10) machine code vs the port on random entry tables with fake IDirectSoundBuffer objects
    add_executable(audio_fade_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/audio_fade_oracle_test.cpp"
        game_sa/Audio/AESmoothFadeThread.cpp
    )
    target_compile_features(audio_fade_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(audio_fade_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)   # the run build's own definitions (USE_DSOUND, NOTSA_STANDALONE_RUN ...): the audio headers need them
    target_include_directories(audio_fade_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(audio_fade_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(audio_fade_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(audio_fade_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(audio_fade_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_af_sources audio_fade_oracle_test SOURCES)
    list(REMOVE_ITEM _af_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET audio_fade_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_af_sources})
    target_link_options(audio_fade_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(audio_fade_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # P2E-3 gameplay differential test: peds / IK / tasks, camera, weapons, vehicles against the exe machine code (same machinery as physics_oracle_test)
    add_executable(gameplay_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/gameplay_oracle_test.cpp"
        game_sa/Collision/Collision.cpp
        game_sa/Collision/CollisionPrimitives.cpp
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Vector2D.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Core/Quaternion.cpp
        game_sa/Entity/Placeable.cpp
        game_sa/Timer.cpp
        standalone/FtolExe.cpp
        # --- peds
        game_sa/PedGeometryAnalyser.cpp
        game_sa/PedIK.cpp
        game_sa/Ragdoll/IKChain.cpp
        game_sa/Ragdoll/BoneNode.cpp
        game_sa/Entity/Ped/Ped.cpp
        game_sa/Collision/ColSphere.cpp
        game_sa/SurfaceInfos_c.cpp
        game_sa/PedIntelligence.cpp
        game_sa/Entity/Entity.cpp
        game_sa/Entity/Physical.cpp
        game_sa/Buoyancy.cpp
        game_sa/Radar.cpp
        game_sa/Pad.cpp
        game_sa/VehicleRecording.cpp
        game_sa/Collision/BoundingBox.cpp
        game_sa/Garage.cpp
        game_sa/CompressedMatrixNotAligned.cpp
        game_sa/FireManager.cpp
        game_sa/Fx/Blueprints/Interp/FxInterpInfoFloat.cpp
        game_sa/Fx/Blueprints/Interp/FxInterpInfo32.cpp
        game_sa/Fx/Blueprints/Interp/FxInterpInfo255.cpp
        game_sa/Fx/Blueprints/Interp/FxInterpInfoU255.cpp
        game_sa/Fx/Blueprints/Interp/FxInterpInfo.cpp
        # --- camera
        game_sa/Camera.cpp
        game_sa/Cam.cpp
        game_sa/IdleCam.cpp            # CCamera::Init test: CCam::Init calls gIdleCam.Reset(true)
        # --- weapons
        game_sa/Weapon.cpp
        game_sa/WeaponInfo.cpp
        game_sa/Explosion.cpp
        game_sa/ShotInfo.cpp
        game_sa/BulletInfo.cpp
        game_sa/ProjectileInfo.cpp
        game_sa/Collision/BoundingBox.cpp
        game_sa/World_Helper.cpp
        # --- vehicles
        game_sa/DamageManager.cpp
        game_sa/cTransmission.cpp
        game_sa/Entity/Vehicle/Vehicle.cpp
        game_sa/Entity/Vehicle/Automobile.cpp
        game_sa/Door.cpp
        game_sa/CarEnterExit.cpp
        game_sa/cHandlingDataMgr.cpp
        game_sa/Entity/Vehicle/Plane.cpp
        game_sa/Entity/Vehicle/Heli.cpp
        game_sa/Entity/Vehicle/Boat.cpp
        game_sa/Entity/Vehicle/Bike.cpp
        game_sa/Entity/Vehicle/Bmx.cpp
        game_sa/Entity/Vehicle/MonsterTruck.cpp
        game_sa/Entity/Vehicle/QuadBike.cpp
        game_sa/Entity/Vehicle/Trailer.cpp
        game_sa/Entity/Vehicle/Train.cpp
        game_sa/Entity/Physical.cpp
    )
    target_compile_features(gameplay_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(gameplay_oracle_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(gameplay_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(gameplay_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(gameplay_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(gameplay_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(gameplay_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_gp_sources gameplay_oracle_test SOURCES)
    list(REMOVE_ITEM _gp_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET gameplay_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_gp_sources})
    target_link_options(gameplay_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(gameplay_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # Review oracle (3e73083e hooks): CHud / CMaths / CTaskTimer / CTaskSequences / CIdleCam / CCoverPoint / CConversations / CCredits / CVehicleSaveStructure /
    # CPathFind::FindNodePairClosestToCoors against the exe machine code (same machinery as game_oracle_test)
    add_executable(review_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/review_oracle_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Hud.cpp
        game_sa/IdleCam.cpp
        game_sa/Maths.cpp
        game_sa/CoverPoint.cpp
        game_sa/Conversations/Conversations.cpp
        game_sa/Credits.cpp
        game_sa/Tasks/TaskTimer.cpp
        game_sa/Tasks/TaskSequences.cpp
        game_sa/VehicleSaveStructure.cpp
        game_sa/PathFind.cpp
        # common.h fidelity cases (TestFidelity): SCREEN_* / PI family users
        game_sa/Ragdoll/BoneNode.cpp
        game_sa/Entity/Ped/Ped.cpp
        game_sa/Cam.cpp
        game_sa/Camera.cpp
        game_sa/Radar.cpp
        game_sa/MenuManager.cpp
        game_sa/PostEffects.cpp
        game_sa/Attractors/PedShelterAttractor.cpp
        # reciprocal-constant fidelity cases (TestRecip)
        game_sa/Collision/Collision.cpp
        game_sa/Collision/CollisionPrimitives.cpp
        game_sa/Curves.cpp
        game_sa/TrainNode.cpp
        game_sa/GridRef.cpp
        game_sa/Audio/AEAudioUtility.cpp
        game_sa/Font.cpp
        game_sa/TimeCycle.cpp
        game_sa/Entity/Vehicle/Train.cpp
        standalone/FtolExe.cpp   # _ftol2_sse with the exe's semantics (TestFtol)
    )
    target_compile_features(review_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(review_oracle_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(review_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(review_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(review_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(review_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(review_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_ro_sources review_oracle_test SOURCES)
    list(REMOVE_ITEM _ro_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET review_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_ro_sources})
    target_link_options(review_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(review_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # FixedFloat/FixedVector/FixedQuat decompress (fmul by the exe's float reciprocal vs fdiv) and compress (_ftol2) per site against the exe machine code (same machinery as review_oracle_test)
    add_executable(fixed_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/fixed_oracle_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Core/Quaternion.cpp
        game_sa/Core/Vector2D.cpp
        game_sa/Maths.cpp
        game_sa/CoverPoint.cpp
        game_sa/Cover.cpp
        game_sa/Occlusion.cpp
        game_sa/Occluder.cpp
        game_sa/ActiveOccluder.cpp
        game_sa/Animation/AnimBlendSequence.cpp
        game_sa/CompressedMatrixNotAligned.cpp
        game_sa/Fx/FxPrimBP.cpp
        game_sa/Audio/Entities/AEVehicleAudioEntity.cpp
        game_sa/common.cpp
        game_sa/ModelIndices.cpp
        game_sa/ActiveOccluder.cpp
        standalone/FtolExe.cpp
    )
    target_compile_features(fixed_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(fixed_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_DEFINITIONS> NOTSA_ORACLE_ADDRESS_GLOBALS)   # the run build's own definitions (USE_DSOUND ...): the audio headers need them
    target_compile_definitions(fixed_oracle_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(fixed_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(fixed_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(fixed_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(fixed_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(fixed_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_fo_sources fixed_oracle_test SOURCES)
    list(REMOVE_ITEM _fo_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET fixed_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_fo_sources})
    target_link_options(fixed_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(fixed_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-B differential test of script command handlers (g9..g12) against the exe's group processors (same exe-oracle machinery as game_oracle_test; the handler TUs are #included by the test)
    add_executable(script_oracle_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
    )
    target_compile_features(script_oracle_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_so_sources script_oracle_test SOURCES)
    list(REMOVE_ITEM _so_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_so_sources})
    target_link_options(script_oracle_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-D differential test of script command handlers (g16) against the exe's group processor (same machinery as script_oracle_test; Group16_17a.cpp is #included by the test)
    add_executable(script_oracle_g16_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_g16_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
    )
    target_compile_features(script_oracle_g16_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_g16_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_g16_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_g16_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_g16_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_g16_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_g16_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_so16_sources script_oracle_g16_test SOURCES)
    list(REMOVE_ITEM _so16_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_g16_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_so16_sources})
    target_link_options(script_oracle_g16_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_g16_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-C differential test of script command handlers (g13..g15) against the exe's group processors (same machinery as script_oracle_test; the handler TU is #included by the test)
    add_executable(script_oracle_g13_15_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_g13_15_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
    )
    target_compile_features(script_oracle_g13_15_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_g13_15_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_g13_15_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_g13_15_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_g13_15_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_g13_15_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_g13_15_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_s6c_sources script_oracle_g13_15_test SOURCES)
    list(REMOVE_ITEM _s6c_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_g13_15_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_s6c_sources})
    target_link_options(script_oracle_g13_15_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_g13_15_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-I differential test of script command handlers (g24) against the exe's group processor (same machinery as script_oracle_test; Group24a/24b.cpp are #included by the test)
    add_executable(script_oracle_g24_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_g24_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
    )
    target_compile_features(script_oracle_g24_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_g24_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_g24_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_g24_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_g24_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_g24_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_g24_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_s6i_sources script_oracle_g24_test SOURCES)
    list(REMOVE_ITEM _s6i_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_g24_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_s6i_sources})
    target_link_options(script_oracle_g24_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_g24_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-J differential test of script command handlers (g25 + g26) against the exe's group processors (same exe-oracle machinery as script_oracle_g24_test; Group25_26a/b/c.cpp are #included by the test)
    add_executable(script_oracle_g25_26_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_g25_26_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
    )
    target_compile_features(script_oracle_g25_26_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_g25_26_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_g25_26_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_g25_26_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_g25_26_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_g25_26_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_g25_26_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_s6j_sources script_oracle_g25_26_test SOURCES)
    list(REMOVE_ITEM _s6j_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_g25_26_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_s6j_sources})
    target_link_options(script_oracle_g25_26_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_g25_26_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-D differential test of script command handlers (g17) against the exe's group processors (same exe-oracle machinery as script_oracle_g25_26_test; the Group16_17b.cpp handler TU(s) are #included by the test)
    add_executable(script_oracle_g17_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_g17_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
    )
    target_compile_features(script_oracle_g17_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_g17_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_g17_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_g17_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_g17_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_g17_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_g17_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_sog17_sources script_oracle_g17_test SOURCES)
    list(REMOVE_ITEM _sog17_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_g17_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_sog17_sources})
    target_link_options(script_oracle_g17_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_g17_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-E differential test of script command handlers (g18) against the exe's group processors (same exe-oracle machinery as script_oracle_g25_26_test; the Group18.cpp handler TU(s) are #included by the test)
    add_executable(script_oracle_g18_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_g18_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
    )
    target_compile_features(script_oracle_g18_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_g18_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_g18_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_g18_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_g18_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_g18_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_g18_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_sog18_sources script_oracle_g18_test SOURCES)
    list(REMOVE_ITEM _sog18_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_g18_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_sog18_sources})
    target_link_options(script_oracle_g18_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_g18_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-E differential test of script command handlers (g19) against the exe's group processors (same exe-oracle machinery as script_oracle_g25_26_test; the Group19.cpp handler TU(s) are #included by the test)
    add_executable(script_oracle_g19_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_g19_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
    )
    target_compile_features(script_oracle_g19_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_g19_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_g19_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_g19_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_g19_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_g19_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_g19_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_sog19_sources script_oracle_g19_test SOURCES)
    list(REMOVE_ITEM _sog19_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_g19_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_sog19_sources})
    target_link_options(script_oracle_g19_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_g19_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-F differential test of script command handlers (g20..g21) against the exe's group processors (same machinery as script_oracle_test; Group20a/20b/21a/21b.cpp are #included by the test)
    add_executable(script_oracle_g20_21_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_g20_21_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
    )
    target_compile_features(script_oracle_g20_21_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_g20_21_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_g20_21_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_g20_21_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_g20_21_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_g20_21_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_g20_21_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_s6f_sources script_oracle_g20_21_test SOURCES)
    list(REMOVE_ITEM _s6f_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_g20_21_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_s6f_sources})
    target_link_options(script_oracle_g20_21_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_g20_21_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-G differential test of script command handlers (g22) against the exe's group processor (same machinery as script_oracle_test; Group22a/22b.cpp are #included by the test, Shopping.cpp is linked for the price modifier ports)
    add_executable(script_oracle_g22_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_g22_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
        game_sa/Shopping.cpp
    )
    target_compile_features(script_oracle_g22_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_g22_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_g22_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_g22_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_g22_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_g22_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_g22_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_s6g_sources script_oracle_g22_test SOURCES)
    list(REMOVE_ITEM _s6g_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_g22_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_s6g_sources})
    target_link_options(script_oracle_g22_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_g22_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # S6-H differential test of script command handlers (g23) against the exe's group processor (same machinery as script_oracle_test; Group23.cpp is #included by the test)
    add_executable(script_oracle_g23_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/script_oracle_g23_test.cpp"
        game_sa/General.cpp
        game_sa/Core/Vector.cpp
        game_sa/Core/Matrix.cpp
    )
    target_compile_features(script_oracle_g23_test PRIVATE cxx_std_23)
    target_compile_definitions(script_oracle_g23_test PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    target_include_directories(script_oracle_g23_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},INCLUDE_DIRECTORIES>)
    target_compile_options(script_oracle_g23_test PRIVATE $<TARGET_PROPERTY:${RE_PROJECT_LIB_NAME},COMPILE_OPTIONS>)
    target_precompile_headers(script_oracle_g23_test PRIVATE StdInc.cpp)
    target_link_libraries(script_oracle_g23_test PRIVATE librw spdlog::spdlog Tracy::TracyClient user32)
    target_sources(script_oracle_g23_test PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_s6h_sources script_oracle_g23_test SOURCES)
    list(REMOVE_ITEM _s6h_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET script_oracle_g23_test PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_s6h_sources})
    target_link_options(script_oracle_g23_test PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO;/SUBSYSTEM:CONSOLE;/FORCE:UNRESOLVED>")
    set_target_properties(script_oracle_g23_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF CXX_SCAN_FOR_MODULES OFF)
    # P2B-06 unit test of Im2D / Im3D (not part of `all`): ninja rw_im2d_im3d_test. Hidden window + librw's D3D9 device (Wine/wined3d is fine); pixels are read back
    if (EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/rw_im2d_im3d_test.cpp")
        add_executable(rw_im2d_im3d_test EXCLUDE_FROM_ALL
            "${PROJECT_SOURCE_DIR}/tests/standalone/rw_im2d_im3d_test.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/im2d_im3d.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/renderstate.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/rwd3d_ff.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/frame.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/camera.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/math.cpp"
        )
        target_compile_features(rw_im2d_im3d_test PRIVATE cxx_std_23)
        target_compile_definitions(rw_im2d_im3d_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
        target_include_directories(rw_im2d_im3d_test BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fakerw")
        target_link_libraries(rw_im2d_im3d_test PRIVATE librw d3d9 user32)
        set_target_properties(rw_im2d_im3d_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()

    # P2B-05b unit test of the matfx / uvanim layer (Wine-runnable; optional SA DFF paths on the command line)
    add_executable(rw_matfx_uvanim_test EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/standalone/rw_matfx_uvanim_test.cpp"
        standalone/rw/matfx.cpp
        standalone/rw/uvanim.cpp
        standalone/rw/rwglobals.cpp
        standalone/rw/atomic.cpp
        standalone/rw/clump.cpp
        standalone/rw/plugins.cpp
        standalone/rw/frame.cpp
        standalone/rw/light.cpp
        standalone/rw/world.cpp
        standalone/rw/material.cpp
        standalone/rw/geometry.cpp
        standalone/rw/geometry_strip.cpp
        standalone/rw/texture.cpp
        standalone/rw/texdict.cpp
        standalone/rw/raster.cpp
        standalone/rw/image.cpp
        standalone/rw/camera.cpp
    )
    target_compile_features(rw_matfx_uvanim_test PRIVATE cxx_std_23)
    target_compile_definitions(rw_matfx_uvanim_test PRIVATE NOTSA_RW_LIBRW NOTSA_RW_UNIT_TEST RW_D3D9)
    target_include_directories(rw_matfx_uvanim_test BEFORE PRIVATE fakerw)
    target_link_libraries(rw_matfx_uvanim_test PRIVATE librw d3d9 user32)
    set_target_properties(rw_matfx_uvanim_test PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    # engine.cpp / camera.cpp log through standalone/Fixups.h: give the shim unit tests the include path (AFTER, so it cannot shadow fakerw) and a Log() stub
    foreach (_t rw_custom_pipelines_test rw_engine_test rw_frame_camera_test rw_raster_image_test rw_texture_test rw_platform_test rw_pipeline_test rw_matfx_pipeline_test rw_matfx_uvanim_test rw_im2d_im3d_test)
        if (TARGET ${_t})
            target_include_directories(${_t} AFTER PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            target_sources(${_t} PRIVATE "${PROJECT_SOURCE_DIR}/tests/standalone/fixups_log_stub.cpp")
        endif()
    endforeach()
    endif() # NOT PORTABLE
endif()

#--------------------------------------------------------------------
# Add dependencies
#--------------------------------------------------------------------

# Adding this here as it has no Conan-center package, and it's header-only
FetchContent_Declare(
    imfilebrowser
    GIT_REPOSITORY https://github.com/AirGuanZ/imgui-filebrowser
    GIT_TAG        47a1884
)
FetchContent_MakeAvailable(imfilebrowser)

macro(add_dependency)
    foreach (target ${ARGV})
        find_package(${target} CONFIG REQUIRED)

        # Disable warnings of dependencies
        add_compile_options(${target} PRIVATE
            "$<$<CXX_COMPILER_ID:MSVC>:-w>"
            "$<$<OR:$<CXX_COMPILER_ID:Clang,GCC>,$<CXX_COMPILER_ID:GNU>>:-Wno-everything>"
        )
    endforeach()
endmacro()
add_dependency(
    nlohmann_json
    Ogg
    spdlog
    Tracy
    Vorbis
    imgui
    libjpeg-turbo
    Boost
)
if (${RE_PROJECT}_USE_SDL3)
    add_dependency(SDL3)
endif()

if (${RE_PROJECT}_RW_LIBRW)
    target_link_libraries(${RE_PROJECT_LIB_NAME} PRIVATE librw)
    if (${RE_PROJECT}_PORTABLE)
        target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE NOTSA_RW_LIBRW) # RW_GL3 comes from the librw target (PUBLIC)
    else()
        target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE NOTSA_RW_LIBRW RW_D3D9)
    endif()
    # fakerw headers (same file names as the RenderWare headers) take precedence over game_sa/RenderWare/rw/
    target_include_directories(${RE_PROJECT_LIB_NAME} BEFORE PRIVATE fakerw)
    # P2B-02a: engine.cpp is the shim's RwEngine*/RwD3D9 device layer and includes librw's private d3d9 header (rwd3dimpl.h)
    set_source_files_properties(standalone/rw/engine.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    # P2B-02c: standalone shim TUs that include fakerw.h directly (no PCH dependency, their own static helpers)
    set_source_files_properties(standalone/rw/renderstate.cpp standalone/rw/rwd3d_ff.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    # P2B-03a: raster.cpp / image.cpp include the same private d3d9 header
    set_source_files_properties(standalone/rw/raster.cpp standalone/rw/image.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

    # P2B-04ab: anonymous-namespace helpers (geometry.cpp mesh builder)
    set_source_files_properties(standalone/rw/geometry.cpp standalone/rw/geometry_strip.cpp standalone/rw/light.cpp standalone/rw/world.cpp standalone/rw/material.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    # P2B-03b: texture.cpp includes the private d3d9 header; texdict.cpp pairs with it
    set_source_files_properties(standalone/rw/texture.cpp standalone/rw/texdict.cpp standalone/rw/rwtexdict.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    # P2B-05a: anonymous-namespace helpers in skin.cpp / hanim.cpp
    set_source_files_properties(standalone/rw/skin.cpp standalone/rw/hanim.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    # P2B-06: Im2D/Im3D over the D3D9 device, same standalone style
    set_source_files_properties(standalone/rw/im2d_im3d.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    # P2B-07: pipeline.cpp includes the private d3d9 header and has anonymous-namespace helpers
    set_source_files_properties(standalone/rw/pipeline.cpp standalone/rw/pipeline_matfx.cpp standalone/rw/pipeline_skin.cpp standalone/rw/pipeline_skin_cpu.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    # P2B-05b: anonymous-namespace helpers in matfx.cpp / uvanim.cpp
    set_source_files_properties(standalone/rw/matfx.cpp standalone/rw/uvanim.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
endif()

if (${RE_PROJECT}_PORTABLE)
    target_link_libraries(${RE_PROJECT_LIB_NAME} PRIVATE
        nlohmann_json::nlohmann_json
        Ogg::ogg
        spdlog::spdlog
        Tracy::TracyClient
        vorbis::vorbis
        imgui::imgui
        libjpeg-turbo::libjpeg-turbo
        Boost::headers
    )
    if (WIN32)
        # MAC-WIN64: import libraries of the Win32 services that stay native on Windows (timers / MF decoders / DirectInput-free input path is SDL3)
        target_link_libraries(${RE_PROJECT_LIB_NAME} PRIVATE Winmm dxguid ole32 oleaut32 uuid shlwapi version)
    endif()
else()
target_link_libraries(${RE_PROJECT_LIB_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    Ogg::ogg
    spdlog::spdlog
    Tracy::TracyClient
    vorbis::vorbis
    imgui::imgui
    libjpeg-turbo::libjpeg-turbo
    ddraw
    Winmm
    dxguid
    strmiids
    d3d9
    Boost::headers
)
endif()
if (${RE_PROJECT}_USE_SDL3)
    target_link_libraries(${RE_PROJECT_LIB_NAME} PRIVATE
        SDL3::SDL3
    )
endif()

target_include_directories(${RE_PROJECT_LIB_NAME} PRIVATE
    .
    /
    toolsmenu/
    game_sa/
    game_sa/Debug/
    game_sa/Animation/
    game_sa/Attractors/

    game_sa/Audio/
    game_sa/Audio/Config
    game_sa/Audio/Entities
    game_sa/Audio/Hardware
    game_sa/Audio/Loaders
    game_sa/Audio/Managers
    game_sa/Audio/Utility

    game_sa/Collision/
    game_sa/Conversations/
    game_sa/Core/
    game_sa/Entity/
    game_sa/Entity/Dummy/
    game_sa/Entity/Object/
    game_sa/Entity/Ped/
    game_sa/Entity/Vehicle/
    game_sa/Enums/
    game_sa/Events/
    game_sa/Frontend/
    game_sa/Fx/
    game_sa/Fx/Blueprints/
    game_sa/Fx/Blueprints/Interp
    game_sa/Interior/
    game_sa/Memory/
    game_sa/Models/
    game_sa/Plant/
    game_sa/Plugins/
    game_sa/Pipelines/
    game_sa/Pipelines/CustomBuilding
    game_sa/Pipelines/CustomCarEnvMap
    game_sa/RenderWare/
    game_sa/RenderWare/rw/
    game_sa/Scripts/
    game_sa/Tasks/
    game_sa/Tasks/TaskTypes/
    game_sa/Tasks/TaskTypes/SeekEntity/

    app/
    app/imgui
    app/platform/
    app/platform/android/
    app/platform/win/
    app/platform/win/VideoPlayer/
    app/platform/linux/
    app/securom/

    oswrapper

    libs/imgui
    ${imfilebrowser_SOURCE_DIR}
)

target_sources(${RE_PROJECT_LIB_NAME} PRIVATE
    notsa.natvis
)

target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    USE_GTASA_ALLOCATOR
    EXTRA_DEBUG_FEATURES
    NOTSA_WINDOWED_MODE # NOTE: This is a workaround for the game to run in windowed mode, and it must be enabled, otherwise shit's gonna crash
    SA_SKINNED_PEDS
)
if (NOT ${RE_PROJECT}_PORTABLE)
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE USE_D3D9) # TODO (portable: librw GL3, no D3D9 code path)
elseif (NOT WIN32)
    # The ~10 files that #include <Windows.h> directly get the shim (source/portable/winshim/Windows.h -> wintypes.h + crt.h). On Windows (PortableWin64) the real SDK headers are used.
    target_include_directories(${RE_PROJECT_LIB_NAME} PRIVATE portable/winshim)
endif()
# D8 (.notes/PHASE2_STATUS.md "D8 defaults"): the runnable standalone build reproduces the ORIGINAL behaviour, so no FIX_BUGS (EXTRA_DEBUG_FEATURES only adds the occlusion debug-module header; the debug UI itself is disabled in source)
if (NOT ${RE_PROJECT}_STANDALONE_RUN_BUILD)
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE FIX_BUGS)
endif()
target_compile_features(${RE_PROJECT_LIB_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${RE_PROJECT_LIB_NAME} PRIVATE StdInc.cpp)

#--------------------------------------------------------------------
# Configure build options
#--------------------------------------------------------------------
if (${RE_PROJECT}_UNITY_BUILD)
    message(STATUS "Configuring Unity/Jumbo build")
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE NOTSA_UNITY_BUILD)
    set_target_properties(${RE_PROJECT_LIB_NAME}
        PROPERTIES
        UNITY_BUILD TRUE
        UNITY_BUILD_MODE BATCH
        UNITY_BUILD_BATCH_SIZE 16
    )
endif()

if (${RE_PROJECT}_WITH_LTO)
    message(STATUS "Configuring Link Time Optimization")
    set_target_properties(${RE_PROJECT_LIB_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if (${RE_PROJECT}_WITH_SANITIZERS)
    message(STATUS "Configuring sanitizers")
   #add_sanitizers(${RE_PROJECT_LIB_NAME}) # TODO
    message(FATAL_ERROR "${RE_PROJECT}_WITH_SANITIZERS is not implemented yet; keep it OFF for now (add_sanitizers() is currently missing).")
endif()

#--------------------------------------------------------------------
# Configure the Audio API
#--------------------------------------------------------------------
if (${RE_PROJECT}_WITH_OPENAL)
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE USE_OPENAL)
    if (${RE_PROJECT}_PORTABLE)
        # openal-soft (Homebrew: `brew install openal-soft`, keg-only; installs OpenALConfig.cmake, headers <AL/al.h>). The macOS OpenAL.framework (<OpenAL/al.h>) is
        # deliberately not used: it is deprecated and lacks the ALC/EFX extensions the EAX replacement needs. A conan route (openal-soft) works the same way via CMAKE_PREFIX_PATH.
        find_package(OpenAL CONFIG REQUIRED HINTS /opt/homebrew/opt/openal-soft /usr/local/opt/openal-soft)
        target_link_libraries(${RE_PROJECT_LIB_NAME} PRIVATE OpenAL::OpenAL)
        # The hardware layer (CAEAudioChannel & co) is still DirectSound code: USE_DSOUND selects it, its types come from source/portable/winshim/dsound.h
        # (abstract interfaces; DirectSoundCreate8 logs and fails, so audio is disabled at run time) until the OpenAL hardware layer replaces it (stream W-3/W-4).
        target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE USE_DSOUND)
    endif()

    target_include_directories(${RE_PROJECT_LIB_NAME} PRIVATE
        game_sa/Audio/OpenAL
    )
else()
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE USE_DSOUND)

    target_link_libraries(${RE_PROJECT_LIB_NAME} PRIVATE dsound)
endif()


#--------------------------------------------------------------------
# Case-sensitive file systems (Linux): MAC-LNX1
#--------------------------------------------------------------------
# The sources were written on Windows: a handful of #includes differ from the file name in letter case (Audio/entities/ vs Entities/, Common.h vs common.h, ...).
# tools/portable/gen_case_shim.py scans the sources at configure time and links each such include (as written) to the real file in ${CMAKE_BINARY_DIR}/case_shim, which is
# the LAST include directory (correctly spelled includes never reach it). No source file is touched.
if (${RE_PROJECT}_PORTABLE AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    get_target_property(_case_inc_dirs ${RE_PROJECT_LIB_NAME} INCLUDE_DIRECTORIES)
    set(_case_abs_dirs)
    foreach (_d IN LISTS _case_inc_dirs)
        cmake_path(ABSOLUTE_PATH _d BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE _dabs)
        list(APPEND _case_abs_dirs "${_dabs}")
    endforeach()
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/portable/gen_case_shim.py" "${PROJECT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/case_shim" ${_case_abs_dirs}
        RESULT_VARIABLE _case_rc OUTPUT_VARIABLE _case_out ERROR_VARIABLE _case_err)
    if (NOT _case_rc EQUAL 0)
        message(FATAL_ERROR "gen_case_shim.py failed: ${_case_err}")
    endif()
    string(REGEX MATCH "case shim: [0-9]+ includes" _case_summary "${_case_out}")
    message(STATUS "${_case_summary} (${CMAKE_BINARY_DIR}/case_shim)")
    target_include_directories(${RE_PROJECT_LIB_NAME} PRIVATE "${CMAKE_BINARY_DIR}/case_shim")
endif()


#--------------------------------------------------------------------
# Configure misc. dependencies for Windows
#--------------------------------------------------------------------
if(WIN32)
    if(MSVC)
        # MSVC knows how to find dbghelp on it's own
        target_link_libraries(${RE_PROJECT_LIB_NAME} PRIVATE
            dbghelp
        )

        target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE DIRECTINPUT_VERSION=0x0800)
    else()
        message(FATAL_ERROR "Need to link dbghelp lib for win32")
    endif()
endif()


#--------------------------------------------------------------------
# Install
#--------------------------------------------------------------------
if (${RE_PROJECT}_INSTALL)
    install(
        TARGETS ${RE_PROJECT_LIB_NAME}
        EXPORT ${RE_PROJECT_LIB_NAME}-targets
        RUNTIME DESTINATION "."
    )
    if (MSVC)
        install(FILES $<TARGET_PDB_FILE:${RE_PROJECT_LIB_NAME}> DESTINATION "." OPTIONAL)
    endif()
endif()

# Portable build only (P-2k): host tests of the portable layer as ctest tests, label "portable" (fast) / "mutation" (slow). enable_testing() is in the top-level CMakeLists.txt.
if (${RE_PROJECT}_PORTABLE)
    include("${PROJECT_SOURCE_DIR}/tools/portable/portable_tests.cmake")
endif()
