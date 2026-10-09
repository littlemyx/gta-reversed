// P2B-04a: RpWorld* on top of librw (rw::World). librw's world is a stub (no sectors / BSP): lights (global + local lists), clumps and the
// camera->world / atomic->world back pointers. The game only uses the world as the owner of lights and the camera.
//   RpWorld{Create,Destroy,AddCamera,RemoveCamera,AddLight,RemoveLight} + RpWorld{AddAtomic,RemoveAtomic,AddClump,RemoveClump,
//   ForAllClumps,ForAllLights}.
// Verified against the exe: RpWorldAddLight 0x751910, RpWorldRemoveLight 0x751960, RpWorldAddCamera 0x750F20, RpWorldRemoveCamera 0x750F50.
// RpWorldPluginAttach is NOT a stub here: librw's Engine::init registers only the core modules (frame / image / raster / texture); the geometry
// "BinMesh" (0x50E) and "NativeData" (0x510) plugins and the rights-to-render plugins (0x1F) are registered by the application. Without the
// mesh plugin a streamed geometry has NO mesh header (and the native-data plugin's destructor is what frees the instanced D3D9 buffers), so
// RW's world attach (the first of the game's PluginAttach list) maps onto exactly these four librw registrations.
// Not here: RpWorldSetRenderOrder (librw has no render order).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

// W: see the file header. Idempotent (the shim's Engine::init cycle resets the plugin lists, a second attach in the same cycle is a no-op).
RwBool RpWorldPluginAttach() {
    if (rw::Geometry::s_plglist.getPluginOffset(rw::ID_MESH) < 0) {
        rw::registerMeshPlugin();
        rw::registerNativeDataPlugin();
        rw::registerAtomicRightsPlugin();
        rw::registerMaterialRightsPlugin();
    }
    return TRUE;
}

// A: the bounding box only matters for BSP sector building, which librw does not have.
RpWorld* RpWorldCreate(RwBBox* boundingBox) {
    return rw::World::create(boundingBox);
}

// A: lights / clumps still tied to the world are detached first (the RW world drops its lists on destroy; librw would leave dangling
// `world` pointers behind in them).
RwBool RpWorldDestroy(RpWorld* world) {
    if (!world) {
        return FALSE;
    }
    while (world->clumps.link.next != &world->clumps.link) {
        world->removeClump(rw::Clump::fromWorld(world->clumps.link.next));
    }
    while (world->globalLights.link.next != &world->globalLights.link) {
        world->removeLight(rw::Light::fromWorld(world->globalLights.link.next));
    }
    while (world->localLights.link.next != &world->localLights.link) {
        world->removeLight(rw::Light::fromWorld(world->localLights.link.next));
    }
    world->destroy();
    return TRUE;
}

// A: the exe stores the world pointer unconditionally (a camera already tied to another world is simply re-pointed); librw asserts on that.
RpWorld* RpWorldAddCamera(RpWorld* world, RwCamera* camera) {
    if (!world || !camera) {
        return nullptr;
    }
    camera->world = nullptr;
    world->addCamera(camera);
    return world;
}

// A: the exe clears the camera's world pointer whatever world it names and returns NULL when it was not tied to any world.
RpWorld* RpWorldRemoveCamera(RpWorld* world, RwCamera* camera) {
    if (!world || !camera || !camera->world) {
        return nullptr;
    }
    camera->world = nullptr;
    return world;
}

// W: global lights (type < 0x80: ambient / directional) go to the global list, positioned lights to the local list, both at the HEAD like
// the exe (librw appends); a light that is already in a world is moved (the exe would corrupt both lists).
RpWorld* RpWorldAddLight(RpWorld* world, RpLight* light) {
    if (!world || !light) {
        return nullptr;
    }
    if (light->world == world) {
        return world;
    }
    if (light->world) {
        light->world->removeLight(light);
    }
    // The exe (0x751910) links the light at the FRONT of its list (rwLinkListAddLLLink); librw's World::addLight appends. The order is
    // observable: ForAllLights (0x74FC00) and the D3D9 global-light setup walk the list from the head.
    light->world = world;
    if (light->getType() < rw::Light::POINT) {
        world->globalLights.add(&light->inWorld);
    } else {
        world->localLights.add(&light->inWorld);
        if (light->getFrame()) {
            light->getFrame()->updateObjects();
        }
    }
    return world;
}

// A: a light that is not in `world` is left alone (librw asserts).
RpWorld* RpWorldRemoveLight(RpWorld* world, RpLight* light) {
    if (!world || !light || light->world != world) {
        return nullptr;
    }
    world->removeLight(light);
    return world;
}

// ---- 04ab extras (declared in rwextra.h) ----

RpWorld* RpWorldAddAtomic(RpWorld* world, RpAtomic* atomic) {
    if (!world || !atomic) {
        return nullptr;
    }
    if (atomic->world) {
        atomic->world = nullptr;
    }
    world->addAtomic(atomic);
    return world;
}

RpWorld* RpWorldRemoveAtomic(RpWorld* world, RpAtomic* atomic) {
    if (!world || !atomic || atomic->world != world) {
        return nullptr;
    }
    world->removeAtomic(atomic);
    return world;
}

RpWorld* RpWorldAddClump(RpWorld* world, RpClump* clump) {
    if (!world || !clump || clump->world) {
        return nullptr;
    }
    world->addClump(clump);
    return world;
}

RpWorld* RpWorldRemoveClump(RpWorld* world, RpClump* clump) {
    if (!world || !clump || clump->world != world) {
        return nullptr;
    }
    world->removeClump(clump);
    return world;
}

// A: RW stops at the first callback that returns NULL; the next link is read before the call so the callback may remove the clump.
RpWorld* RpWorldForAllClumps(RpWorld* world, RpClumpCallBack callback, void* data) {
    if (!world || !callback) {
        return world;
    }
    for (rw::LLLink* l = world->clumps.link.next; l != &world->clumps.link;) {
        rw::LLLink* next = l->next;
        if (!callback(rw::Clump::fromWorld(l), data)) {
            break;
        }
        l = next;
    }
    return world;
}

// A: global lights, then local lights (RW walks the sector lists; the order is not observable).
RpWorld* RpWorldForAllLights(RpWorld* world, RpLightCallBack callback, void* data) {
    if (!world || !callback) {
        return world;
    }
    rw::LinkList* lists[2] = {&world->globalLights, &world->localLights};
    for (rw::LinkList* list : lists) {
        for (rw::LLLink* l = list->link.next; l != &list->link;) {
            rw::LLLink* next = l->next;
            if (!callback(rw::Light::fromWorld(l), data)) {
                return world;
            }
            l = next;
        }
    }
    return world;
}
#endif
