cmake_minimum_required(VERSION 4.2.0 FATAL_ERROR)

if (MSVC AND NOT CMAKE_GENERATOR MATCHES "Ninja")
    set(CMAKE_GENERATOR_PLATFORM Win32)
endif()


project(GTASA C CXX)
set(RE_PROJECT GTASA)
set(RE_PROJECT_LIB_NAME gta_reversed)
set(RE_PROJECT_APP_NAME gta_reversed_app)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC_TOOLSET_VERSION)
    message("-- MSVC_TOOLSET_VERSION: ${MSVC_TOOLSET_VERSION}")
endif()

option(${RE_PROJECT}_NO_EDIT_AND_CONTINUE "Disable Edit&Continue" OFF)

if (MSVC)
    add_compile_options(/bigobj /utf-8)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    if (${RE_PROJECT}_NO_EDIT_AND_CONTINUE)
        add_compile_options(/Z7 /FS)
        add_link_options(/MAP) # /SAFESEH:NO disabled, /MAP added for Wine build
    else()
        # For Edit&Continue
        add_compile_options(/ZI)
        add_link_options(/SAFESEH:NO)
    endif()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
set_property(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT ${RE_PROJECT_LIB_NAME}
)

# Put DLLs in root
set(CMAKE_INSTALL_BINDIR .)
# And other components in its own folder
set(CMAKE_INSTALL_LIBDIR ${RE_PROJECT_LIB_NAME}/lib)
set(CMAKE_INSTALL_INCLUDEDIR ${RE_PROJECT_LIB_NAME}/include)


# Opt-in MSVC compile cache (docs/WINDOWS_SETUP.md "Compile cache"). OFF by default: the default build is unchanged.
#   -DGTASA_COMPILE_CACHE=ON                 find sccache (C:/gta-tools/sccache/sccache.exe, GTA_TOOLS, PATH)
#   -DGTASA_COMPILE_CACHE=<path/sccache.exe> use that binary
# sccache cannot cache a compile that uses a precompiled header (/Yu), so with the cache on the PCH is replaced by a forced include (/FI) of the
# same header: no PCH speed-up for cache MISSES (~12-25x slower than the PCH build), cache HITS skip the compiler altogether. The forced-include
# build needs ODR-clean headers (no non-inline definitions in headers: the PCH object used to hold them once).
# Debug info is /Z7 (object-embedded, set above with GTASA_NO_EDIT_AND_CONTINUE); /ZI (Edit&Continue) is not cacheable.
set(GTASA_COMPILE_CACHE "OFF" CACHE STRING "OFF | ON | path to sccache.exe: compile cache for MSVC (opt-in)")
if (MSVC AND NOT "${GTASA_COMPILE_CACHE}" STREQUAL "OFF" AND NOT "${GTASA_COMPILE_CACHE}" STREQUAL "")
    if ("${GTASA_COMPILE_CACHE}" STREQUAL "ON")
        find_program(GTASA_SCCACHE_EXE sccache HINTS "$ENV{GTA_TOOLS}/sccache" "C:/gta-tools/sccache" NO_CACHE)
    else()
        set(GTASA_SCCACHE_EXE "${GTASA_COMPILE_CACHE}")
    endif()
    if (NOT GTASA_SCCACHE_EXE OR NOT EXISTS "${GTASA_SCCACHE_EXE}")
        message(FATAL_ERROR "GTASA_COMPILE_CACHE=${GTASA_COMPILE_CACHE}: sccache not found (tools/standalone/windows/get_sccache.sh installs it under C:/gta-tools)")
    endif()
    if (NOT ${RE_PROJECT}_NO_EDIT_AND_CONTINUE)
        message(FATAL_ERROR "GTASA_COMPILE_CACHE needs GTASA_NO_EDIT_AND_CONTINUE=ON (/Z7): /ZI is not cacheable")
    endif()
    message(STATUS "Compile cache: ${GTASA_SCCACHE_EXE} (PCH replaced by a forced include)")
    # `cmake -E env` so that the sccache server, started by the first compile, gets the settings (a user-set variable of the same name wins):
    #  - SCCACHE_DIR / SCCACHE_CACHE_SIZE: one cache for every worktree (disk cache, shared between the servers below)
    #  - SCCACHE_BASEDIRS = this source tree: the key is computed from the preprocessed text, which has absolute paths (#line, __FILE__): without a base dir a
    #    worktree in another directory never hits. BASEDIRS is a server setting, hence one sccache server per source tree (port derived from its path).
    #    Caveat: a cached object carries the paths of the worktree that compiled it (/Z7 debug info, __FILE__ in asserts).
    set(_cc_env)
    if (NOT DEFINED ENV{SCCACHE_DIR})
        if (DEFINED ENV{GTA_TOOLS})
            list(APPEND _cc_env "SCCACHE_DIR=$ENV{GTA_TOOLS}/sccache-cache")
        else()
            list(APPEND _cc_env "SCCACHE_DIR=C:/gta-tools/sccache-cache")
        endif()
    endif()
    if (NOT DEFINED ENV{SCCACHE_CACHE_SIZE})
        list(APPEND _cc_env "SCCACHE_CACHE_SIZE=8G")
    endif()
    if (NOT DEFINED ENV{SCCACHE_BASEDIRS})
        list(APPEND _cc_env "SCCACHE_BASEDIRS=${CMAKE_SOURCE_DIR}")
    endif()
    if (NOT DEFINED ENV{SCCACHE_SERVER_PORT})
        string(MD5 _cc_md5 "${CMAKE_SOURCE_DIR}")
        string(SUBSTRING "${_cc_md5}" 0 4 _cc_md5)
        math(EXPR _cc_port "4400 + (0x${_cc_md5} % 3000)")
        list(APPEND _cc_env "SCCACHE_SERVER_PORT=${_cc_port}")
        message(STATUS "Compile cache: sccache server port ${_cc_port}, base dir ${CMAKE_SOURCE_DIR}")
    endif()
    set(_cc_launcher "${CMAKE_COMMAND}" -E env ${_cc_env} "${GTASA_SCCACHE_EXE}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${_cc_launcher}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${_cc_launcher}")
    # PCH -> forced include of the same header (callers pass `<PRIVATE|PUBLIC|INTERFACE> <header>`, paths relative to the calling directory)
    function(target_precompile_headers target)
        set(_scope PRIVATE)
        foreach (_h IN LISTS ARGN)
            if (_h MATCHES "^(PRIVATE|PUBLIC|INTERFACE)$")
                set(_scope ${_h})
            else()
                cmake_path(ABSOLUTE_PATH _h BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE _abs)
                target_compile_options(${target} ${_scope} "$<$<COMPILE_LANGUAGE:CXX>:/FI${_abs}>")
            endif()
        endforeach()
    endfunction()
endif()

add_subdirectory(source)

function(create_source_group target)
    get_property(sources TARGET ${target} PROPERTY SOURCES)
    foreach (filename ${sources})
        source_group("${path}" FILES ${filename})
    endforeach ()
endfunction()
create_source_group(${RE_PROJECT_LIB_NAME})

