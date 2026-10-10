/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "app/app_debug.h"
#include <rw/rwplcore.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#include <ranges>
namespace rng = std::ranges;
namespace rngv = std::views;

#include <filesystem>
namespace fs = std::filesystem;

#define PLUGIN_API

#define VALIDATE_SIZE(struc, size) static_assert(sizeof(struc) == size, "Invalid structure size of " #struc)
#define VALIDATE_OFFSET(struc, member, offset) \
	static_assert(offsetof(struc, member) == offset, "The offset of " #member " in " #struc " is not " #offset "...")

VALIDATE_SIZE(bool, 1);
VALIDATE_SIZE(char, 1);
VALIDATE_SIZE(short, 2);
VALIDATE_SIZE(int, 4);
VALIDATE_SIZE(float, 4);
VALIDATE_SIZE(long long, 8);

// Use this to add const that wasn't there in the original code
#define Const const

// Basic types for structures describing
typedef int8_t    int8;
typedef int16_t   int16;
typedef int32_t   int32;
typedef int64_t   int64;
typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;
typedef uint64_t  uint64;
typedef intptr_t  intptr;
typedef uintptr_t uintptr;

typedef uint8     bool8;
typedef uint16    bool16;
typedef uint32    bool32;

#if (defined(__GNUC__) || defined(__GNUG__) || defined(__clang__))
#define UNREACHABLE_INTRINSIC(...) __builtin_unreachable()
#elif (defined(_MSC_VER))
#define UNREACHABLE_INTRINSIC(...) __assume(false)
#else
#define UNREACHABLE_INTRINSIC(...) assert(false)
#endif

namespace notsa {
/*!
* @return Return source code's base path
*/
fs::path GetSourceCodeBasePath();
};

// Use the `NOTSA_UNREACHABLE` macro for unreachable code paths.
// In debug mode it will do a DebugBreak() and print a message to the console,
// while in release code it'll be optimized away (by using special compiler directives)
// Also serves as a way to supress compiler warnings (for example, when you don't have need a `default` case in a `switch`)
#if _DEBUG
#include <format>
#include <winuser.h>

namespace notsa {
template<typename... Ts>
[[noreturn]] static void unreachable(std::string_view method, std::string_view file, unsigned line, std::string userDetails = "<None provided>") {
    const auto mbMsg = std::format(
        "File: {}\nIn: {}:{}\n\nDetails:\n{}",
        fs::relative(file, GetSourceCodeBasePath()).string(),
        method,
        line,
        userDetails
    );

    spdlog::error(mbMsg);
    spdlog::dump_backtrace();
    spdlog::apply_all([](std::shared_ptr<spdlog::logger> l) { // Flush all sinks immidiately
        l->flush();
    });

    const auto result = MessageBox(
        NULL,
        mbMsg.c_str(),
        "Unreachable code reached! SEND HALP IMMEDIATELY!",
        MB_TASKMODAL | MB_ICONHAND | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND
    );

    switch (result) {
    case IDRETRY: 
        __debugbreak();  // Cause a debug break 
        [[fallthrough]]; // If it's "Continue"'d fall to exit anyways, because function is no-return
    case IDIGNORE:
    case IDABORT:
    default:
        // This is here because:
        // - The compiler will optimize unreachable code away in release builds
        // - No warnings will be generated for missing `return`s
        // - If this function is ever called we're supposed to abort
        exit(3);
    }
}
};
// TODO/NOTE: We might need to manually suppress warnings here?
// Since all the code here is perfectly valid, so the compiler might
// still complain that, for example, the function doesn't return on all code paths, etc
#define IMPL_NOTSA_UNREACHABLE_FMT_ARGS(...) std::format(__VA_ARGS__)
#define NOTSA_UNREACHABLE(...) do { ::notsa::unreachable(__FUNCTION__, __FILE__, __LINE__ __VA_OPT__(,IMPL_NOTSA_UNREACHABLE_FMT_ARGS(__VA_ARGS__))); } while (false)
#elif defined(NOTSA_STANDALONE_RUN)
// Standalone run build (NDEBUG): NO undefined behaviour where the exe is defined. `__assume(false)` lets the compiler drop the epilogue of the function
// (execution then runs into the NEXT function) and everything after the site; the exe simply continues with whatever its code does next. So this logs
// the site once (standalone.log: "UNREACHABLE hit: ...", the reachable ones need an explicit exe-semantics fix) and falls through.
namespace notsa::standalone::Fixups { void UnreachableHit(const char* fn, const char* file, int line); }
#define NOTSA_UNREACHABLE(...) do { ::notsa::standalone::Fixups::UnreachableHit(__FUNCTION__, __FILE__, __LINE__); } while (false)
#else 
#define NOTSA_UNREACHABLE(...) UNREACHABLE_INTRINSIC()
#endif
#define NOTSA_UNUSED_FUNCTION() NOTSA_UNREACHABLE("Unused Function")
#define NOTSA_UNREACHABLE_CASE(val) NOTSA_UNREACHABLE("Unreachable switch case with value: {}", val)

#ifdef _DEBUG
#define NOTSA_DEBUG_BREAK() __debugbreak()
#else
#define NOTSA_DEBUG_BREAK()
#endif

// In order to be able to get the vtable address using GetProcAddress
// the whole class must be exported. (Along which the vtable is exported as well)
// See `ReversibleHooks::detail::GetClassVTableAddress`
// This should be added to every and all class with a vtable
#define NOTSA_EXPORT_VTABLE __declspec(dllexport)

// Macro for unused function arguments - Use it to avoid compiler warnings of unused arguments
#define UNUSED(x) (void)(x);

// Macro for unused function return values.
// Eventually could instead verify the returned value? In case of `sscanf` etc...
#define RET_IGNORED(x) (void)(x);

//! Cause a debug break
#define NOTSA_DEBUGBREAK() __debugbreak()

//! switch case fallthru
#define NOTSA_SWCFALLTHRU [[fallthrough]]

//! Macro for passing a string var to *scanf_s function.
#define SCANF_S_STR(s) s, std::size(s)

#define NOTSA_FORCEINLINE __forceinline

/*!
* @brief Used for static variable references
*
* @tparam T    The type of the variable
* @param Addr  The address of it
*/
#ifdef NOTSA_STANDALONE_RUN
namespace notsa::standalone::detail {
// Set by source/standalone/DataImage.cpp once the original data image (.rdata/.data/BSS) is mapped at its original VA.
// It is loaded from a `.CRT$XIB` initializer, i.e. before any C++ dynamic initializer, see .notes/P2A_DESIGN.md
extern bool g_DataImageLoaded;
}
#endif

template<typename T>
T& StaticRef(uintptr addr) {
#ifdef NOTSA_STANDALONE_RUN
    // Standalone run: the original data image lives at the ORIGINAL addresses, so this is a plain dereference (like the DLL build)
    // Debug builds verify that nobody touches a global before the image is mapped.
    assert(notsa::standalone::detail::g_DataImageLoaded);
    return *reinterpret_cast<T*>(addr);
#elif defined(NOTSA_STANDALONE_DUMP_HOOKS_ONLY)
    // NOTE/BUG:
    // In NOTSA_STANDALONE_DUMP_HOOKS_ONLY, StaticRef() returns a single per-type static buffer for all addresses.
    // That aliases unrelated globals of the same type (e.g., many StaticRef<int32>(...)), so writes intended for one address will overwrite the dummy storage for another.
    // This can corrupt state during hook registration and make dump output unreliable/non-deterministic.
    // It can be easily fixed by putting the address in the template too, but we're not doing that yet because I guess it would impact compile times + it'd be a big diff in terms of code for now
    alignas(alignof(T)) static uint8 buf[sizeof(T)]{};
    return *reinterpret_cast<T*>(buf);
#else
    return *reinterpret_cast<T*>(addr);
#endif
}

/*!
 * @brief Use for scoped static variables (That is, static variables that are initialized in functions)
 * @brief See `CAEGlobalWeaponAudioEntity::ServiceAmbientGunFire` for examples)
 * @tparam T The type of the var
 * @param varAddr 
 * @param flagsAddr 
 * @param flagsMask 
 * @param initVal 
 * @return 
 */
template<typename T>
T& ScopedStaticRef(uintptr varAddr, uintptr flagsAddr, uint32 flagsMask, T&& initVal) {
    auto& var   = StaticRef<T>(varAddr);
    auto& flags = StaticRef<uint32>(flagsAddr);
    if (!(flags & flagsMask)) {
        flags |= flagsMask;
        var    = initVal;
    }
    return var;
}

/*!
 * @brief NOTSA_GLOBAL*: declare a game global that the original exe keeps at a fixed address (slice A2 of .notes/DETACH_DATA_PLAN.md).
 *
 * Three modes (the same source line serves all of them):
 *  1. ADDRESS (default: ASI/DLL build, hook dump, everything that is not "detached"): the global is a reference to the original storage,
 *     `auto& name = StaticRef<Type>(addr)` - exactly what the hand-written declarations were, bit for bit.
 *  2. DETACHED (`NOTSA_DETACHED_GLOBALS`, CMake `GTASA_DETACHED_GLOBALS`, run build only): a real C++ variable with the exe's initial value
 *     (`init`), no use of the original data image.
 *  3. ORACLE (`NOTSA_ORACLE_ADDRESS_GLOBALS`): forces mode 1 even when the build configuration is detached. The oracle tests compile game sources
 *     next to the REAL original code, both must see the same storage at the original VA.
 *
 * @param name  declarator (unqualified; class static members keep their name, so no use site changes)
 * @param addr  original virtual address
 * @param type  PARENTHESISED type, e.g. `(float)`, `(bool[92])`, `(std::array<int, 4>)`, `(void (*[92])())`
 * @param ...   initialiser appended verbatim after the declarator in detached mode: `{}`, `{ 1.0f }`, `{ 1, 2, 3 }`, `= 5`. Ignored in address mode.
 *
 * The call site supplies the storage specifiers and the trailing `;`:
 *    class scope / namespace scope of a .cpp:   `static inline NOTSA_GLOBAL(m_x, 0xB7CB84, (uint32), {});`
 *    function scope:                              `NOTSA_GLOBAL_LOCAL(v, 0xC0FFEE, (int), {});`   (detached: `static T v`)
 *    namespace scope of a header:                 `NOTSA_GLOBAL_HDR(g, 0xA, (int), {});`          (address: `static inline auto&`, detached: `inline T`)
 *                                                 `NOTSA_GLOBAL_HDR_EXT(...)` for a header variable declared `inline auto&` WITHOUT `static` (external linkage; address: `inline auto&`, so the object code stays identical)
 *    big/pointer-table initialisers: header `static NOTSA_GLOBAL_DECL(CCheat, name, 0xA, (T));` + the .cpp `NOTSA_GLOBAL_DEF(CCheat, name, 0xA, (T), { &f, ... });`
 *      (address mode: DECL = the old `static inline auto&`, DEF = nothing)
 *    header declaration of a namespace-scope .cpp global:   `NOTSA_GLOBAL_EXTERN(name, (T));`   (address: `extern T& name;`, detached: `extern T name;`)
 *
 * With `NOTSA_VERIFY_GLOBALS` every detached global registers itself for `tools/standalone/verify_globals.py` (source/standalone/GlobalsVerify.h).
 *
 * Aliases / synthetics decided by .notes/aliases.json (codemod_globals.py): the address-mode text never mentions the owner, so the default build is unchanged:
 *    owner that no site can host (shared header / TU-local):   `NOTSA_GLOBAL_SYNTH(k_name, 0xADDR, (T), init);`       (address mode: nothing)
 *    declaration that aliases the owner:                      `NOTSA_GLOBAL_ALIAS(n, 0xADDR, (T), Owner);`          (detached: `auto& n = Owner`)
 *    expression use of the address:                           `NOTSA_GLOBAL_EXPR(0xADDR, (T), Owner)`               (detached: `(Owner)`)
 *    lazy `ScopedStaticRef` variable (flag word dropped):      `NOTSA_SCOPED_GLOBAL(v, 0xVAR, 0xFLAGS, mask, (T), initVal);`  (detached: `static T v = initVal`)
 *
 * Read-only tables that are plain C++ data in both modes (not touched by the default build: an unused internal `constexpr` array costs nothing):
 *    `constexpr T kTable[N] = {...};  NOTSA_GLOBAL_VERIFY(0xADDR, kTable);`  (verify_globals.py checks it against the image in detached+verify builds)
 *    in the function that reads it:  `NOTSA_GLOBAL_LOCAL_REF(t, 0xADDR, (const T[N]), kTable);`  (`t` is a POINTER to the first element: address `reinterpret_cast<const T*>(addr)`, exactly the old raw cast; detached `&kTable[0]`)
 */
#if defined(NOTSA_DETACHED_GLOBALS) && !defined(NOTSA_ORACLE_ADDRESS_GLOBALS)
#define NOTSA_GLOBALS_DETACHED 1
#endif

#define NOTSA_UNPAREN(...) __VA_ARGS__
#define NOTSA_GLOBAL_CAT_(a, b) a##b
#define NOTSA_GLOBAL_CAT(a, b) NOTSA_GLOBAL_CAT_(a, b)

#ifdef NOTSA_GLOBALS_DETACHED
#include <type_traits>
#ifdef NOTSA_VERIFY_GLOBALS
#include "standalone/GlobalsVerify.h"
// `reg` is a second declaration right behind the variable (the call site's `;` terminates it); `qname` is what `&` is applied to
#define NOTSA_GLOBAL_REG_(spec, reg, qname, name, addr) \
    ; spec ::notsa::globals::Reg reg { #name, __FILE__, __LINE__, (addr), sizeof(qname), &(qname) }
#else
#define NOTSA_GLOBAL_REG_(spec, reg, qname, name, addr)
#endif
namespace notsa { inline constexpr bool kGlobalsDetached = true; }
#define NOTSA_GLOBAL(name, addr, type, ...)       std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__ NOTSA_GLOBAL_REG_(static inline const, NOTSA_GLOBAL_CAT(name, _gReg_), name, name, addr)
#define NOTSA_GLOBAL_HDR(name, addr, type, ...)   inline std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__ NOTSA_GLOBAL_REG_(inline const, NOTSA_GLOBAL_CAT(name, _gReg_), name, name, addr)
#define NOTSA_GLOBAL_HDR_EXT(name, addr, type, ...) NOTSA_GLOBAL_HDR(name, addr, type, __VA_ARGS__)
#define NOTSA_GLOBAL_LOCAL(name, addr, type, ...) static std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__
#define NOTSA_GLOBAL_LOCAL_REF(name, addr, type, obj) const auto* name = &(obj)[0]
#define NOTSA_GLOBAL_ALIAS(name, addr, type, ...) auto& name = __VA_ARGS__
#define NOTSA_GLOBAL_EXPR(addr, type, ...) (__VA_ARGS__)
#define NOTSA_SCOPED_GLOBAL(name, varAddr, flagsAddr, mask, type, ...) static std::type_identity_t<NOTSA_UNPAREN type> name = __VA_ARGS__
#define NOTSA_GLOBAL_SYNTH(name, addr, type, ...) inline std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__ NOTSA_GLOBAL_REG_(inline const, NOTSA_GLOBAL_CAT(name, _gReg_), name, name, addr)
#ifdef NOTSA_VERIFY_GLOBALS
#define NOTSA_GLOBAL_VERIFY(addr, obj) static inline const ::notsa::globals::Reg NOTSA_GLOBAL_CAT(obj, _gVer_) { #obj, __FILE__, __LINE__, (addr), sizeof(obj), &(obj) }
#else
#define NOTSA_GLOBAL_VERIFY(addr, obj) static_assert(true)
#endif
#define NOTSA_GLOBAL_DECL(cls, name, addr, type)  std::type_identity_t<NOTSA_UNPAREN type> name
#define NOTSA_GLOBAL_EXTERN(name, type)           extern std::type_identity_t<NOTSA_UNPAREN type> name
#define NOTSA_GLOBAL_DEF(cls, name, addr, type, ...) \
    std::type_identity_t<NOTSA_UNPAREN type> cls::name __VA_ARGS__ NOTSA_GLOBAL_REG_(static inline const, NOTSA_GLOBAL_CAT(NOTSA_GLOBAL_CAT(name, _gRegDef_), __COUNTER__), cls::name, name, addr)
#else
namespace notsa { inline constexpr bool kGlobalsDetached = false; }
#define NOTSA_GLOBAL(name, addr, type, ...)       auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_HDR(name, addr, type, ...)   static inline auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_HDR_EXT(name, addr, type, ...) inline auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_LOCAL(name, addr, type, ...) static auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_LOCAL_REF(name, addr, type, obj) auto* name = reinterpret_cast<std::remove_extent_t<NOTSA_UNPAREN type>*>(addr)
#define NOTSA_GLOBAL_ALIAS(name, addr, type, ...) auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_EXPR(addr, type, ...) StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_SCOPED_GLOBAL(name, varAddr, flagsAddr, mask, type, ...) static auto& name = ScopedStaticRef<NOTSA_UNPAREN type>(varAddr, flagsAddr, mask, __VA_ARGS__)
#define NOTSA_GLOBAL_SYNTH(name, addr, type, ...) static_assert(true)
#define NOTSA_GLOBAL_VERIFY(addr, obj) static_assert(true)
#define NOTSA_GLOBAL_DECL(cls, name, addr, type)  inline auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_EXTERN(name, type)           extern NOTSA_UNPAREN type& name
#define NOTSA_GLOBAL_DEF(cls, name, addr, type, ...) static_assert(true)
#endif

template<typename T>
void SAFE_RELEASE(T*& ptr) { // DirectX stuff `Release()`
    if (ptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

// std::format support for enums
// either using `EnumToString`, or using the enum name and value as a fallback
template<typename Enum>
    requires std::is_enum_v<Enum>
struct std::formatter<Enum> : std::formatter<std::string> {
    auto format(Enum e, format_context& ctx) const {
        if constexpr (requires { EnumToString(e); }) {
            if (const auto name = EnumToString(e)) {
                return formatter<string>::format(*name, ctx);
            }
        }
        return formatter<string>::format(
            std::format("{} ({})", typeid(Enum).name(), static_cast<std::underlying_type_t<Enum>>(e)),
            ctx
        );
    }
};

#define _IGNORED_
#define _CAN_BE_NULL_

// TODO: Use premake/cmake for this instead of relaying on `_DEBUG`
#if defined(_DEBUG) && !defined(NOTSA_STANDALONE_RUN) // D8: the runnable standalone build is behaviour-identical to the original (no debug-only cheats/UI hooks), whatever its CRT config
#define NOTSA_DEBUG 1
#endif

#if (defined(__GNUC__) || defined(__GNUG__) || defined(__clang__))
#define PLUGIN_SOURCE_FILE
#define PLUGIN_VARIABLE
#define _NOINLINE_
#elif (defined(_MSC_VER))
#define PLUGIN_SOURCE_FILE  __pragma(init_seg(lib))
#define PLUGIN_VARIABLE
#define _NOINLINE_ __declspec(noinline)
#else
#define PLUGIN_SOURCE_FILE
#define PLUGIN_VARIABLE
#define _NOINLINE_
#endif

// III/VC char > wchar_t string conversion
#define _SWSTRING_INIT(str, id) std::wstring my_ws##id; for (size_t i = 0; i < strlen(str); i++) my_ws##id += str[i]
#define _SWSTRING(id) const_cast<wchar_t *>(my_ws##id.c_str())
#define _SWSTRING_STATIC_INIT(id) static wchar_t my_ws##id[512] ; my_ws##id[0] = 0
#define _SWSTRING_STATIC(id) my_ws##id
#define _SWSTRING_STATIC_FROM(id, src) for (size_t i = 0; i < strlen(src); i++) my_ws##id[i] = src[i]
#define _SWSTRING_STATIC_TO(id, dst) for (size_t i = 0; i < wcslen(my_ws##id); i++) dst[i] = static_cast<char>(my_ws##id[i])

#ifdef NOTSA_RW_LIBRW
// RwRGBAReal is an alias of rw::RGBAf: nlohmann finds (de)serializers by ADL, so they have to live next to the aliased type
namespace rw { NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RGBAf, red, blue, green, alpha); }
#else
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RwRGBAReal, red, blue, green, alpha);
#endif
