// P2B-03b: RwTexDictionary* on top of librw (rw::TexDictionary), the dictionary half of the texture module.
//   Game-called (rwapi.h): RwTexDictionaryCreate, Destroy, AddTexture, RemoveTexture, FindNamedTexture, ForAllTextures, GetCurrent, SetCurrent,
//   RegisterPlugin, StreamWrite.  GetFirstTexture (rwtexdict.h, SA's RW patch) lives here too.
//   Declared in rwextra.h (03b block): RwTexDictionaryRegisterPluginStream, RwTexDictionaryStreamRead, RwTexDictionaryStreamGetSize.
// Function classes (see .notes/P2B_SHIM_PLAN.md): D = direct, A = adapter, W = written here because librw's version differs from the exe.
//
// Checked against the exe (RW 3.6): Create 0x7F3600, Destroy 0x7F36A0, ForAllTextures 0x7F3730, AddTexture 0x7F3980, RemoveTexture 0x7F39C0,
// FindNamedTexture 0x7F39F0, SetCurrent 0x7F3A70, GetCurrent 0x7F3A90, GetFirstTexture 0x734940 (+ callback 0x734930), StreamRead 0x804C30:
//   * The texture list is a RW LinkList and AddTexture inserts at the FRONT (rwLinkListAddLLLink): iteration order (ForAllTextures, GetFirstTexture,
//     the game's rwLLLinkGetNext walks) is newest first. librw's TexDictionary::add APPENDS, so every insertion here uses addFront. A stream read
//     therefore yields the textures in reverse file order, as the exe does.
//   * AddTexture first unlinks the texture from its old dictionary. RemoveTexture unlinks and clears texture->dict (a texture that is in no dictionary is
//     returned unchanged). Both return the texture.
//   * FindNamedTexture: linear walk, ASCII case-insensitive full-string compare (a-z folded to A-Z, stops at the first NUL of either string, both must
//     end together). NO 32-char limit and no hashing. (librw's find uses strncmp_ci(.., 32): same result for NUL terminated names.)
//   * ForAllTextures: next link is read before the callback runs (the callback may destroy / remove the texture); a NULL return stops the walk;
//     returns the dictionary in both cases. SetCurrent returns the dictionary, GetCurrent the stored pointer (NULL when nothing is current).
//   * Destroy: clears the current dictionary if it is this one, then RwTextureDestroy for every texture (a texture with other references survives:
//     the exe leaves it linked to the freed dictionary, librw unlinks first and clears texture->dict, which is what is used here), plugin
//     destructors, unlink from the engine's dictionary list, free.
//   * StreamRead: STRUCT {u16 numTextures, u16 deviceId} -> create dictionary -> per texture chunk 0x15 the D3D9 native reader (texture.cpp: PAL8
//     expansion), then the texture plugin data, AddTexture (front) -> dictionary plugin data. Any failure destroys all textures read so far and the
//     dictionary and returns NULL. The device id is not checked (SA files carry 0 or 9). The shim does NOT set the dictionary as current.
// Only needs fakerw + librw + the CRT (also built by the PCH-less unit test tests/standalone/rw_texture_test.cpp). Excluded from unity builds.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#include <cassert>

namespace rwshim { RwTexture* ReadNativeTexture(rw::Stream* stream); } // texture.cpp

namespace {
// The exe's compare: fold a-z to A-Z, walk until a NUL, both strings must be at their end together.
bool NameEqualsCI(const char* a, const char* b) {
    for (;;) {
        unsigned char ca = static_cast<unsigned char>(*a), cb = static_cast<unsigned char>(*b);
        if (!ca || !cb) {
            return ca == cb;
        }
        if (ca >= 'a' && ca <= 'z') ca -= 0x20;
        if (cb >= 'a' && cb <= 'z') cb -= 0x20;
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
}
} // namespace

// D: TexDictionary::create (engine list registration + plugin constructors).
RwTexDictionary* RwTexDictionaryCreate(void) {
    return rw::TexDictionary::create();
}

// D: see header (clears current, destroys the textures, plugin destructors, unlink, free).
RwBool RwTexDictionaryDestroy(RwTexDictionary* dict) {
    dict->destroy();
    return TRUE;
}

// A: front insertion (TexDictionary::addFront also unlinks from the previous dictionary).
RwTexture* RwTexDictionaryAddTexture(RwTexDictionary* dict, RwTexture* texture) {
    dict->addFront(texture);
    return texture;
}

// W: unlink + clear texture->dict.
RwTexture* RwTexDictionaryRemoveTexture(RwTexture* texture) {
    if (texture->dict) {
        texture->inDict.remove();
        texture->dict = nullptr;
    }
    return texture;
}

// W: the exe's compare.
RwTexture* RwTexDictionaryFindNamedTexture(RwTexDictionary* dict, const RwChar* name) {
    if (!dict || !name) {
        return nullptr;
    }
    rw::LLLink* end = &dict->textures.link;
    for (rw::LLLink* l = end->next; l != end; l = l->next) {
        RwTexture* tex = rw::Texture::fromDict(l);
        if (NameEqualsCI(tex->name, name)) {
            return tex;
        }
    }
    return nullptr;
}

// A: iterate the list (see header).
const RwTexDictionary* RwTexDictionaryForAllTextures(const RwTexDictionary* dict, RwTextureCallBack fpCallBack, void* pData) {
    rw::LLLink* end = const_cast<rw::LLLink*>(&dict->textures.link);
    for (rw::LLLink* l = end->next; l != end;) {
        rw::LLLink* next = l->next;
        if (!fpCallBack(rw::Texture::fromDict(l), pData)) {
            break;
        }
        l = next;
    }
    return dict;
}

RwTexDictionary* RwTexDictionaryGetCurrent(void) {
    return rw::TexDictionary::getCurrent();
}

RwTexDictionary* RwTexDictionarySetCurrent(RwTexDictionary* dict) {
    rw::TexDictionary::setCurrent(dict);
    return dict;
}

// SA's RW patch (rwtexdict.h, exe 0x734940): the first texture ForAllTextures visits = the front of the list; NULL for an empty dictionary.
RwTexture* GetFirstTexture(RwTexDictionary* txd) {
    rw::LLLink* end = &txd->textures.link;
    return end->next != end ? rw::Texture::fromDict(end->next) : nullptr;
}

// A: Frame-style thunks: RW's callback types differ from librw's only by const, same calling convention and layout.
// Registration changes sizeof(dict): none may exist yet apart from the engine's initial one, which Engine::open creates BEFORE the game's
// PluginAttach list runs only if the order in the game is wrong -> asserted (CTxdStore::PluginAttach registers right after RwEngineInit).
RwInt32 RwTexDictionaryRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    assert(rw::TexDictionary::numAllocated == 0 && "RwTexDictionaryRegisterPlugin after a dictionary was created");
    return rw::TexDictionary::registerPlugin(size, pluginID, reinterpret_cast<rw::Constructor>(constructCB), reinterpret_cast<rw::Destructor>(destructCB),
                                             reinterpret_cast<rw::CopyConstructor>(copyCB));
}

RwInt32 RwTexDictionaryRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return rw::TexDictionary::registerPluginStream(pluginID, reinterpret_cast<rw::StreamRead>(readCB), reinterpret_cast<rw::StreamWrite>(writeCB),
                                                   reinterpret_cast<rw::StreamGetSize>(getSizeCB));
}

// W: see header.
RwTexDictionary* RwTexDictionaryStreamRead(RwStream* stream) {
    if (!rw::findChunk(stream, rw::ID_STRUCT, nullptr, nullptr)) {
        return nullptr;
    }
    int32_t numTex = stream->readI16() & 0xFFFF;
    stream->readI16(); // device id
    RwTexDictionary* dict = rw::TexDictionary::create();
    if (!dict) {
        return nullptr;
    }
    for (int32_t i = 0; i < numTex; ++i) {
        RwTexture* tex = nullptr;
        if (!rw::findChunk(stream, rw::ID_TEXTURENATIVE, nullptr, nullptr) || !(tex = rwshim::ReadNativeTexture(stream))) {
            dict->destroy();
            return nullptr;
        }
        if (!rw::Texture::s_plglist.streamRead(stream, tex)) {
            tex->destroy();
            dict->destroy();
            return nullptr;
        }
        dict->addFront(tex);
    }
    if (!rw::TexDictionary::s_plglist.streamRead(stream, dict)) {
        dict->destroy();
        return nullptr;
    }
    return dict;
}

// D: texdict chunk 0x16 (struct count + native textures + plugin data).
const RwTexDictionary* RwTexDictionaryStreamWrite(const RwTexDictionary* texDict, RwStream* stream) {
    const_cast<RwTexDictionary*>(texDict)->streamWrite(stream);
    return texDict;
}

RwUInt32 RwTexDictionaryStreamGetSize(const RwTexDictionary* texDict) {
    return const_cast<RwTexDictionary*>(texDict)->streamGetSize();
}
#endif
