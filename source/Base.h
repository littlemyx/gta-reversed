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

// Layout checks. Two kinds, because the original exe is a 32-bit x86 program and a 64-bit build cannot (and need not) keep the in-memory layout:
//  * VALIDATE_SIZE / VALIDATE_OFFSET      - the x86 IN-MEMORY layout of the original exe (the struct is only ever accessed through memory, never
//                                           as raw bytes of a file). Enforced when the code is built as the 32-bit x86 reference; compiled out
//                                           under NOTSA_PORTABLE (pointers, size_t, long ... grow on 64-bit targets).
//  * VALIDATE_SIZE_FILE / VALIDATE_OFFSET_FILE - the layout is an ON-DISK / raw-byte format (read or written with fread / CFileMgr::Read /
//                                           RwStreamRead / memcpy from a file buffer / save-game blocks / IMG, COL, IPL, IFP, DAT headers /
//                                           script-file structures). Enforced in EVERY build, including NOTSA_PORTABLE: a failure on a 64-bit
//                                           target means the struct needs a portable on-disk definition. There is deliberately no switch that
//                                           disables these.
#define VALIDATE_SIZE_FILE(struc, size) static_assert(sizeof(struc) == size, "Invalid structure size of " #struc)
#define VALIDATE_OFFSET_FILE(struc, member, offset) \
	static_assert(offsetof(struc, member) == offset, "The offset of " #member " in " #struc " is not " #offset "...")
#define VALIDATE_LAYOUT_FILE(...) static_assert(__VA_ARGS__) // any other constant expression over an on-disk layout (arguments of static_assert)
#ifdef NOTSA_PORTABLE
#define VALIDATE_SIZE(struc, size) static_assert(true) // x86 in-memory layout: not checked in a portable build
#define VALIDATE_OFFSET(struc, member, offset) static_assert(true)
#define VALIDATE_LAYOUT(...) static_assert(true)
#else
#define VALIDATE_SIZE(struc, size) VALIDATE_SIZE_FILE(struc, size)
#define VALIDATE_OFFSET(struc, member, offset) VALIDATE_OFFSET_FILE(struc, member, offset)
#define VALIDATE_LAYOUT(...) static_assert(__VA_ARGS__) // any other constant expression over the x86 in-memory layout (arguments of static_assert)
#endif

VALIDATE_SIZE_FILE(bool, 1);
VALIDATE_SIZE_FILE(char, 1);
VALIDATE_SIZE_FILE(short, 2);
VALIDATE_SIZE_FILE(int, 4);
VALIDATE_SIZE_FILE(float, 4);
VALIDATE_SIZE_FILE(long long, 8);

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

//! x87d: the type of an arithmetic expression the exe evaluates on the x87 stack with the FPU precision control at PC=24 (D3D9 CreateDevice without
//! FPU_PRESERVE): EVERY operation result is rounded to a 24 bit significand, so the faithful equivalent on SSE/arm64 is IEEE single arithmetic per operation.
//! The reversed code often spells such expressions with (double) casts / double locals; write them with x87d instead (tools/portable/pc24_scan.py finds and
//! rewrites them). `double` in every non-portable build (the Win32 reference keeps its x87 code, bytes unchanged), `float` when NOTSA_PORTABLE. RR_PLAN 11.8.
//! Exact-float literals in such an expression are written `(x87d)1.0`; a literal that is not exactly a float (0.02) needs fp::MulD / fp::AddD (Core/Fp.h).
#ifdef NOTSA_PORTABLE
typedef float x87d;
#else
typedef double x87d;
#endif
//! X87R(expr): `expr` is evaluated as written (it has a genuine 53 bit operand: a double global / call result / x87:: transcendental), the exe rounds the RESULT of the
//! operation to a 24 bit significand (PC=24): portable rounds to float, every other build leaves the TOKENS untouched (neither a cast nor parentheses: MSVC /fp:precise spills at both).
#ifdef NOTSA_PORTABLE
#define X87R(...) ((x87d)(__VA_ARGS__))
#else
#define X87R(...) __VA_ARGS__
#endif
//! X87W(expr): `expr` (an x87d / float-valued expression, already rounded per operation) is handed to a CRT function taking a DOUBLE (log10 / pow / exp ...): portable widens it
//! explicitly so that std::log10 & co. pick the double overload (a float argument selects the float overload = less precise than the exe's CRT call); every other build leaves the TOKENS untouched.
#ifdef NOTSA_PORTABLE
#define X87W(...) ((double)(__VA_ARGS__))
#else
#define X87W(...) __VA_ARGS__
#endif

//! NOTSA_FTOL(T, expr): the float / double -> integer conversion `(T)(expr)` of the exe (MAC-FTOL1, tools/portable/ftol_scan.py lists the sites). The exe's CRT converts through `_ftol2`
//! (x87 fistp, truncation, low dword), so a NaN / inf / out-of-range value (a junk float from a file, a position that left the map) gives a defined integer; a plain C++ cast is UB there,
//! saturates on arm64 and gives 0x80000000 on x86-64, which turns sector / grid / index arithmetic into 2^32-iteration loops or out-of-bounds accesses. Portable: notsa::fp::FtolCast<T>
//! (Core/Fp.h); every other build keeps the plain cast (same code bytes as before: `(T)(expr)`; NOTSA_FTOL(T, a * b) replaces `(T)(a * b)` token for token, without an extra pair of parentheses).
#ifdef NOTSA_PORTABLE
#include "game_sa/Core/Fp.h"
#define NOTSA_FTOL(T, ...) (::notsa::fp::FtolCast<T>(__VA_ARGS__))
#define NOTSA_FTOL_SC(T, ...) (::notsa::fp::FtolCast<T>(__VA_ARGS__))
#define NOTSA_FTOL_FN(T, ...) (::notsa::fp::FtolCast<T>(__VA_ARGS__))
#define NOTSA_FTOL_P(T, ...) (::notsa::fp::FtolCast<T>(__VA_ARGS__))
#else
#define NOTSA_FTOL(T, ...) (T)(__VA_ARGS__)   /* tokens of the usual `(T)(expr)` spelling: MSVC's x87 scheduling reacts to extra parentheses (MAC-FTOL1 A/B: CWeather::AddRain) */
#define NOTSA_FTOL_SC(T, ...) static_cast<T>(__VA_ARGS__)   /* the same for sites that were spelled static_cast<T>(expr): MAC-FTOL2 A/B found a codegen difference between the two spellings (GangWars FightTimer) */
#define NOTSA_FTOL_FN(T, ...) T(__VA_ARGS__)                /* ... for functional casts T(expr) */
#define NOTSA_FTOL_P(T, ...) (T)__VA_ARGS__                 /* ... for casts of a primary operand without parentheses: (T)x, (T)f(a) (MSVC x87/regalloc reacts to `(T)(x)`; Pickup revenue) */
#endif

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
#if _DEBUG && !defined(W2_UNREACHABLE_RUNBUILD)   // W2_UNREACHABLE_RUNBUILD: the wave2/3 oracle tests take the run-build branch below (log the site, fall through like the exe) instead of the MessageBox + exit(3) of the Debug build
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
#define NOTSA_UNREACHABLE_CASE_RET(RET, val) NOTSA_UNREACHABLE_RET(RET, "Unreachable switch case with value: {}", val)
// NOTSA_UNREACHABLE at the end of a non-void function / lambda: the Win32 build (MSVC) keeps the exact text of NOTSA_UNREACHABLE (the exe just runs off the end of
// the function and returns whatever is in EAX / ST0; the site is logged by the run build), while the portable build (clang: falling off the end of a non-void
// function is UB, it turns the path into `unreachable` and runs into the next function) logs the site and returns RET. RET is the first argument, the others are
// the (optional) format arguments of NOTSA_UNREACHABLE. MAC-C3, -Werror=return-type of the portable preset.
// The standalone run build (NOTSA_STANDALONE_RUN) logs the hit site and returns RET as well: NOTSA_UNREACHABLE does not end the control flow there, so without the
// `return` MSVC reports C4715 (not all control paths return a value), an error under /WX in a from-scratch build. Only the (never taken, logged) hit path changes.
#if defined(NOTSA_PORTABLE) || defined(NOTSA_STANDALONE_RUN)
#define NOTSA_UNREACHABLE_RET(RET, ...) do { NOTSA_UNREACHABLE(__VA_ARGS__); return RET; } while (false)
#else
#define NOTSA_UNREACHABLE_RET(RET, ...) NOTSA_UNREACHABLE(__VA_ARGS__)
#endif

// NOTSA-AUDIT (.notes/reports/NOTSA-AUDIT.md): an upstream NOTSA change that makes the game behave differently from the exe. The standalone run build
// (NOTSA_STANDALONE_RUN) must behave like the exe, so it takes `exeExpr`; the DLL build keeps upstream's `upstreamExpr` (the macro expands to exactly that
// expression there, the object code of the DLL build does not change). For statements use `#ifdef NOTSA_STANDALONE_RUN` and keep the line numbers (`#line`).
#ifdef NOTSA_STANDALONE_RUN
#define NOTSA_EXE_OR(exeExpr, upstreamExpr) (exeExpr)
#else
#define NOTSA_EXE_OR(exeExpr, upstreamExpr) (upstreamExpr)
#endif

#ifdef _DEBUG
#define NOTSA_DEBUG_BREAK() __debugbreak()
#else
#define NOTSA_DEBUG_BREAK()
#endif

// In order to be able to get the vtable address using GetProcAddress
// the whole class must be exported. (Along which the vtable is exported as well)
// See `ReversibleHooks::detail::GetClassVTableAddress`
// This should be added to every and all class with a vtable
// F2 (.notes/DETACH_DATA_PLAN.md): the detached run build has no hook registry and nothing reads the exported vtables any more (VMTInfo::FindByClassName was the only
// consumer): the attribute is empty there, so the exe no longer exports ~540 classes with all their members.
#include "reversiblehooks/NoHookRegistry.h"
#ifdef NOTSA_NO_HOOK_REGISTRY
#define NOTSA_EXPORT_VTABLE
#else
#define NOTSA_EXPORT_VTABLE __declspec(dllexport)
#endif

// `inline` on a member that is only DEFINED in a .cpp but called from other TUs is an ODR violation (CRect, CMatrixLink[List]): it linked because the exported class / the hook's `&fn`
// forced an out-of-line copy. Without both (detached run build) that copy must be a plain external function: empty there, `inline` (unchanged code) everywhere else.
#ifdef NOTSA_NO_HOOK_REGISTRY
#define NOTSA_INLINE_IN_CPP
#else
#define NOTSA_INLINE_IN_CPP inline
#endif

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

//! 32-bit identity key of an object address (corona / rope / streak / crime / shadow / marker ids, srand seeds, bug-compatible "pointer passed as int").
//! x86: the address itself, code identical to `(uint32)p`. 64-bit: its low 32 bits - the keys are only compared / hashed, never converted back
//! to a pointer; objects of one pool differ in the low bits (pools are far smaller than 4 GiB). A macro on purpose: an (even always-inlined) function
//! template would still be emitted as an unreferenced COMDAT by cl and change the object file of every user (wine A/B, P-4c).
#ifdef NOTSA_STATEHASH
// RR-1 (.notes/RR_PLAN.md row 8): ASLR makes object addresses differ from run to run on every OS, and some of these keys seed the game's RNG (`srand(NOTSA_PTR_ID32(this) + days)`),
// so the state-hash harness replaces them with a stable id (pool tag + slot index; see portable/statehash.cpp). Only defined with -DGTASA_STATEHASH=ON.
namespace notsa::rr { uint32_t StableId(const void* p); }
#define NOTSA_PTR_ID32(p) ::notsa::rr::StableId(p)
#else
#define NOTSA_PTR_ID32(p) static_cast<uint32>(reinterpret_cast<uintptr_t>(p))
#endif

/*!
* @brief Used for static variable references
*
* @tparam T    The type of the variable
* @param Addr  The address of it
*/
#ifdef NOTSA_STANDALONE_RUN
namespace notsa::standalone::detail {
// Address mode only (the exe-oracle test TUs, NOTSA_ORACLE_ADDRESS_GLOBALS; the run build has no data image and StaticRef is a compile error there): the test defines it,
// true once the original data image is mapped at its original VA (tests/standalone/game_oracle.h). The dev-only GTASA_IMAGE_DIAG exe defines it in ImageLoad.cpp.
extern bool g_DataImageLoaded;
}
#endif

template<typename T>
T& StaticRef(uintptr addr) {
#if defined(NOTSA_DETACHED_GLOBALS) && !defined(NOTSA_ORACLE_ADDRESS_GLOBALS)
    // F1 (.notes/DETACH_DATA_PLAN.md): the detached run build has no original data image, so ANY instantiation is a compile error: declare the global with NOTSA_GLOBAL* (named storage)
    static_assert(sizeof(T) == 0, "StaticRef<T>(0xADDR) reads the original exe's data image: not allowed in the detached run build, use a NOTSA_GLOBAL* macro");
#endif
#line 194
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
 *                                                 `NOTSA_GLOBAL_LOCAL_NS(...)` when the old local was `auto& v = StaticRef<T>(a)` WITHOUT `static` (address mode keeps it non-static; detached: `static T v`)
 *    namespace scope of a header:                 `NOTSA_GLOBAL_HDR(g, 0xA, (int), {});`          (address: `static inline auto&`, detached: `inline T`)
 *                                                 `NOTSA_GLOBAL_HDR_EXT(...)` for a header variable declared `inline auto&` WITHOUT `static` (external linkage; address: `inline auto&`, so the object code stays identical)
 *    big/pointer-table initialisers: header `static NOTSA_GLOBAL_DECL(CCheat, name, 0xA, (T));` + the .cpp `NOTSA_GLOBAL_DEF(CCheat, name, 0xA, (T), { &f, ... });`
 *      (address mode: DECL = the old `static inline auto&`, DEF = nothing)
 *    header declaration of a namespace-scope .cpp global:   `NOTSA_GLOBAL_EXTERN(name, (T));`   (address: `extern T& name;`, detached: `extern T name;`)
 *    zero-image object whose repo ctor would seed non-zero defaults (the exe's static init leaves it all-zero), `ExeZero<T>()` without the temporaries:
 *                                                 `NOTSA_GLOBAL_ZERO(name, 0xA, (T));` (class scope / .cpp; NO specifiers) or `NOTSA_GLOBAL_ZERO_HDR(...)` (header namespace scope).
 *                                                 address: `static inline auto& name = StaticRef<T>(0xA)`; detached: constant-initialised zero storage + `T& name` (no dynamic initialiser, no ctor/dtor, usable for 500 KB arrays)
 *    global whose source INITIALISER must run in both modes (the image holds zeros, the exe fills it later; Maths.cpp SIN_LUT):
 *                                                 `NOTSA_GLOBAL_INIT(name, 0xA, (T), expr);`   (address: `const auto& name = StaticRef<T>(0xA) = expr;` (internal linkage, as the old text), detached: `T name = expr;` (external linkage:
 *                                                 other TUs name it with `NOTSA_GLOBAL_EXTERN` + `static NOTSA_GLOBAL_ALIAS(name, 0xA, (T), ::name);` - the alias text in address mode is the old StaticRef)
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
#define NOTSA_GLOBAL_LOCAL_NS(name, addr, type, ...) static std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__ // old local was a plain `auto&` (no `static`)
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
#define NOTSA_GLOBAL_INIT(name, addr, type, ...)  std::type_identity_t<NOTSA_UNPAREN type> name = __VA_ARGS__ NOTSA_GLOBAL_REG_(static inline const, NOTSA_GLOBAL_CAT(name, _gReg_), name, name, addr)
#define NOTSA_GLOBAL_EXTERN(name, type)           extern std::type_identity_t<NOTSA_UNPAREN type> name
#define NOTSA_GLOBAL_DEF(cls, name, addr, type, ...) \
    std::type_identity_t<NOTSA_UNPAREN type> cls::name __VA_ARGS__ NOTSA_GLOBAL_REG_(static inline const, NOTSA_GLOBAL_CAT(NOTSA_GLOBAL_CAT(name, _gRegDef_), __COUNTER__), cls::name, name, addr)
// Zero-image object: raw zero storage (constant-initialised, no dynamic initialiser, no ctor/dtor, no stack temporaries) viewed as T. For objects the exe leaves all-zero
// whose repo ctor / default member initialisers would seed non-zero ids (CStreamingInfo, CZoneInfo ...). The call site gives no specifiers (the macro says `static inline` / `inline`).
namespace notsa { template<typename T> struct alignas(T) ExeZeroStorage { unsigned char bytes[sizeof(T)]; }; }
#define NOTSA_GLOBAL_ZERO(name, addr, type) \
    static inline ::notsa::ExeZeroStorage<NOTSA_UNPAREN type> NOTSA_GLOBAL_CAT(name, _Zero_); \
    static inline std::type_identity_t<NOTSA_UNPAREN type>& name = reinterpret_cast<std::type_identity_t<NOTSA_UNPAREN type>&>(NOTSA_GLOBAL_CAT(name, _Zero_)) \
    NOTSA_GLOBAL_REG_(static inline const, NOTSA_GLOBAL_CAT(name, _gReg_), name, name, addr)
#define NOTSA_GLOBAL_ZERO_DECL(cls, name, addr, type) std::type_identity_t<NOTSA_UNPAREN type>& name
#define NOTSA_GLOBAL_ZERO_DEF(cls, name, addr, type) \
    static ::notsa::ExeZeroStorage<NOTSA_UNPAREN type> NOTSA_GLOBAL_CAT(name, _Zero_); \
    std::type_identity_t<NOTSA_UNPAREN type>& cls::name = reinterpret_cast<std::type_identity_t<NOTSA_UNPAREN type>&>(NOTSA_GLOBAL_CAT(name, _Zero_)); \
    NOTSA_GLOBAL_REG_(static inline const, NOTSA_GLOBAL_CAT(NOTSA_GLOBAL_CAT(name, _gRegDef_), __COUNTER__), cls::name, name, addr)
#define NOTSA_GLOBAL_ZERO_HDR(name, addr, type) \
    inline ::notsa::ExeZeroStorage<NOTSA_UNPAREN type> NOTSA_GLOBAL_CAT(name, _Zero_); \
    inline std::type_identity_t<NOTSA_UNPAREN type>& name = reinterpret_cast<std::type_identity_t<NOTSA_UNPAREN type>&>(NOTSA_GLOBAL_CAT(name, _Zero_)) \
    NOTSA_GLOBAL_REG_(inline const, NOTSA_GLOBAL_CAT(name, _gReg_), name, name, addr)
#else
namespace notsa { inline constexpr bool kGlobalsDetached = false; }
#define NOTSA_GLOBAL(name, addr, type, ...)       auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_HDR(name, addr, type, ...)   static inline auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_HDR_EXT(name, addr, type, ...) inline auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_LOCAL(name, addr, type, ...) static auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_LOCAL_NS(name, addr, type, ...) auto& name = StaticRef<NOTSA_UNPAREN type>(addr) // no `static`: a static reference needs a thread-safe init guard, which changes the object code
#define NOTSA_GLOBAL_LOCAL_REF(name, addr, type, obj) auto* name = reinterpret_cast<std::remove_extent_t<NOTSA_UNPAREN type>*>(addr)
#define NOTSA_GLOBAL_ALIAS(name, addr, type, ...) auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_EXPR(addr, type, ...) StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_SCOPED_GLOBAL(name, varAddr, flagsAddr, mask, type, ...) static auto& name = ScopedStaticRef<NOTSA_UNPAREN type>(varAddr, flagsAddr, mask, __VA_ARGS__)
#define NOTSA_GLOBAL_SYNTH(name, addr, type, ...) static_assert(true)
#define NOTSA_GLOBAL_VERIFY(addr, obj) static_assert(true)
#define NOTSA_GLOBAL_DECL(cls, name, addr, type)  inline auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_INIT(name, addr, type, ...)  const auto& name = StaticRef<NOTSA_UNPAREN type>(addr) = __VA_ARGS__
#define NOTSA_GLOBAL_EXTERN(name, type)           extern NOTSA_UNPAREN type& name
#define NOTSA_GLOBAL_DEF(cls, name, addr, type, ...) static_assert(true)
#define NOTSA_GLOBAL_ZERO(name, addr, type)       static inline auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_ZERO_DECL(cls, name, addr, type) inline auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
#define NOTSA_GLOBAL_ZERO_DEF(cls, name, addr, type) static_assert(true)
#define NOTSA_GLOBAL_ZERO_HDR(name, addr, type)   static inline auto& name = StaticRef<NOTSA_UNPAREN type>(addr)
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
