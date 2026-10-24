#pragma once

#include "Route.hpp"
#include "PathFind.h" // CNodeAddress

class CNodeRoute : public notsa::Route<CNodeAddress> {
public:
    static void InjectHooks();

    static void* operator new(size_t size);
    static void operator delete(void* ptr, size_t sz);
};
VALIDATE_SIZE(CNodeRoute, 0x24);
