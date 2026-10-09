#include <StdInc.h>

#include "../Commands.hpp"
#include "Group22.hpp"

//! S6-G: central registration of the script commands ported from the exe's group processor g22 (ids 2200..2299)
void notsa::script::commands::ported::g22::RegisterHandlers() {
    RegisterG22a();
    RegisterG22b();
}
