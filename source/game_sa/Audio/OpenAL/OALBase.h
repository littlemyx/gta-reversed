#pragma once

#include <AL/al.h>

// Non-owning list of heap allocated OpenAL wrapper objects (they own themselves through the refcount + `OALBase::trashCan`)
template<typename T>
using HeapPtrArray = std::vector<T*>;

// This class has to be heap allocated! (with `new`)
class OALBase {
public:
    // Number of living `OALBase` object instances.
    static inline uint32 livingCount{};

    // To be garbage collected OpenAL objects.
    static inline std::vector<std::unique_ptr<OALBase>> trashCan{};

    uint32 m_refCount;

public:
    // NOTSA: possibly inlined in SA.
    OALBase();

    virtual ~OALBase();
    virtual void Release();

    // NOTSA
    virtual void AddRef();
};

bool OALCheckErrors(std::string_view file, int32 line);
#define OAL_CHECKED(c) (c, OALCheckErrors(__FILE__, __LINE__))
