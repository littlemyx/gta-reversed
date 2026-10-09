cmake_minimum_required(VERSION 4.2.0 FATAL_ERROR)

include(FetchContent)

#--------------------------------------------------------------------
# Game features
#--------------------------------------------------------------------
option(${RE_PROJECT}_WITH_OPENAL "Use OpenAL" OFF)
option(${RE_PROJECT}_USE_SDL3 "Use SDL3" OFF)
option(${RE_PROJECT}_STANDALONE "Build standalone executable" OFF)
option(${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY "Dump hooks only" OFF)

if (${RE_PROJECT}_USE_SDL3)
    add_compile_definitions(NOTSA_USE_SDL3)
endif()
if (${RE_PROJECT}_STANDALONE)
    add_compile_definitions(NOTSA_STANDALONE)
endif()
if (${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY)
    if (NOT ${RE_PROJECT}_STANDALONE)
        message(FATAL_ERROR "${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY requires ${RE_PROJECT}_STANDALONE=ON")
    endif()
    add_compile_definitions(NOTSA_STANDALONE_DUMP_HOOKS_ONLY)
endif()
# Standalone exe that is meant to RUN (not just dump the hooks table): needs the data image of the user's own original exe
if (${RE_PROJECT}_STANDALONE AND NOT ${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY)
    add_compile_definitions(NOTSA_STANDALONE_RUN)
    set(${RE_PROJECT}_STANDALONE_RUN_BUILD ON)
    # D7 (.notes/PHASE2_PLAN.md): the original is x87 code (no SSE) running at PC=24 (D3D9 CreateDevice without FPU_PRESERVE). Everything compiled
    # in this directory (game code, shim, librw, shim unit tests) uses x87 + strict FP: no SSE/SSE2 codegen, no /fp:fast, no FMA contraction.
    if (MSVC)
        add_compile_options(/arch:IA32 /fp:precise)
    endif()
    set(${RE_PROJECT}_ORIGINAL_EXE "" CACHE FILEPATH "Original gta_sa.exe (1.0 US), the standalone build extracts the data image from it at build time")
    if (NOT ${RE_PROJECT}_ORIGINAL_EXE)
        message(WARNING "${RE_PROJECT}_STANDALONE without -D${RE_PROJECT}_ORIGINAL_EXE=<gta_sa.exe>: the exe will not find original_data.bin and refuse to start")
    elseif (NOT EXISTS "${${RE_PROJECT}_ORIGINAL_EXE}")
        message(FATAL_ERROR "${RE_PROJECT}_ORIGINAL_EXE does not exist: ${${RE_PROJECT}_ORIGINAL_EXE}")
    endif()
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
target_link_options(${RE_PROJECT_LIB_NAME} PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/INCREMENTAL>"
)
target_compile_options(${RE_PROJECT_LIB_NAME} PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/MP;/Zc:preprocessor;/W3;/wd4200;/wd4251;/wd4275;/wd4996;/wd26495;/wd26812;/wd28159>"
    "$<$<OR:$<CXX_COMPILER_ID:Clang,GCC>,$<CXX_COMPILER_ID:GNU>>:-Wall;-Wno-switch>"
)

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
target_link_options(${RE_PROJECT_LIB_NAME} PRIVATE "/SUBSYSTEM:WINDOWS")

if (${RE_PROJECT}_STANDALONE AND NOT ${RE_PROJECT}_STANDALONE_DUMP_HOOKS_ONLY)
    # The original data range (0x858000 ... 0xCB0000) must be at its original VA. A VirtualAlloc at startup is NOT reliable (the process heap /
    # NLS mappings already sit in low memory, see .notes/P2A_DESIGN.md), so the main image itself covers it: linked at the original base with a
    # 9 MB zero placeholder (tools/standalone/make_orig_pad.py: plain `.text`, FIRST object = start of .text = 0x401000). No relocations (/FIXED): original addresses are baked in.
    target_link_options(${RE_PROJECT_LIB_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/BASE:0x400000;/DYNAMICBASE:NO;/FIXED;/MERGE:_TEXT=.text;/INCREMENTAL:NO>")
    # Keep the exe/pdb/map out of <repo>/bin/<config>, where the DLL build puts gta_reversed.asi/.pdb
    set_target_properties(${RE_PROJECT_LIB_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # NOTE: the python needs `unicorn` (pip install unicorn) to replay the exe's static initialisers; pass -D${RE_PROJECT}_PYTHON=<venv python>
    find_program(${RE_PROJECT}_PYTHON NAMES python3 python NO_CMAKE_FIND_ROOT_PATH)
    if (NOT ${RE_PROJECT}_PYTHON)
        message(FATAL_ERROR "python3 not found (needed for the original-image placeholder and the data image extraction)")
    endif()
    # The placeholder object (plain `.text` section, see tools/standalone/make_orig_pad.py) MUST be the first object on the link line
    set(STANDALONE_PAD_OBJ "${CMAKE_BINARY_DIR}/orig_image_pad.obj")
    add_custom_command(
        OUTPUT "${STANDALONE_PAD_OBJ}"
        COMMAND "${${RE_PROJECT}_PYTHON}" -I "${PROJECT_SOURCE_DIR}/tools/standalone/make_orig_pad.py" "${STANDALONE_PAD_OBJ}"
        DEPENDS "${PROJECT_SOURCE_DIR}/tools/standalone/make_orig_pad.py"
        COMMENT "Writing the original-image placeholder object"
        VERBATIM
    )
    set_source_files_properties("${STANDALONE_PAD_OBJ}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    target_sources(${RE_PROJECT_LIB_NAME} PRIVATE "${STANDALONE_PAD_OBJ}")
    get_target_property(_all_sources ${RE_PROJECT_LIB_NAME} SOURCES)
    list(REMOVE_ITEM _all_sources "${STANDALONE_PAD_OBJ}")
    set_property(TARGET ${RE_PROJECT_LIB_NAME} PROPERTY SOURCES "${STANDALONE_PAD_OBJ}" ${_all_sources})

    if (${RE_PROJECT}_ORIGINAL_EXE)
        set(STANDALONE_DATA_DIR "${CMAKE_BINARY_DIR}/standalone_data")
        set(STANDALONE_DATA_OUT
            "${STANDALONE_DATA_DIR}/original_data.bin"
            "${STANDALONE_DATA_DIR}/original_data.json"
            "${STANDALONE_DATA_DIR}/data_pointers.bin"
            "${STANDALONE_DATA_DIR}/data_pointers.txt")
        add_custom_command(
            OUTPUT ${STANDALONE_DATA_OUT}
            COMMAND "${${RE_PROJECT}_PYTHON}" -I "${PROJECT_SOURCE_DIR}/tools/standalone/extract_exe_data.py"
                    "${${RE_PROJECT}_ORIGINAL_EXE}" "${STANDALONE_DATA_DIR}"
            DEPENDS "${PROJECT_SOURCE_DIR}/tools/standalone/extract_exe_data.py" "${PROJECT_SOURCE_DIR}/tools/standalone/initterm_delta.py" "${PROJECT_SOURCE_DIR}/.notes/symbols.txt" "${${RE_PROJECT}_ORIGINAL_EXE}"
            COMMENT "Extracting the original data image from ${${RE_PROJECT}_ORIGINAL_EXE}"
            VERBATIM
        )
        add_custom_target(${RE_PROJECT_LIB_NAME}_exe_data DEPENDS ${STANDALONE_DATA_OUT})
        add_dependencies(${RE_PROJECT_LIB_NAME} ${RE_PROJECT_LIB_NAME}_exe_data)
        # Next to the exe, where the loader looks for them
        add_custom_command(TARGET ${RE_PROJECT_LIB_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${STANDALONE_DATA_DIR}/original_data.bin" "${STANDALONE_DATA_DIR}/original_data.json" "${STANDALONE_DATA_DIR}/data_pointers.bin" "$<TARGET_FILE_DIR:${RE_PROJECT_LIB_NAME}>"
            VERBATIM
        )
    endif()
endif()

if (${RE_PROJECT}_RW_LIBRW)
    # librw as a static library built with the same (static) CRT. Options are forced before add_subdirectory (cache vars of the subproject).
    # NOTE: must come BEFORE the add_dependency() calls below: that macro leaks junk tokens into the directory-wide compile options.
    set(LIBRW_PLATFORM D3D9 CACHE STRING "" FORCE)
    set(LIBRW_TOOLS OFF CACHE BOOL "" FORCE)
    set(LIBRW_INSTALL OFF CACHE BOOL "" FORCE)
    set(LIBRW_EXAMPLES OFF CACHE BOOL "" FORCE)
    if (NOT EXISTS "${PROJECT_SOURCE_DIR}/vendor/librw/CMakeLists.txt")
        message(FATAL_ERROR "${RE_PROJECT}_RW_LIBRW needs the librw submodule: git submodule update --init vendor/librw")
    endif()
    add_subdirectory("${PROJECT_SOURCE_DIR}/vendor/librw" "${CMAKE_BINARY_DIR}/vendor/librw")
    # librw is third-party code: never -Werror (COMPILE_WARNING_AS_ERROR is set per target on ${RE_PROJECT_LIB_NAME} only)
    set_target_properties(librw PROPERTIES COMPILE_WARNING_AS_ERROR OFF)

    # P2B-01 unit test (defined here, before add_dependency() leaks junk into the directory compile options) of the RW math/stream shim (not part of `all`): ninja rw_math_stream_test
    if (EXISTS "${PROJECT_SOURCE_DIR}/tests/standalone/rw_math_stream_test.cpp")
        add_executable(rw_math_stream_test EXCLUDE_FROM_ALL
            "${PROJECT_SOURCE_DIR}/tests/standalone/rw_math_stream_test.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/math.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/rwmath_tables.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/standalone/rw/stream.cpp"
        )
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
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE NOTSA_RW_LIBRW RW_D3D9)
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
    set_source_files_properties(standalone/rw/pipeline.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    # P2B-05b: anonymous-namespace helpers in matfx.cpp / uvanim.cpp
    set_source_files_properties(standalone/rw/matfx.cpp standalone/rw/uvanim.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
endif()

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

    USE_D3D9 # TODO
)
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

    target_include_directories(${RE_PROJECT_LIB_NAME} PRIVATE
        game_sa/Audio/OpenAL
    )
else()
    target_compile_definitions(${RE_PROJECT_LIB_NAME} PRIVATE USE_DSOUND)

    target_link_libraries(${RE_PROJECT_LIB_NAME} PRIVATE dsound)
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
