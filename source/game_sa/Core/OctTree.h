/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Pool.h"

typedef CPool<class COctTree> COctTreePool;

class NOTSA_EXPORT_VTABLE COctTree {
public:
    static void InjectHooks();
    uint32 m_nLevel;
    bool   m_bLastStep;       // no children
    int16  m_aChildrens[8];   // pool slot IDs,  -1 - empty
    uint32 m_nRedComponent;
    uint32 m_nGreenComponent;
    uint32 m_nBlueComponent;

public:
    static inline NOTSA_GLOBAL(ms_bFailed, 0xBC12DC, (bool), {});
    static inline NOTSA_GLOBAL(ms_level, 0xBC12E0, (uint32), {});
#ifdef NOTSA_GLOBALS_DETACHED
    // The image holds 20 zero bytes: no CPool NSDMI (m_LastFreeSlot{-1}) and no ~CPool() (Flush() at exit) in the exe -> raw zeroed storage viewed as the pool
    static inline NOTSA_GLOBAL(ms_octTreePoolRaw, 0xBC12E4, (std::array<uint32, 5>), {});
    static COctTreePool& ms_octTreePool; // defined after the class (COctTree must be complete)
#else
    static inline auto& ms_octTreePool = StaticRef<COctTreePool>(0xBC12E4);
#endif
#line 27

public:
    COctTree();
    ~COctTree();

    static void* operator new(size_t size);
    static void  operator delete(void* data);

    virtual bool InsertTree(uint8 red, uint8 green, uint8 blue);
    virtual void FillPalette(uint8* colors);

    static void InitPool(void* data, int32 dataSize);
    static void ShutdownPool();
    uint32      FindNearestColour(uint8 red, uint8 green, uint8 blue);
    uint32      NoOfChildren();
    void        ReduceTree();
    void        RemoveChildren();
    void        empty();
};
VALIDATE_SIZE(COctTree, 0x28);
#ifdef NOTSA_GLOBALS_DETACHED
inline COctTreePool& COctTree::ms_octTreePool = *reinterpret_cast<COctTreePool*>(&COctTree::ms_octTreePoolRaw);
#endif
#line 47

NOTSA_GLOBAL_EXTERN(gpTmpOctTree, (COctTree*));
