#include <StdInc.h>

#include "../Commands.hpp"
#include "Group25_26.hpp"

//! S6-J: central registration of the script commands ported from the exe's group processors g25..g26 (ids 2500..2699)
void notsa::script::commands::ported::g25_26::RegisterHandlers() {
    RegisterG25a();
    RegisterG25b();
    RegisterG26();
}
