// P2B-05c: RtAnim* (rtanim.h) on top of librw's rw::Animation / rw::AnimInterpolator / rw::AnimInterpolatorInfo.
//   RtAnimInitialize 0x7CCCA0, RtAnimRegisterInterpolationScheme 0x7CCD40, RtAnimAnimationCreate 0x7CCE40, RtAnimAnimationDestroy 0x7CCF10,
//   RtAnimInterpolatorSetCurrentAnim 0x7CD5A0.
// The exe's RtAnimAnimation {interpInfo, numFrames, flags, duration, pFrames, customData} + key frames is the same memory layout as librw's rw::Animation
// (header 0x18 bytes, `keyframes` = pFrames), so the game's direct field accesses (RpAnimBlend.cpp: `rtA->numFrames = ...`) work on it unchanged. The interpolator
// is librw's (RW's has more fields: callbacks, sub-interpolator links; the game uses none of them), its frames start at `this + 1` like RW's `animI + 1`.
//
// Deliberate differences:
//   * RtAnimInitialize is a stub (librw has no RtAnim module state); the scheme table is librw's, 10 entries instead of the exe's 16.
//   * RtAnimInterpolatorSetCurrentAnim keeps the exe's ORDER (interpolate callbacks first, then the keyFrame1/2 pointers), which matters for the game's scheme:
//     RpAnimBlendKeyFrameInterpolate zero-fills its whole interp frame, librw's setCurrentAnim sets the pointers first and would have them wiped.
//     It also refuses (returns 0, like librw) an interpolation frame bigger than the interpolator's maximum; the exe has no such check (it would overflow).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#include <cstring>

namespace {
constexpr int kMaxSchemes = 10;   // librw's AnimInterpolatorInfo table size (MAXINTERPINFO)

// What the game handed to RtAnimRegisterInterpolationScheme, kept by typeID for the stream callbacks (librw's have a different signature and no context).
RtAnimInterpolatorInfo   g_rtInfos[kMaxSchemes];
rw::AnimInterpolatorInfo g_rwInfos[kMaxSchemes];
int                      g_numRegistered = 0;

const RtAnimInterpolatorInfo* FindRt(int32_t id) {
    for (int i = 0; i < g_numRegistered; i++) {
        if (g_rtInfos[i].typeID == id) {
            return &g_rtInfos[i];
        }
    }
    return nullptr;
}

void StreamReadTramp(rw::Stream* stream, rw::Animation* anim) {
    if (const auto* rt = FindRt(anim->interpInfo->id); rt && rt->keyFrameStreamReadCB) {
        rt->keyFrameStreamReadCB(stream, anim);
    }
}
void StreamWriteTramp(rw::Stream* stream, rw::Animation* anim) {
    if (const auto* rt = FindRt(anim->interpInfo->id); rt && rt->keyFrameStreamWriteCB) {
        rt->keyFrameStreamWriteCB(anim, stream);
    }
}
rw::uint32 StreamGetSizeTramp(rw::Animation* anim) {
    if (const auto* rt = FindRt(anim->interpInfo->id); rt && rt->keyFrameStreamGetSizeCB) {
        return (rw::uint32)rt->keyFrameStreamGetSizeCB(anim);
    }
    return 0;
}
} // namespace

// 0x7CCCA0 (registers the module's engine plugin; nothing to do on librw)
RwBool RtAnimInitialize(void) {
    return TRUE;
}

// 0x7CCD40: copy of the 0x30-byte info into the scheme table; FALSE when the table is full or the id is already registered.
// The librw table also holds librw's own schemes (hanim 1, uv anim), a duplicate id there counts as registered too.
RwBool RtAnimRegisterInterpolationScheme(RtAnimInterpolatorInfo* interpolatorInfo) {
    if (g_numRegistered >= kMaxSchemes || rw::AnimInterpolatorInfo::find(interpolatorInfo->typeID)) {
        return FALSE;
    }
    const int slot = g_numRegistered++;
    g_rtInfos[slot] = *interpolatorInfo;
    const RtAnimInterpolatorInfo& rt = g_rtInfos[slot];
    rw::AnimInterpolatorInfo&     ri = g_rwInfos[slot];
    ri.id                 = rt.typeID;
    ri.interpKeyFrameSize = rt.interpKeyFrameSize;
    ri.animKeyFrameSize   = rt.animKeyFrameSize;
    ri.customDataSize     = rt.customDataSize;
    ri.applyCB            = rt.keyFrameApplyCB;
    ri.blendCB            = rt.keyFrameBlendCB;
    ri.interpCB           = rt.keyFrameInterpolateCB;
    ri.addCB              = rt.keyFrameAddCB;
    ri.mulRecipCB         = rt.keyFrameMulRecipCB;
    ri.streamRead         = StreamReadTramp;
    ri.streamWrite        = StreamWriteTramp;
    ri.streamGetSize      = StreamGetSizeTramp;
    rw::AnimInterpolatorInfo::registerInterp(&ri);
    return TRUE;
}

// 0x7CCE40. Allocation = header + numFrames * animKeyFrameSize + customDataSize; customData is NULL without custom data.
RtAnimAnimation* RtAnimAnimationCreate(RwInt32 typeID, RwInt32 numFrames, RwInt32 flags, RwReal duration) {
    rw::AnimInterpolatorInfo* info = rw::AnimInterpolatorInfo::find(typeID);
    if (!info) {
        return nullptr;
    }
    const int32_t size = info->animKeyFrameSize * numFrames + info->customDataSize + (int32_t)sizeof(rw::Animation);
    auto* anim = static_cast<rw::Animation*>(rwMalloc(size, rw::MEMDUR_EVENT | rw::ID_ANIMANIMATION));
    if (!anim) {
        return nullptr;
    }
    anim->interpInfo = info;
    anim->numFrames  = numFrames;
    anim->flags      = flags;
    anim->duration   = duration;
    anim->keyframes  = anim + 1;
    anim->customData = info->customDataSize > 0 ? static_cast<uint8_t*>(anim->keyframes) + info->animKeyFrameSize * numFrames : nullptr;
    return anim;
}

// 0x7CCF10
RwBool RtAnimAnimationDestroy(RtAnimAnimation* animation) {
    rwFree(animation);
    return TRUE;
}

// 0x7CD5A0
RwBool RtAnimInterpolatorSetCurrentAnim(RtAnimInterpolator* animI, RtAnimAnimation* anim) {
    rw::AnimInterpolatorInfo* info = anim->interpInfo;
    if (info->interpKeyFrameSize > animI->maxInterpKeyFrameSize) {
        return FALSE;
    }
    animI->currentAnim                = anim;
    animI->currentTime                = 0.0f;
    animI->currentInterpKeyFrameSize  = info->interpKeyFrameSize;
    animI->currentAnimKeyFrameSize    = info->animKeyFrameSize;
    animI->applyCB                    = info->applyCB;
    animI->blendCB                    = info->blendCB;
    animI->interpCB                   = info->interpCB;
    animI->addCB                      = info->addCB;
    const int32_t n = animI->numNodes;
    for (int32_t i = 0; i < n; i++) {
        if (animI->interpCB) {
            animI->interpCB(animI->getInterpFrame(i), animI->getAnimFrame(i), animI->getAnimFrame(n + i), 0.0f, anim->customData);
        }
    }
    for (int32_t i = 0; i < n; i++) {
        rw::InterpFrameHeader* f = animI->getInterpFrame(i);
        f->keyFrame1 = animI->getAnimFrame(i);
        f->keyFrame2 = animI->getAnimFrame(n + i);
    }
    animI->nextFrame = animI->getAnimFrame(2 * n);
    return TRUE;
}
#endif // NOTSA_RW_LIBRW
