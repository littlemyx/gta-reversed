#pragma once

#include "Group09_12.hpp"

/*!
* Helpers shared by the S6-I script command ports (Group24*.cpp): ids 2400..2499
*/
namespace notsa::script::commands::ported::g24 {
void RegisterHandlers(); // (also declared in Commands.hpp)
void RegisterG24a(); // ids 2400..2446
void RegisterG24b(); // ids 2449..2499
} // namespace notsa::script::commands::ported::g24
