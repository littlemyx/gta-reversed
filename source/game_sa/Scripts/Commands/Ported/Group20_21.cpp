#include <StdInc.h>

#include "../Commands.hpp"
#include "Group20_21.hpp"

//! S6-F: central registration of the script commands ported from the exe's group processors g20..g21 (ids 2000..2199)
void notsa::script::commands::ported::g20_21::RegisterHandlers() {
    RegisterG20a();
    RegisterG20b();
    RegisterG21a();
    RegisterG21b();
}
