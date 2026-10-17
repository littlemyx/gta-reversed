#pragma once
#include <source_location>
#include <string>
#include "KeyGen.h"


#if defined(NOTSA_STANDALONE_RUN) && defined(FIX_BUGS)
#error "The standalone run build reproduces the original exe: FIX_BUGS must not be defined (see tools/standalone/standalone_deviations.tsv, .notes/reports/FIX-AUDIT.md)"
#endif

namespace notsa::bugfixes {
namespace detail {
// Default state of a bugfix. The standalone run build (NOTSA_STANDALONE_RUN) reproduces the ORIGINAL exe, bugs included, so every fix is OFF there unless its
// definition in Bugs.hpp says `.Enabled = true` (a fix the port needs in order to run at all; each one is listed in tools/standalone/standalone_deviations.tsv and
// docs/STANDALONE_FIDELITY.md). The DLL/ASI build and the hook dump keep the upstream default (ON).
#ifdef NOTSA_STANDALONE_RUN
inline constexpr bool kDefaultEnabled = false;
#else
inline constexpr bool kDefaultEnabled = true;
#endif
} // namespace detail

/*!
* This class is *not* meant to be constructed on the stack.
* Only place it should be defined at is `Bugs.hpp`.
*/
struct ReversibleBugFix {
    static inline ReversibleBugFix* Tail{};

    constexpr operator bool() const { return Enabled; }

public:
    const char*       Name{};                            //!< User-defined name
    const char*       Description{};                     //!< User defined description
    const char*       Credit{};                          //!< Credits for this bugfix
    bool              Enabled{ detail::kDefaultEnabled };//!< Is this bugfix enabled? (If not, the bug is present)  [run build: off by default, see `detail::kDefaultEnabled`]
    bool              Locked{ false };                   //!< Can this bugfix's enabled state be changed?
    bool              RequiresRestart{ false };          //!< Does this bug require a restart for it to take effect? 
    ReversibleBugFix* Prev{ std::exchange(Tail, this) }; //!< Linked list of bugfixes (Maintained for the debug tool)
};
};
