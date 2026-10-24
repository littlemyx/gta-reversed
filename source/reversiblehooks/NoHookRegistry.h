#pragma once

// Slice F2 of .notes/DETACH_DATA_PLAN.md (Phase F): the detached run build has no hook registry.
//
// NOTSA_NO_HOOK_REGISTRY (derived, never set by hand) = every standalone RUN translation unit: the run exe AND the exe-oracle test TUs (F3: the run build is always
// detached, there is no longer a run build that patches an original data image; the oracle tests only share the game's SOURCE with it, they never read the registry:
// their Fixups::Register* stubs were empty).
//
// In this configuration every object in our globals already points at our own functions / vtables (real C++ objects, constructors, `&fn` initialisers: verified
// by tools/standalone/verify_vptrs.py and verify_globals.py), so nothing needs the (exe address -> our function) pairs any more:
//  * the RH_Scoped* macros (ReversibleHooks.h) expand to nothing: no exe address, no RHManager item, no VMTInfo exe/export read
//  * NOTSA_EXPORT_VTABLE is empty (Base.h): the exported vtables were only read by VMTInfo::FindByClassName
// The DLL/ASI build and the hook-dump build are not affected (NOTSA_STANDALONE_RUN is not defined there: identical code).
#ifdef NOTSA_STANDALONE_RUN
#ifndef NOTSA_NO_HOOK_REGISTRY
#define NOTSA_NO_HOOK_REGISTRY 1
#endif
#endif

// Portable build (GTASA_PORTABLE, .notes/STREAM_C_PLAN.md slice A-1): the same, whatever else is defined. ReversibleHooks.h then includes PortableStub.h instead of the registry.
#ifdef NOTSA_PORTABLE
#ifndef NOTSA_NO_HOOK_REGISTRY
#define NOTSA_NO_HOOK_REGISTRY 1
#endif
#endif
