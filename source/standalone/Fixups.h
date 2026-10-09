#pragma once

// Standalone build: "hooks as fixups". See .notes/P2A_DESIGN.md
//
// There is no original code to patch. Every `RH_Scoped*Install(fn, 0xEXEADDR)` instead registers (exe address -> our function) here.
// After the original data image is loaded, `ApplyToDataImage()` rewrites every code pointer in the image (vtables, callback tables)
// with our function, or with a trap stub if we do not have one (so leftovers fail loudly with the exe address).
// This header is intentionally dependency free: it is included from Base-level headers (PluginBase.h, RHManager.h).

#ifdef NOTSA_STANDALONE_RUN
#include <cstddef>
#include <cstdint>

namespace notsa::standalone {
struct FixupStats {
    size_t RegisteredFunctions{}; // distinct exe function addresses with a replacement
    size_t RegisteredVMTSlots{};  // distinct (vtable, slot) replacements
    size_t Conflicts{};           // same exe address registered twice with a different target (first one wins)
    size_t CodePointersV{};       // data-image dwords classified as vtable/callback-table entries (run >= 2), by the extractor
    size_t CodePointersC{};       // isolated code pointers
    size_t TextLikeIgnored{};     // code-range dwords the extractor rejected as text (not rewritten)
    size_t UnalignedIgnored{};    // ... as unaligned / u16 pairs (not rewritten)
    size_t ChangedDwords{};       // dwords of the image that differ from the loaded original after ApplyToDataImage (self-check: == fixed + trapped)
    size_t FixedBySlot{};
    size_t FixedByFunction{};
    size_t TrappedV{};            // unknown + replaced by a trap stub
    size_t TrappedC{};
};

namespace Fixups {
//! Register a replacement for the exe function at `exeAddr`
void RegisterFunction(uint32_t exeAddr, void* ours, const char* name);

//! Register a replacement for one vtable slot of the original exe: vtable at `vtblAddr`, slot index `slot`
//! (`exeFn` is the exe function that slot originally holds: it is also used as a fallback for inherited slots in other vtables)
void RegisterVMTSlot(uint32_t vtblAddr, size_t slot, uint32_t exeFn, void* ours, const char* name);

//! Replacement for `exeAddr` or nullptr
void* FindKnown(uint32_t exeAddr);

//! Executable stub `push exeAddr; call TrapHandler` for `exeAddr` (created on demand, never null)
void* GetTrapStub(uint32_t exeAddr);

//! `FindKnown(exeAddr)`, else `GetTrapStub(exeAddr)`
void* Resolve(uint32_t exeAddr);

//! Scan the loaded data image, replace known code pointers, trap the unknown ones. Call once, after ALL hooks were registered.
FixupStats ApplyToDataImage();

//! Vectored handler: a raw jump/call into the (unmapped) original code range (`((T(*)(...))0xADDR)(...)` casts) is redirected
//! to our function if it is known, otherwise it logs the exe address and aborts.
void InstallRedirectHandler();

//! Log (to `standalone.log` next to the exe, the debugger and stderr) and terminate the process
[[noreturn]] void Fatal(const char* fmt, ...);
//! Append a line to the log
void Log(const char* fmt, ...);
} // namespace Fixups

//! Used by `plugin::Call*<addr>`
inline void* ResolveCallTarget(uint32_t exeAddr) { return Fixups::Resolve(exeAddr); }
} // namespace notsa::standalone
#endif
