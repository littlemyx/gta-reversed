// P2B-04c: RpAtomic* on top of librw (rw::Atomic).
//   RpAtomic{Create,Destroy,Clone,SetGeometry,SetFrame,GetWorldBoundingSphere}, _rpAtomicResyncInterpolatedSphere.
//   RpAtomicGet{Geometry,Frame,Clump,Flags,BoundingSphere} / SetFlags are macros in fakerw (rwaccessors.h). Render callback + plugin thunks: plugins.cpp. The interpolator is constant "none" (00c decision: RwCompatAtomicSphereDirty
//   is false), so the morph-target interpolation of the exe collapses to morph target 0.
// Verified against the exe: AtomicCreate 0x749C50, SetGeometry 0x749D40, Destroy 0x749DC0, Clone 0x749E60, _rpAtomicResyncInterpolatedSphere
// 0x7491F0, RpAtomicGetWorldBoundingSphere 0x749330, SetFrame 0x74BF20.
// Exe facts that matter: RpAtomicDestroy does NOT unlink the atomic from a clump/world (the game never destroys an attached atomic); here
// the atomic is unlinked first (leniency, harmless). RpAtomicSetGeometry with the geometry it already has is a complete no-op.
// The world bounding sphere keeps RW's scaling rule: radius * sqrt(max(|right|^2, |up|^2, |at|^2)) of the frame's LTM unless the LTM is
// orthonormal (matrix type bits == 3); sqrt is the exe's table sqrt (rwx::WorldSphereRadius, call at 0x749445).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include "rwmath_exact.h"

#include <algorithm>
#include <cassert>
#include <cmath>

// D: Atomic::create (flags COLLISIONTEST|RENDER, default render callback, no geometry). The render-callback slot is registered on first use.
RpAtomic* RpAtomicCreate() {
    RwShimEnsureAtomicRenderSlot();
    return rw::Atomic::create();
}

// W: frees like the exe -- plugin destructors first, then the geometry reference (through RpGeometryDestroy so the shim's per-geometry side
// table is cleaned), then the frame link. librw's Atomic::destroy would release the geometry with rw::Geometry::destroy directly.
RwBool RpAtomicDestroy(RpAtomic* atomic) {
    if (!atomic) {
        return FALSE;
    }
    if (atomic->clump) {
        atomic->clump->removeAtomic(atomic);
    }
    if (atomic->world) {
        atomic->world->removeAtomic(atomic);
    }
    rw::Atomic::s_plglist.destruct(atomic);
    if (atomic->geometry) {
        RpGeometry* const geometry = atomic->geometry;
        atomic->geometry           = nullptr;
        RpGeometryDestroy(geometry);
    }
    atomic->setFrame(nullptr);
    rwFree(atomic);
    rw::Atomic::numAllocated--;
    return TRUE;
}

// A: Atomic::clone copies flags, geometry (+1 ref), render callback, pipeline and runs the plugin copy callbacks; the exe also copies the
// bounding sphere verbatim (librw re-derives it from morph target 0, which differs after RpGeometry bounds edits).
RpAtomic* RpAtomicClone(RpAtomic* atomic) {
    if (!atomic) {
        return nullptr;
    }
    RwShimEnsureAtomicRenderSlot();
    RpAtomic* const clone = atomic->clone();
    if (clone) {
        clone->boundingSphere = atomic->boundingSphere;
        clone->object.object.privateFlags |= rw::Atomic::WORLDBOUNDDIRTY;
    }
    return clone;
}

// A: exe 0x749D40 order: same geometry -> nothing; +1 on the new one BEFORE releasing the old (librw releases first); the bounding sphere is
// taken from morph target 0 unless rpATOMICSAMEBOUNDINGSPHERE; the frame's objects are re-synced.
RpAtomic* RpAtomicSetGeometry(RpAtomic* atomic, RpGeometry* geometry, RwUInt32 flags) {
    if (!atomic || geometry == atomic->geometry) {
        return atomic;
    }
    if (geometry) {
        geometry->addRef();
    }
    if (atomic->geometry) {
        RpGeometryDestroy(atomic->geometry);
    }
    atomic->geometry = geometry;
    if (!(flags & rw::Atomic::SAMEBOUNDINGSPHERE)) {
        if (geometry) {
            atomic->boundingSphere = geometry->morphTargets[0].boundingSphere;
        }
        if (atomic->getFrame()) {
            atomic->getFrame()->updateObjects();
        }
    }
    atomic->object.object.privateFlags |= rw::Atomic::WORLDBOUNDDIRTY;
    return atomic;
}

// A: exe 0x74BF20: attach (or detach with NULL) and mark the world sphere dirty; returns the atomic.
RpAtomic* RpAtomicSetFrame(RpAtomic* atomic, RwFrame* frame) {
    if (!atomic) {
        return nullptr;
    }
    atomic->setFrame(frame); // Atomic::setFrame also sets WORLDBOUNDDIRTY
    return atomic;
}

// A: exe 0x7491F0 with the interpolator fixed at (start 0, end 0): the sphere is morph target 0's.
void _rpAtomicResyncInterpolatedSphere(RpAtomic* atomic) {
    if (atomic && atomic->geometry) {
        atomic->boundingSphere = atomic->geometry->morphTargets[0].boundingSphere;
        atomic->object.object.privateFlags |= rw::Atomic::WORLDBOUNDDIRTY;
    }
}

// W: exe 0x749330 (see the file header for the radius rule).
const RwSphere* RpAtomicGetWorldBoundingSphere(RpAtomic* atomic) {
    if (!atomic) {
        return nullptr;
    }
    RwFrame* const frame = atomic->getFrame();
    if (!frame) {
        return &atomic->worldBoundingSphere;
    }
    if (frame->dirty() || (atomic->object.object.privateFlags & rw::Atomic::WORLDBOUNDDIRTY)) {
        rw::Matrix* const ltm = frame->getLTM();
        rwx::TransformPoint(&atomic->worldBoundingSphere.center, &atomic->boundingSphere.center, ltm);   // 0x7EDD60 (exe x87 order)
        atomic->worldBoundingSphere.radius = rwx::WorldSphereRadius(ltm, atomic->boundingSphere.radius);   // 01r: table sqrt, exe compare order
        atomic->object.object.privateFlags &= ~rw::Atomic::WORLDBOUNDDIRTY;
    }
    return &atomic->worldBoundingSphere;
}
#endif
