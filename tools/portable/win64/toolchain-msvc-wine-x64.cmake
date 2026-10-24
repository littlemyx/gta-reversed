# CMake toolchain for cross-compiling x64 MSVC through Wine (msvc-wine wrappers in <MSVC_WINE_ROOT>/bin/x64). x64 twin of the x86 recipe in contrib/msvc-wine.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if (DEFINED ENV{MSVC_WINE_ROOT})
    set(_default_root "$ENV{MSVC_WINE_ROOT}")
else()
    set(_default_root "$ENV{HOME}/tools/msvc")
endif()
set(MSVC_WINE_ROOT "${_default_root}" CACHE PATH "msvc-wine installation root")

file(GLOB _msvc_versions "${MSVC_WINE_ROOT}/vc/tools/msvc/*")
list(SORT _msvc_versions ORDER DESCENDING)
list(GET _msvc_versions 0 _msvc_ver_path)
get_filename_component(MSVC_VERSION_DIR "${_msvc_ver_path}" NAME)
file(GLOB _sdk_versions "${MSVC_WINE_ROOT}/kits/10/include/*")
list(SORT _sdk_versions ORDER DESCENDING)
list(GET _sdk_versions 0 _sdk_ver_path)
get_filename_component(SDK_VERSION "${_sdk_ver_path}" NAME)
message(STATUS "msvc-wine x64 toolchain: MSVC ${MSVC_VERSION_DIR}, SDK ${SDK_VERSION}")

set(MSVC_TOOLS "${MSVC_WINE_ROOT}/vc/tools/msvc/${MSVC_VERSION_DIR}")
set(SDK_ROOT "${MSVC_WINE_ROOT}/kits/10")
set(_bin "${MSVC_WINE_ROOT}/bin/x64")

set(CMAKE_C_COMPILER   "${_bin}/cl")
set(CMAKE_CXX_COMPILER "${_bin}/cl")
set(CMAKE_LINKER       "${_bin}/link")
set(CMAKE_RC_COMPILER  "${_bin}/rc")
set(CMAKE_MT           "${_bin}/mt")
set(CMAKE_AR           "${_bin}/lib")

# The wrapper banner goes through Wine's stderr and confuses CMake's compiler id probe: state the facts (19.40 is a version CMake knows; the real one is 19.51)
set(CMAKE_C_COMPILER_ID "MSVC" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_ID "MSVC" CACHE STRING "" FORCE)
set(CMAKE_C_COMPILER_VERSION "19.40" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_VERSION "19.40" CACHE STRING "" FORCE)
set(MSVC_VERSION 1940 CACHE STRING "" FORCE)
set(MSVC_TOOLSET_VERSION 143 CACHE STRING "" FORCE)
set(MSVC TRUE CACHE BOOL "" FORCE)
set(CMAKE_C_STANDARD_COMPUTED_DEFAULT "11" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_COMPUTED_DEFAULT "14" CACHE STRING "" FORCE)
set(CMAKE_C_EXTENSIONS_COMPUTED_DEFAULT "OFF" CACHE STRING "" FORCE)
set(CMAKE_CXX_EXTENSIONS_COMPUTED_DEFAULT "OFF" CACHE STRING "" FORCE)

set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES
    "${MSVC_TOOLS}/include"
    "${SDK_ROOT}/include/${SDK_VERSION}/ucrt"
    "${SDK_ROOT}/include/${SDK_VERSION}/shared"
    "${SDK_ROOT}/include/${SDK_VERSION}/um"
    "${SDK_ROOT}/include/${SDK_VERSION}/winrt"
)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_C_STANDARD_INCLUDE_DIRECTORIES})

link_directories(
    "${MSVC_TOOLS}/lib/x64"
    "${SDK_ROOT}/lib/${SDK_VERSION}/ucrt/x64"
    "${SDK_ROOT}/lib/${SDK_VERSION}/um/x64"
)

# CMake's TryCompile ABI probe generates /Fd with an empty PDB path (C1902): skip it, the compiler is known to work
set(CMAKE_C_COMPILER_WORKS TRUE CACHE BOOL "" FORCE)
set(CMAKE_CXX_COMPILER_WORKS TRUE CACHE BOOL "" FORCE)
set(CMAKE_C_ABI_COMPILED TRUE CACHE BOOL "" FORCE)
set(CMAKE_CXX_ABI_COMPILED TRUE CACHE BOOL "" FORCE)
set(CMAKE_SIZEOF_VOID_P 8 CACHE STRING "" FORCE)
set(CMAKE_C_SIZEOF_DATA_PTR 8 CACHE STRING "" FORCE)
set(CMAKE_CXX_SIZEOF_DATA_PTR 8 CACHE STRING "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
