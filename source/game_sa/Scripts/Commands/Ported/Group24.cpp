#include <StdInc.h>

#include "../Commands.hpp"
#include "Group24.hpp"

//! S6-I: central registration of the script commands ported from the exe's group processor g24 (ids 2400..2499)
void notsa::script::commands::ported::g24::RegisterHandlers() {
    RegisterG24a();
    RegisterG24b();
}
