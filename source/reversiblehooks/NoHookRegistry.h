#pragma once

// Slice F2 of .notes/DETACH_DATA_PLAN.md (Phase F): the detached run build has no hook registry.
//
// NOTSA_NO_HOOK_REGISTRY (derived, never set by hand) = standalone RUN build with detached globals (CMake GTASA_DETACHED_GLOBALS=ON) and not an oracle test
// translation unit (those add NOTSA_ORACLE_ADDRESS_GLOBALS and keep the full hook machinery, they run next to the real original code).
//
// In this configuration every object in our globals already points at our own functions / vtables (real C++ objects, constructors, `&fn` initialisers: verified
// by tools/standalone/verify_vptrs.py and verify_globals.py), so nothing needs the (exe address -> our function) pairs any more:
//  * the RH_Scoped* macros (ReversibleHooks.h) expand to nothing: no exe address, no RHManager item, no Fixups::Register* call, no VMTInfo exe/export read
//  * Fixups::Register* / RegisterVMTClass / RegisterUnverified do not exist (Fixups.h), ApplyToDataImage / RedirectVEH never find a known replacement
//  * NOTSA_EXPORT_VTABLE is empty (Base.h): the exported vtables were only read by VMTInfo::FindByClassName
// The DLL/ASI build, the hook-dump build and the oracle tests are not affected (macro not defined: identical code).
#if defined(NOTSA_STANDALONE_RUN) && defined(NOTSA_DETACHED_GLOBALS) && !defined(NOTSA_ORACLE_ADDRESS_GLOBALS)
#ifndef NOTSA_NO_HOOK_REGISTRY
#define NOTSA_NO_HOOK_REGISTRY 1
#endif
#endif
