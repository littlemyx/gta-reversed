/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "AnimationStyleDescriptor.h"
#include "AnimBlendAssocGroup.h"
#include "AnimBlendAssociation.h"
#include "AnimBlock.h"

#include <extensions/ci_string.hpp>

struct IFPSectionHeader;

constexpr auto MAX_ANIM_BLOCK_NAME = 16;
constexpr auto NUM_ANIM_ASSOC_GROUPS = 118;
constexpr auto NUM_ANIM_BLOCKS = 180;

class CAnimManager {
private:
    static std::array<AnimAssocDefinition, NUM_ANIM_ASSOC_GROUPS> ms_aAnimAssocDefinitionsX; // replacement
    static inline auto& ms_aAnimAssocDefinitions = StaticRef<AnimAssocDefinition[NUM_ANIM_ASSOC_GROUPS]>(0x8AA5A8); // std::array - see SurfaceInfos_c
    static inline NOTSA_GLOBAL(ms_numAnimAssocDefinitions, 0xB4EA28, (uint32), {});

    static inline NOTSA_GLOBAL(ms_aAnimAssocGroups, 0xB4EA34, (CAnimBlendAssocGroup*), {});

    static inline auto& ms_aAnimations = StaticRef<std::array<CAnimBlendHierarchy, 2500>>(0xB4EA40);
    static inline NOTSA_GLOBAL(ms_numAnimations, 0xB4EA2C, (int32), {});

    static inline NOTSA_GLOBAL(ms_aAnimBlocks, 0xB5D4A0, (std::array<CAnimBlock, NUM_ANIM_BLOCKS>), {});
    static inline NOTSA_GLOBAL(ms_numAnimBlocks, 0xB4EA30, (uint32), {});

    static inline NOTSA_GLOBAL(ms_AnimCache, 0xB5EB20, (CLinkList<CAnimBlendHierarchy*>), {});

public:
    static void InjectHooks();

    static void Initialise();
    static void Shutdown();

    static void LoadAnimFiles();
    static void LoadAnimFile(RwStream* stream, bool loadCompressed, const char (*uncompressedAnimations)[32] = nullptr);
    static void ReadAnimAssociationDefinitions();

    static CAnimBlock* GetAnimationBlock(AssocGroupId animGroup);
    static CAnimBlock* GetAnimationBlock(const char* name);

    static int32 GetAnimationBlockIndex(AssocGroupId animGroup);
    static int32 GetAnimationBlockIndex(CAnimBlock* animBlock);
    static int32 GetAnimationBlockIndex(const char* name);

    static AssocGroupId GetFirstAssocGroup(const char* name);
    static CAnimBlendHierarchy* GetAnimation(uint32 hash, const CAnimBlock* animBlock);
    static CAnimBlendHierarchy* GetAnimation(const char* animName, const CAnimBlock* animBlock);
    static CAnimBlendHierarchy& GetAnimation(AnimationId id);
    static const char* GetAnimGroupName(AssocGroupId groupId);
    static const char* GetAnimBlockName(AssocGroupId groupId);
    static AssocGroupId GetAnimationGroupIdByName(notsa::ci_string_view name);
    static CAnimBlendStaticAssociation* GetAnimAssociation(AssocGroupId groupId, AnimationId animId);
    static CAnimBlendStaticAssociation* GetAnimAssociation(AssocGroupId groupId, const char* animName);
    static CAnimBlendAssociation* AddAnimationToClump(RpClump* clump, CAnimBlendAssociation* anim); // NOTSA - Internal
    static int32 GetNumRefsToAnimBlock(int32 index);

    static CAnimBlendAssociation* CreateAnimAssociation(AssocGroupId groupId, AnimationId animId);

    static CAnimBlendAssociation* AddAnimation(RpClump* clump, AssocGroupId groupId, AnimationId animId);
    static CAnimBlendAssociation* AddAnimation(RpClump* clump, CAnimBlendHierarchy* hier, int32 clumpAssocFlag);
    static CAnimBlendAssociation* AddAnimationAndSync(RpClump* clump, CAnimBlendAssociation* syncWith, AssocGroupId groupId, AnimationId animId);
    static AnimAssocDefinition* AddAnimAssocDefinition(const char* groupName, const char* blockName, uint32 modelIndex, uint32 animsCount, AnimDescriptor* descriptor);
    static void AddAnimToAssocDefinition(AnimAssocDefinition* definition, const char* animName);
    static void AddAnimBlockRef(int32 index);

    static void CreateAnimAssocGroups();
    static int32 RegisterAnimBlock(const char* name);
    static void RemoveLastAnimFile();
    static void RemoveAnimBlock(int32 index);
    static void RemoveAnimBlockRef(int32 index);
    static void RemoveAnimBlockRefWithoutDelete(int32 index);
    static void RemoveFromUncompressedCache(CAnimBlendHierarchy* hier);

    /*!
     * @addr 0x4D41C0
     * 
     * @brief Uncompress animation data (Unless the animation has the keep-compressed flag).
     * @brief Also marks the anim as recently-used in the LRU cache
     * 
     * @details This function does a 2 things:
     * @details - Uncompress anim data  (Unless the animation has the keep-compressed flag)
     * @details - Since it uses a LRU cache, the hierarchy is put at the front (thus marking it as recently used)
     * @details So even if the anim is already un-compressed this function should be called
     * @details to mark it as recently-used in the LRU cache.

     * @param hier The hierarchy to uncompress
    */
    static void UncompressAnimation(CAnimBlendHierarchy* hier);
    static CAnimBlendAssociation* BlendAnimation(RpClump* clump, CAnimBlendHierarchy* animBlendHier, int32 flags, float clumpAssocBlendData = 8.f);
    static CAnimBlendAssociation* BlendAnimation(RpClump* clump, AssocGroupId groupId, AnimationId animId, float clumpAssocBlendData = 8.f);

    static uint32 GetAnimIndex(const CAnimBlendHierarchy* h);
    static bool IsAnimInBlock(const CAnimBlendHierarchy* h, const CAnimBlock* b);


    /// NOTSA. Get random gangtalk anim
    static AnimationId GetRandomGangTalkAnim();

    static auto GetAnimBlocks() { return ms_aAnimBlocks | rng::views::take(ms_numAnimBlocks); }
    static auto GetAssocGroups() { return std::span{ms_aAnimAssocGroups, ms_numAnimAssocDefinitions}; }
    static auto GetAssocGroupDefs() { return std::span{ms_aAnimAssocDefinitions, ms_numAnimAssocDefinitions}; }

    static void StreamAnimBlock(const char* blck, bool shouldBeLoaded, bool& isLoaded);
private:
    static void LoadAnimFile_ANPK(RwStream* stream, const IFPSectionHeader& h, bool compress, const char (*uncompressedAnims)[32]);
    static void LoadAnimFile_ANP23(RwStream* stream, const IFPSectionHeader& h, bool compress, bool isANP3);
    static auto GetOrCreateAnimBlock(const char* name, uint32 numAnims);
};

// 0x4C4DC0
inline bool IsClumpSkinned(RpClump *clump) {
    const auto a = GetFirstAtomic(clump);
    return a && RpSkinGeometryGetSkin(RpAtomicGetGeometry(a));
}
