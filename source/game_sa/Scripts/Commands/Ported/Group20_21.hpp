#pragma once

#include "Group09_12.hpp"

/*!
* Helpers shared by the S6-F script command ports (Group20*.cpp, Group21*.cpp): ids 2000..2199
* (generic helpers like GroundZIfAuto / SortPair / TransformVectorOriginal live in Group09_12.hpp)
*/
namespace notsa::script::commands::ported::g20_21 {
void RegisterHandlers(); // (also declared in Commands.hpp)
void RegisterG20a(); // ids 2000..2043
void RegisterG20b(); // ids 2045..2086
void RegisterG21a(); // ids 2087..2148
void RegisterG21b(); // ids 2150..2199
} // namespace notsa::script::commands::ported::g20_21
