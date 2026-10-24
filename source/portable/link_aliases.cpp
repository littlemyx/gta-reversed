// MAC-BUILD1: Itanium C++ ABI complete-object constructor (C1) aliases for ABSTRACT classes.
//
// The reversed sources contain hook-helper functions of the form `T* T::Constructor(args) { this->T::T(args); return this; }` (MSVC extension: explicit constructor
// call). For a class with a pure virtual function clang emits only the base-object constructor (C2): no object of an abstract class is ever complete, so C1 is
// never needed by the language. The explicit call above does reference C1, hence an undefined symbol at link time. Without virtual bases C1 and C2 are the same
// code, so the missing C1 is a plain tail branch to C2 (this is what clang itself emits as an alias for concrete classes).
// These are NOT stubs: the behaviour is exactly the constructor's. Only compiled for the portable (arm64 Mach-O) build; the Windows builds never see this file.
#if (defined(__aarch64__) && defined(__APPLE__)) || (defined(__x86_64__) && defined(__linux__))
// (Linux x86_64 ELF: same reason, found by the first PortableLinux link, MAC-LNX1; symbols have one underscore less than Mach-O and the tail call goes through the PLT)

#if defined(__APPLE__)
#define NOTSA_C1_ALIAS(MANGLED_STEM, ARGS) \
    __asm__(".text\n.p2align 2\n.globl __ZN" MANGLED_STEM "C1" ARGS "\n__ZN" MANGLED_STEM "C1" ARGS ":\n    b __ZN" MANGLED_STEM "C2" ARGS "\n")
#else
#define NOTSA_C1_ALIAS(MANGLED_STEM, ARGS) \
    __asm__(".text\n.p2align 4\n.globl _ZN" MANGLED_STEM "C1" ARGS "\n.type _ZN" MANGLED_STEM "C1" ARGS ",@function\n_ZN" MANGLED_STEM "C1" ARGS ":\n    jmp _ZN" MANGLED_STEM "C2" ARGS "@PLT\n")
#endif

NOTSA_C1_ALIAS("6CEvent", "Ev");                                                    // CEvent::CEvent()
NOTSA_C1_ALIAS("21CEventAcquaintancePed", "EP4CPed9eTaskType");                      // CEventAcquaintancePed::CEventAcquaintancePed(CPed*, eTaskType)
NOTSA_C1_ALIAS("22CEventEditableResponse", "E9eTaskType");                           // CEventEditableResponse::CEventEditableResponse(eTaskType)
NOTSA_C1_ALIAS("19CEventVehicleDamage", "EP8CVehicleP7CEntity11eWeaponType");        // CEventVehicleDamage::CEventVehicleDamage(CVehicle*, CEntity*, eWeaponType)
NOTSA_C1_ALIAS("19CTaskComplexPartner", "EPKcP4CPedbfba7CVector");                   // CTaskComplexPartner::CTaskComplexPartner(const char*, CPed*, bool, float, bool, signed char, CVector)
NOTSA_C1_ALIAS("18CTaskComplexWander", "E10eMoveStatehbf");                          // CTaskComplexWander::CTaskComplexWander(eMoveState, uint8, bool, float)
NOTSA_C1_ALIAS("19CAEStreamingDecoder", "EP13CAEDataStream");                        // CAEStreamingDecoder::CAEStreamingDecoder(CAEDataStream*)

#endif
