#include "StdInc.h"

#include "TaskSimpleFight.h"

#include <numbers>

#include "Game.h"
#include "Glass.h"
#include "Crime.h"
#include "FileMgr.h"
#include "FileLoader.h"
#include "Fx.h"
#include "RwHelper.h"
#include "PedModelInfo.h"
#include "PedStats.h"
#include "EventSoundQuiet.h"
#include "EventVehicleDamageWeapon.h"
#include "Plugins/RpAnimBlendPlugin/RpAnimBlend.h"
#include "TaskSimpleFall.h"

// Storage used by `FightSetUpCol` (The original keeps the collision model, its data, and the one sphere in .bss)
// NOTE: `col1[1]` (Game.h) really is the `CCollisionData` at 0xC17854, see `CGame::ShutDownForRestart`
static inline auto& s_FightColModel = StaticRef<CColModel>(0xC17824);
static inline auto& s_FightColData  = StaticRef<CCollisionData>(0xC17854);
static inline auto& s_FightColSphere = StaticRef<CColSphere>(0xC17884);

// 0x59C910 - CVector::Normalise as the original evaluates it: the sum of squares and the reciprocal root stay in the FPU
// (extended precision), every component is stored as float. A length of 0 (or less) yields (1, y, z) - only x is written.
static void NormaliseExt(CVector& v) {
    const double sq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sq <= 0.0) { // FCOM + JP: NaN takes the sqrt path
        v.x = 1.0f;
        return;
    }
    const double inv = 1.0 / std::sqrt(sq);
    v.x = (float)(v.x * inv);
    v.y = (float)(v.y * inv);
    v.z = (float)(v.z * inv);
}

// Max volume of the melee hit sounds (indexed with `m_nCurrentMove`)
static inline auto& s_HitSoundMaxVolume = StaticRef<std::array<uint32, 3>>(0x8D2E3C);

// 0x59C890 - the original evaluation order; the sum stays in the FPU registers (extended precision), stored as float
static CVector TransformPointExt(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x) + p.x),
        (float)((((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y) + p.y),
        (float)((((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y) + p.z)
    };
}

void CTaskSimpleFight::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleFight, 0x86D684, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x61C470);
    RH_ScopedInstall(Destructor, 0x61C530);

    RH_ScopedInstall(LoadMeleeData, 0x5BEDC0);
    RH_ScopedInstall(GetHitLevel, 0x5BD360);
    RH_ScopedInstall(GetHitSound, 0x5BD3B0);

    RH_ScopedInstall(FightSetUpCol, 0x61D5F0);
    RH_ScopedInstall(BeHitWhileBlocking, 0x61C650);
    RH_ScopedInstall(GetStrikeDamage, 0x61C740);
    RH_ScopedInstall(FightHitPed, 0x61CBA0);
    RH_ScopedInstall(FightHitCar, 0x61D0B0);
    RH_ScopedInstall(FightHitObj, 0x61D400);
    RH_ScopedInstall(FightStrike, 0x6240B0);

    RH_ScopedInstall(IsTargetInRange, 0x61D6F0);
    RH_ScopedInstall(GetAvailableComboSet, 0x61C7F0);
    RH_ScopedInstall(ChooseAttackPlayer, 0x624710);
    RH_ScopedInstall(ChooseAttackAI, 0x624A40);
    RH_ScopedInstall(StartAnim, 0x623B10);
    RH_ScopedInstall(SetPlayerMoveAnim, 0x61C9B0);

    RH_ScopedInstall(GetComboAnimGroupID, 0x4ABDA0);
    RH_ScopedInstall(IsComboSet, 0x4ABDC0);
    RH_ScopedInstall(IsHitComboSet, 0x4ABDF0);
    RH_ScopedInstall(ControlFight, 0x61C5E0);
    RH_ScopedInstall(FinishMeleeAnimCB, 0x61DAE0);
    RH_ScopedVMTInstall(MakeAbortable, 0x6239F0);
    RH_ScopedVMTInstall(ProcessPed, 0x629920);
}

// 0x61C470
CTaskSimpleFight::CTaskSimpleFight(CEntity* entity, int32 nCommand, uint32 nIdlePeriod) : CTaskSimple() {
    m_nComboSet          = -1;
    m_nCurrentMove       = (eFightAttackType)-1;
    m_bIsFinished        = false;
    m_bIsInControl       = true;
    m_bAnimsReferenced   = false;
    m_nRequiredAnimGroup = (AssocGroupId)0x21; // "No group"
    m_nIdleCounter       = 0;
    m_nContinueStrike    = 0;
    m_nChainCounter      = 0;
    m_pTargetEntity      = entity;
    m_pAnim              = nullptr;
    m_pIdleAnim          = nullptr;
    m_nNextCommand       = (uint8)nCommand;
    m_nLastCommand       = 0;

    CEntity::SafeRegisterRef(m_pTargetEntity);

    m_nIdlePeriod = (uint16)std::min<uint32>(nIdlePeriod, 60'000);
}

// 0x61C530
CTaskSimpleFight::~CTaskSimpleFight() {
    CEntity::SafeCleanUpRef(m_pTargetEntity);

    if (m_pAnim) {
        m_pAnim->SetDefaultDeleteCallback();
    }
    if (m_pIdleAnim) {
        m_pIdleAnim->SetDefaultDeleteCallback();
    }

    if (m_bAnimsReferenced && m_nRequiredAnimGroup != 0x21) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex(m_nRequiredAnimGroup));
        m_bAnimsReferenced = false;
    }
}

// 0x61C5E0
bool CTaskSimpleFight::ControlFight(CEntity* entity, uint8 command) {
    m_bIsInControl = true;
    if (entity != m_pTargetEntity) {
        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = entity;
        CEntity::SafeRegisterRef(m_pTargetEntity);
    }
    // NOTE: The original compares as signed chars
    if ((int8)m_nNextCommand < (int8)command) {
        m_nNextCommand = command;
    }
    return true;
}

// 0x4ABDC0
bool CTaskSimpleFight::IsComboSet() {
    const auto idx = std::max<int32>(m_nComboSet - 4, 0); // (Original: branchless clamp)
    return (m_aComboData[idx].m_wFlags & (0x10u << (m_nCurrentMove & 0x1F))) != 0;
}

// 0x4ABDF0
bool CTaskSimpleFight::IsHitComboSet() {
    if (m_nCurrentMove > 2) { // signed compare
        return false;
    }
    const auto idx = std::max<int32>(m_nComboSet - 4, 0);
    return (m_aComboData[idx].m_wFlags & (0x1000u << (m_nCurrentMove & 0x1F))) != 0;
}

// 0x4ABDA0
AssocGroupId CTaskSimpleFight::GetComboAnimGroupID() {
    const auto idx = std::max<int32>(m_nComboSet - 4, 0);
    return m_aComboData[idx].m_nAnimGroup;
}

// 0x5BD360
uint8 CTaskSimpleFight::GetHitLevel(const char* str) {
    switch (str[0]) {
    case 'H': return 0;
    case 'L': return 1;
    case 'G': return 2;
    case 'B': return 3;
    }
    // BUG: The original continues by comparing the (zero extended) first char with 2 char constants ("HL", "LL", "GL"), which can never be true.
    return 7;
}

// 0x5BD3B0
int32 CTaskSimpleFight::GetHitSound(int32 level) {
    switch (level) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8: return 60 + level;
    default: return 62;
    }
}

// 0x5BEDC0
void CTaskSimpleFight::LoadMeleeData() {
    ZoneScoped;

    // Defaults
    for (auto& info : m_aComboData) {
        info.m_nAnimGroup = (AssocGroupId)0x21;
        info.m_fRanges    = 1.5f;
        for (auto i = 0u; i < 5; i++) {
            info.m_fHit[i]      = 100.f;
            info.m_fChain[i]    = 100.f;
            info.m_fRadius[i]   = 1.f;
            info.m_nHitLevel[i] = 7;
            info.m_nDamage[i]   = 0;
            info.m_Hit[i]       = 0;
            info.m_AltHit[i]    = 0;
        }
        info.m_fGroundLoop = 0.f;
        info.ABlockHit     = 100.f;
        info.ABlockChain   = 100.f;
        info.m_wFlags      = 0;
    }
    for (auto& offset : m_aHitOffsets) {
        offset = CVector{0.f, 0.75f, 0.f};
    }

    const auto f = CFileMgr::OpenFile("DATA\\melee.dat", "rb");

    bool bInCombo  = false;
    bool bInLevels = false;
    auto lineIdx   = 0u; // Index of the line inside the current section
    auto comboIdx  = 0u;

    for (auto line = CFileLoader::LoadLine(f); line; line = CFileLoader::LoadLine(f)) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        if (strncmp(line, "END_MELEE_DATA", 14) == 0) {
            break;
        }

        if (!bInCombo && !bInLevels) {
            if (strncmp(line, "START_COMBO", 11) == 0) {
                bInCombo = true;
            } else if (strncmp(line, "START_LEVELS", 12) == 0) {
                bInLevels = true;
            }
            continue;
        }

        if (strncmp(line, "END_COMBO", 9) == 0) {
            if (bInCombo) {
                comboIdx++;
            }
            lineIdx   = 0;
            bInCombo  = false;
            bInLevels = false;
            continue;
        }

        if (bInLevels) {
            char  name[64]{};
            float x{}, y{}, z{};
            sscanf_s(line, "%s %f %f %f", SCANF_S_STR(name), &x, &y, &z);
            m_aHitOffsets[lineIdx] = CVector{x, y, z};
            lineIdx++;
            continue;
        }

        // Inside a combo, each line has a different meaning
        auto& info = m_aComboData[comboIdx];
        switch (lineIdx++) {
        case 0: { // Anim group name
            char name[64]{}, grpName[64]{};
            sscanf_s(line, "%s %s", SCANF_S_STR(name), SCANF_S_STR(grpName));
            for (auto g = 0u; g < CAnimManager::GetAssocGroupDefs().size(); g++) {
                if (strcmp(grpName, CAnimManager::GetAnimGroupName((AssocGroupId)g)) == 0) {
                    info.m_nAnimGroup = (AssocGroupId)g;
                    break;
                }
            }
            break;
        }
        case 1: { // Range
            char  name[64]{};
            float range{};
            sscanf_s(line, "%s %f", SCANF_S_STR(name), &range);
            info.m_fRanges = range;
            break;
        }
        case 2:
        case 3:
        case 4:
        case 5:
        case 6: { // Moves
            const auto move = lineIdx - 1 - 2;

            char  name[64]{}, level[64]{};
            float hit{}, chain{}, radius{}, groundLoop{};
            int32 damage{}, sound{}, altSound{};
            sscanf_s(line, "%s %f %f %f %s %d %d %d %f", SCANF_S_STR(name), &hit, &chain, &radius, SCANF_S_STR(level), &damage, &sound, &altSound, &groundLoop);

            constexpr float FRAMES_TO_SECS = 1.f / 30.f; // 0x858F10
            info.m_fHit[move]      = hit * FRAMES_TO_SECS;
            info.m_fChain[move]    = chain * FRAMES_TO_SECS;
            info.m_fRadius[move]   = radius;
            info.m_nHitLevel[move] = GetHitLevel(level);
            info.m_nDamage[move]   = (uint8)damage;
            info.m_Hit[move]       = (int16)GetHitSound(sound);
            info.m_AltHit[move]    = (int16)GetHitSound(altSound);
            if (groundLoop > 0.f) {
                info.m_fGroundLoop = groundLoop * FRAMES_TO_SECS;
            }
            break;
        }
        case 7: { // Block
            char  name[64]{};
            float hit{}, chain{};
            sscanf_s(line, "%s %f %f", SCANF_S_STR(name), &hit, &chain);
            constexpr float FRAMES_TO_SECS = 1.f / 30.f; // 0x858F10
            info.ABlockHit   = hit * FRAMES_TO_SECS;
            info.ABlockChain = chain * FRAMES_TO_SECS;
            break;
        }
        case 8: { // Flags
            char   name[64]{};
            uint32 flags{};
            sscanf_s(line, "%s %x", SCANF_S_STR(name), &flags);
            info.m_wFlags = (uint16)flags;
            break;
        }
        }
    }

    CFileMgr::CloseFile(f);
}

// 0x61DAE0 - Finish/delete callback of `m_pAnim` and `m_pIdleAnim` (`data` is the task)
void CTaskSimpleFight::FinishMeleeAnimCB(CAnimBlendAssociation* anim, void* data) {
    auto* const task = static_cast<CTaskSimpleFight*>(data);

    if (task->m_pAnim == anim) {
        task->m_pAnim = nullptr;
    } else if (task->m_pIdleAnim == anim) {
        task->m_pIdleAnim = nullptr;
    }

    if (anim->m_AnimId == ANIM_ID_FIGHT2IDLE) {
        task->m_bIsFinished = true;
    }

    if (!task->m_pIdleAnim) {
        switch (task->m_nLastCommand) {
        case 1:
        case 0xF:
        case 0x10:
        case 0x11:
            task->m_bIsFinished = true;
            break;
        }
    }
}

// Low byte of the flags of the combo set `comboSet`
// NOTE: The original doesn't care that `comboSet` can be < 4 here (no set picked), and reads whatever precedes the array
static uint8 GetComboFlagsLo(int8 comboSet) {
    return *(reinterpret_cast<const uint8*>(&CTaskSimpleFight::m_aComboData[0]) + (comboSet - 4) * (int32)sizeof(CMeleeInfo) + offsetof(CMeleeInfo, m_wFlags));
}

// Float member (at `offset` in `CMeleeInfo`) of the combo set `comboSet`
// NOTE: Like `GetComboFlagsLo`, doesn't care that `comboSet` can be < 4 (the original reads whatever precedes the array)
static float GetComboFloatRaw(int8 comboSet, size_t offset) {
    return *reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(&CTaskSimpleFight::m_aComboData[0]) + (comboSet - 4) * (int32)sizeof(CMeleeInfo) + offset);
}

// 0x61D6F0 (name guessed) - Is the target within reach of the current hit? Called by `ChooseAttack{Player,AI}`
// Without a target it picks the best ped from the scanner (and turns the ped towards it), when the player is the one fighting
bool CTaskSimpleFight::IsTargetInRange(CPed* ped) {
    if (!CLocalisation::KickingWhenDown()) { // 0x56D270
        return false;
    }

    // x87: The sum isn't rounded to float
    // NOTE: `m_nComboSet` can be < 4 here
    const double reach = (double)m_aHitOffsets[2].y + (double)GetComboFloatRaw(m_nComboSet, offsetof(CMeleeInfo, m_fRadius) + 3 * sizeof(float)); // 0xC177EC + (0xC1710C + ..)
    const float  limitA = (float)((double)0.2f + reach); // 0x858CC4 - Limit for the "right" axis
    float        limitB = (float)(reach + (double)0.4f);  // 0x858EE8 - Limit for the "forward" axis

    CVector bonePos{};
    auto* const target = m_pTargetEntity;
    if (target) {
        if (target->GetType() != ENTITY_TYPE_PED) {
            // Not a ped => true only if it's what the ped is standing on
            return target == ped->m_standingOnEntity;
        }

        auto* const tped = static_cast<CPed*>(target);
        tped->GetBonePosition(&bonePos, BONE_SPINE1, false); // 0x5E4280
        if (tped->IsAlive()) { // 0x5E0170
            // Is the bone at least as high as the ped's feet?
            if (!((double)ped->GetPosition().z - (double)0.2f > (double)bonePos.z)) {
                if (!tped->bIsDucking) {
                    return false;
                }
                limitB = (float)((double)limitB - (double)0.4f);
            }
        }

        // Still falling down?
        if (auto* const task = tped->GetIntelligence()->m_TaskMgr.GetSimplestActiveTask()) { // 0x6819D0
            if (task->GetTaskType() == TASK_SIMPLE_FALL) {
                if (auto* const anim = static_cast<CTaskSimpleFall*>(task)->m_pAnim) {
                    if (anim->m_BlendHier->m_fTotalTime > anim->m_CurrentTime) {
                        return false;
                    }
                }
            }
        }

        const auto& tm = target->GetMatrix(); // 0x411990 (it's called twice in the original, the 2nd time is a no-op)
        const auto& pp = ped->GetPosition();
        const auto& tp = target->GetPosition();

        // x87: The 1st dot product uses the unrounded differences
        {
            const double dx = (double)tp.x - (double)pp.x;
            const double dy = (double)tp.y - (double)pp.y;
            const double dz = (double)tp.z - (double)pp.z;
            const auto&  f  = tm.GetForward();
            if (!(std::fabs((dz * f.z + dy * f.y) + dx * f.x) < (double)limitB)) {
                return false;
            }
        }

        // 0x40FE60 (rounds to float), 0x40FDB0 (the sum stays in the FPU)
        const CVector d{
            (float)((double)tp.x - (double)pp.x),
            (float)((double)tp.y - (double)pp.y),
            (float)((double)tp.z - (double)pp.z)
        };
        const auto& r = tm.GetRight();
        return std::fabs(((double)d.z * r.z + (double)d.y * r.y) + (double)d.x * r.x) < (double)limitA;
    }

    // No target => only the player looks for one
    if (!ped->IsPlayer()) { // 0x5DF8F0
        return false;
    }

    CPed*       best      = nullptr;
    float       bestAngle = 0.f;
    float       bestDiff  = std::numbers::pi_v<float>;
    auto* const intel     = ped->GetIntelligence();
    for (auto i = 0; i < 16; i++) {
        auto* const cand = static_cast<CPed*>(intel->m_pedScanner.m_apEntities[i]);
        if (!cand) {
            continue;
        }

        CVector candBone{};
        cand->GetBonePosition(&candBone, BONE_SPINE1, false); // 0x5E4280
        if (cand->m_nPedState != PEDSTATE_DEAD) {
            if (!((double)ped->GetPosition().z - (double)0.2f > (double)candBone.z)) {
                continue;
            }
        }

        const auto& pp = ped->GetPosition();
        const auto& cp = cand->GetPosition();
        // x87: X and Y are rounded to float, Z isn't
        const float  dx = (float)((double)cp.x - (double)pp.x);
        const float  dy = (float)((double)cp.y - (double)pp.y);
        const double dz = (double)cp.z - (double)pp.z;

        // NOTE: Like in the original, there's no check for the matrix
        const auto& cm = *cand->m_matrix;
        if (!(std::fabs((dz * cm.GetForward().z + (double)dy * cm.GetForward().y) + (double)dx * cm.GetForward().x) < (double)limitB)) {
            continue;
        }
        if (!(std::fabs((dz * cm.GetRight().z + (double)dy * cm.GetRight().y) + (double)dx * cm.GetRight().x) < (double)limitA)) {
            continue;
        }

        const float ang  = (float)std::atan2(-(double)dx, (double)dy);
        double      diff = (double)ang - (double)ped->m_fCurrentRotation;
        if (diff < -(double)std::numbers::pi_v<float>) { // 0x858CC0
            diff += (double)(2.f * std::numbers::pi_v<float>); // 0x858CBC
        } else if (diff > (double)std::numbers::pi_v<float>) { // 0x858CB8
            diff -= (double)(2.f * std::numbers::pi_v<float>);
        }
        if (diff < 0.0) {
            diff *= -1.0; // 0x858C1C
        }
        if (!(diff < (double)(std::numbers::pi_v<float> / 3.f))) { // 0x8630F8
            continue;
        }

        // Better than the best one so far? (any living one is better than a dead one)
        const bool bBetter = best && best->m_fHealth <= 0.f && (double)cand->m_fHealth > 0.0 // 0x859EF8 (double 0.0)
            ? true
            : !best || diff < (double)bestDiff;
        if (bBetter) {
            bestDiff  = (float)diff;
            best      = cand;
            bestAngle = ang;
        }
    }

    if (best) {
        ped->m_fAimingRotation = bestAngle;
        return true;
    }

    // Nothing to aim at => true if the ped is standing on a car
    if (auto* const contact = ped->m_standingOnEntity) {
        if (contact->GetType() == ENTITY_TYPE_VEHICLE && static_cast<CVehicle*>(contact)->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
            return true;
        }
    }
    return false;
}

// 0x61C7F0 - Picks the combo set for `command`, and makes sure its anim block is referenced (requests it if it's not loaded)
int8 CTaskSimpleFight::GetAvailableComboSet(CPed* ped, int8 command) {
    // Only make sure the required anims are referenced
    if (command < 0) {
        if (m_nRequiredAnimGroup != 0x21 && !m_bAnimsReferenced) {
            auto* blk = CAnimManager::GetAnimationBlock(m_nRequiredAnimGroup);
            if (!blk) {
                blk = CAnimManager::GetAnimationBlock(CAnimManager::GetAnimBlockName(m_nRequiredAnimGroup));
            }
            if (blk->IsLoaded) {
                CAnimManager::AddAnimBlockRef(CAnimManager::GetAnimationBlockIndex(blk)); // 0x4D3FB0
                m_bAnimsReferenced = true;
            }
        }
        return 0;
    }

    // 0 = idle, 2 = block, 11..14 = attacks
    if (command != 2 && command != 0 && (command < 11 || command > 14)) {
        return 0;
    }

    const auto* const wi = CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, (eWeaponSkill)1);
    int8 comboSet = (int8)wi->m_nBaseCombo;
    if (command == 12) {
        comboSet = (int8)ped->m_nFightingStyle;
    } else {
        if (comboSet == 4 && (command == 2 || command == 0)) {
            comboSet = (int8)ped->m_nFightingStyle;
        }
        if (command == 0 && !(m_aComboData[comboSet - 4].m_wFlags & 0x400)) {
            return 4; // Doesn't have the "idle" set
        }
    }

    const auto& combo = m_aComboData[comboSet - 4];
    if (combo.m_nAnimGroup == 0x21) { // No group
        return comboSet;
    }

    if (combo.m_nAnimGroup != m_nRequiredAnimGroup) {
        if (m_bAnimsReferenced) {
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex(m_nRequiredAnimGroup)); // 0x4D3FD0
            m_bAnimsReferenced = false;
        }
        m_nRequiredAnimGroup = combo.m_nAnimGroup;
    } else if (m_bAnimsReferenced) {
        return comboSet;
    }

    auto* blk = CAnimManager::GetAnimationBlock(m_nRequiredAnimGroup);
    if (!blk) {
        blk = CAnimManager::GetAnimationBlock(CAnimManager::GetAnimBlockName(m_nRequiredAnimGroup));
    }
    const auto blkIdx = CAnimManager::GetAnimationBlockIndex(blk);
    if (blk->IsLoaded) {
        CAnimManager::AddAnimBlockRef(blkIdx);
        m_bAnimsReferenced = true;
        return comboSet;
    }

    // Not loaded yet => request it, and fall back to the standard set for now
    CStreaming::RequestModel(IFPToModelId(blkIdx), STREAMING_KEEP_IN_MEMORY); // 8
    if ((int8)m_nNextCommand >= 11 && (int8)m_nNextCommand <= 14) {
        m_nNextCommand = 11;
    }
    return 4;
}

// 0x624710 - Returns the move (-1 = none) the player should do next (see `ProcessPed` => `StartAnim`)
int16 CTaskSimpleFight::ChooseAttackPlayer(CPed* ped) {
    const bool bNoTarget = !m_pTargetEntity;

    int16 move = -1;
    if (!((int8)m_nNextCommand >= 11 && (int8)m_nNextCommand <= 14 && m_nComboSet >= 4)) {
        move = 1;
    }

    // Moves this combo set has (bits 0/1 = 2nd/3rd hit, 2 = blocking, 3 = running)
    uint8 moves = GetComboFlagsLo(m_nComboSet);
    if (m_nComboSet > 4 && m_nComboSet <= 7) {
        moves &= (uint8)ped->m_nAllowedAttackMoves;
    }

    if (move < 0) {
        if (m_pAnim && m_nCurrentMove != FIGHT_ATTACK_FIGHT_BLOCK && m_nCurrentMove != FIGHT_ATTACK_FIGHTIDLE) {
            ped->SetMoveState(PEDMOVE_STILL); // 0x5DEC00

            int16 maxMove = 0;
            if (moves & 2) {
                maxMove = 2;
            } else if (moves & 1) {
                maxMove = 1;
            }

            if (IsTargetInRange(ped)) { // 0x61D6F0
                move = -1;
            } else {
                bool bFallback = true;
                if (m_nNextCommand != m_nLastCommand && (int32)m_nChainCounter <= StaticRef<int32>(0x8D2E48)) {
                    m_nChainCounter++;
                    const int32 v = (int8)m_nCurrentMove - (int8)m_nLastCommand + (int8)m_nNextCommand;
                    if (v & 1) {
                        move = 0;
                    } else {
                        move = (v & 2) ? 1 : 2;
                    }
                    bFallback = false;
                    if (move > maxMove) {
                        move = 0;
                    }
                }
                if (bFallback) {
                    move = (int16)((int8)m_nCurrentMove + 1);
                    if (move > maxMove) {
                        move = -1;
                    }
                }
            }
        } else {
            if ((int32)ped->m_nMoveState > (int32)PEDMOVE_WALK) { // Signed compare (`cmp [ped+0x534], 4`)
                if (!(moves & 8)) {
                    m_nComboSet = 4;
                }
                move = 4;
            } else if (IsTargetInRange(ped)) { // 0x61D6F0
                if (m_pAnim && m_nNextCommand != m_nLastCommand) {
                    return -1;
                }
                if (!(moves & 4)
                    || (m_pTargetEntity && m_pTargetEntity->GetType() == ENTITY_TYPE_PED && static_cast<CPed*>(m_pTargetEntity)->bIsDucking)
                ) {
                    m_nComboSet = 4;
                }
                return 3;
            } else {
                m_nChainCounter = 0;
                move = 0;
            }
        }
    }

    // No target => aim at the best one in the scanner
    if (bNoTarget) {
        float bestAngle = -1000.f;
        float bestDiff  = 1000.f;
        const float range = StaticRef<float>(0x8D2E8C); // 2.0

        auto* const intel = ped->GetIntelligence();
        for (auto i = 0; i < 16; i++) {
            auto* const cand = static_cast<CPed*>(intel->m_pedScanner.m_apEntities[i]);
            if (!cand || !cand->IsAlive()) { // 0x5E0170
                continue;
            }

            // x87: the differences aren't rounded to float
            const auto& pedPos  = ped->GetPosition();
            const auto& candPos = cand->GetPosition();
            const double dx = (double)candPos.x - (double)pedPos.x;
            const double dy = (double)candPos.y - (double)pedPos.y;
            const double dz = (double)candPos.z - (double)pedPos.z;
            const double distSq = (dz * dz + dy * dy) + dx * dx;
            if (!((double)range * (double)range > distSq)) {
                continue;
            }

            const double angle = std::atan2(-dx, dy);
            double       diff  = angle - (double)ped->m_fCurrentRotation;
            if (diff > (double)std::numbers::pi_v<float>) { // 0x858CB8
                diff -= (double)(2.f * std::numbers::pi_v<float>); // 0x858CBC
            } else if (diff < -(double)std::numbers::pi_v<float>) { // 0x858CC0
                diff += (double)(2.f * std::numbers::pi_v<float>);
            }
            diff = std::fabs(diff);
            if (diff < (double)bestDiff) {
                bestDiff  = (float)diff;
                bestAngle = (float)angle;
            }
        }
        if (bestAngle > -10.f) { // 0x859004
            ped->m_fAimingRotation = bestAngle;
        }
    }

    return move;
}

// 0x624A40 - Returns the move (0 = hit, 1 = ..., 2 = block, 3 = ...) the AI ped should do next (see `ProcessPed` => `StartAnim`)
int16 CTaskSimpleFight::ChooseAttackAI(CPed* ped) {
    uint8 moves = GetComboFlagsLo(m_nComboSet);
    if (m_nComboSet > 4 && m_nComboSet <= 7) {
        moves &= (uint8)ped->m_nAllowedAttackMoves;
    }

    // x87: The product is exact, but only the second use sees it rounded to float
    const double rollD = (double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL; // 0x821B1E, 0x858C7C
    const float  rollF = (float)rollD;

    if (rollD > (double)0.8f && (moves & 2)) { // 0x858C98
        // Block unless the target is a player that's strong enough (wanted, healthy)
        auto* const tgt = m_pTargetEntity;
        if (!(moves & 0x40)
            || !tgt
            || tgt->GetType() != ENTITY_TYPE_PED
            || !static_cast<CPed*>(tgt)->GetPlayerData()
            || !((int32)static_cast<CPlayerPed*>(tgt)->GetWantedLevel() > 0) // 0x41BE60
            || !(static_cast<CPed*>(tgt)->m_fHealth > 20.f)                  // 0x858BA4
        ) {
            return 2;
        }
    } else if (IsTargetInRange(ped) && (moves & 4)) { // 0x61D6F0
        return 3;
    }

    if (rollF > 0.5f && (moves & 1)) { // 0x858B8C
        return 1;
    }
    return 0;
}

// 0x623B10 - Starts the animation for the move (`move` < 0 = none) of the command in `m_nNextCommand`, and consumes it
void CTaskSimpleFight::StartAnim(CPed* ped, int32 move) {
    if (move < 0) {
        m_nNextCommand = 0;
        return;
    }

    if (m_pAnim) {
        m_pAnim->SetDefaultDeleteCallback(); // 0x4CEBC0
        m_pAnim = nullptr;
    }

    auto* const clump = ped->GetRpClump();
    const int32 cmd   = (int8)m_nNextCommand;

    if (cmd >= 0 && cmd <= 0x12) {
        switch (cmd) {
        case 0: { // Idle
            m_nComboSet       = 0;
            m_nCurrentMove    = (eFightAttackType)0;
            m_nContinueStrike = 0;

            if ((int32)ped->m_nMoveState >= (int32)PEDMOVE_WALK && ped->IsPlayer()) { // 0x5DF8F0
                m_bIsFinished = true;
                break;
            }

            const auto* const wi = CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, (eWeaponSkill)1);
            int8 comboSet = (int8)wi->m_nBaseCombo;
            if (comboSet == 4) {
                comboSet = (int8)ped->m_nFightingStyle;
            }
            const auto& combo = m_aComboData[comboSet - 4];

            bool bUseSet = true; // false => fall back to the standard set
            if (!(combo.m_wFlags & 0x400)) {
                bUseSet = false;
            } else if (combo.m_nAnimGroup != 0x21) {
                bool bGotAnims = true;
                if (combo.m_nAnimGroup == m_nRequiredAnimGroup) {
                    bGotAnims = m_bAnimsReferenced;
                } else {
                    if (m_bAnimsReferenced) {
                        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex(m_nRequiredAnimGroup)); // 0x4D3FD0
                        m_bAnimsReferenced = false;
                    }
                    m_nRequiredAnimGroup = combo.m_nAnimGroup;
                    bGotAnims = false;
                }
                if (!bGotAnims) {
                    auto* blk = CAnimManager::GetAnimationBlock(m_nRequiredAnimGroup);
                    if (!blk) {
                        blk = CAnimManager::GetAnimationBlock(CAnimManager::GetAnimBlockName(m_nRequiredAnimGroup));
                    }
                    const auto blkIdx = CAnimManager::GetAnimationBlockIndex(blk);
                    if (blk->IsLoaded) {
                        CAnimManager::AddAnimBlockRef(blkIdx);
                        m_bAnimsReferenced = true;
                    } else {
                        CStreaming::RequestModel(IFPToModelId(blkIdx), STREAMING_KEEP_IN_MEMORY); // 8
                        if ((int8)m_nNextCommand >= 11 && (int8)m_nNextCommand <= 14) {
                            m_nNextCommand = 11;
                        }
                        bUseSet = false;
                    }
                }
            }
            if (!bUseSet) {
                comboSet = 4;
            }

            m_nComboSet = comboSet;
            if (!m_pIdleAnim) {
                m_pIdleAnim = CAnimManager::BlendAnimation(clump, m_aComboData[comboSet - 4].m_nAnimGroup, ANIM_ID_FIGHT_IDLE, 8.f); // 0x4D4610
                m_pIdleAnim->SetDeleteCallback(FinishMeleeAnimCB, this); // 0x4CEBC0
            } else if (m_pIdleAnim->m_BlendAmount < 1.f && m_pIdleAnim->m_BlendDelta <= 0.f) {
                // NOTE: No callback this time
                m_pIdleAnim = CAnimManager::BlendAnimation(clump, m_aComboData[comboSet - 4].m_nAnimGroup, ANIM_ID_FIGHT_IDLE, 8.f);
            }

            if (ped->IsPlayer()) {
                ped->GetPlayerData()->m_vecFightMovement = CVector2D{0.f, 0.f};
                SetPlayerMoveAnim(static_cast<CPlayerPed*>(ped)); // 0x61C9B0
            }
            m_nComboSet = 0;
            break;
        }
        case 2: { // Block
            m_nCurrentMove    = (eFightAttackType)0;
            m_nContinueStrike = 0;
            if (!(m_aComboData[m_nComboSet - 4].m_wFlags & 0x200)) {
                m_nComboSet = 4;
            }
            m_pAnim = CAnimManager::BlendAnimation(clump, m_aComboData[m_nComboSet - 4].m_nAnimGroup, ANIM_ID_FIGHT_FIGHT_BLOCK, 8.f);
            m_pAnim->SetFinishCallback(FinishMeleeAnimCB, this); // 0x4CEBE0
            break;
        }
        case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: { // Ground attacks/moves (non-players only)
            if (ped->IsPlayer()) {
                break;
            }
            m_nContinueStrike = 0;
            m_nComboSet       = 1;

            AnimationId animId;
            switch (cmd) {
            case 7:
                m_nCurrentMove = (eFightAttackType)0;
                animId         = ANIM_ID_FIGHTSHF;
                break;
            case 9:
                m_nCurrentMove = (eFightAttackType)2;
                animId         = ANIM_ID_FIGHTSHB;
                break;
            default:
                m_nCurrentMove = (eFightAttackType)(cmd == 8 ? 1 : cmd == 10 ? 2 : (int8)(cmd - 3));
                animId         = (AnimationId)(ANIM_ID_FIGHTSH_FWD + (int32)m_nCurrentMove);
                break;
            }
            m_pAnim = CAnimManager::BlendAnimation(clump, ANIM_GROUP_DEFAULT, animId, 8.f);

            if (m_nNextCommand == 3) {
                m_pAnim->SetDeleteCallback(FinishMeleeAnimCB, this); // 0x4CEBC0
                m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
            } else {
                m_pAnim->m_Flags &= ~ANIMATION_IS_LOOPED;
                m_pAnim->m_Flags &= ~ANIMATION_IS_SYNCRONISED;
                m_pAnim->m_Flags |= ANIMATION_IS_FINISH_AUTO_REMOVE;
                m_pAnim->SetFinishCallback(FinishMeleeAnimCB, this); // 0x4CEBE0
            }
            break;
        }
        case 11: case 12: case 13: case 14: { // Attacks
            const auto& combo = m_aComboData[m_nComboSet - 4];
            m_nCurrentMove    = (eFightAttackType)(int8)move;
            m_nContinueStrike = 1;
            m_pAnim = CAnimManager::BlendAnimation(clump, combo.m_nAnimGroup, (AnimationId)(ANIM_ID_FIGHT_1 + (int8)move), 8.f);
            m_pAnim->SetFinishCallback(FinishMeleeAnimCB, this); // 0x4CEBE0

            if (m_nCurrentMove == FIGHT_ATTACK_FIGHT_BLOCK) {
                // NOTE: The original checks for `!= 0` (NaN included)
                if (!(m_pAnim->m_CurrentTime == 0.f)) {
                    m_pAnim->SetCurrentTime(m_aComboData[m_nComboSet - 4].m_fGroundLoop); // 0x4CEA80
                }
            }

            if (ped->GetPlayerData()) {
                m_pAnim->m_Speed = CStats::GetFatAndMuscleModifier(STAT_MOD_3); // 0x559AF0
                if (m_nCurrentMove < 4) {
                    ped->SetMoveState(PEDMOVE_STILL); // 0x5DEC00
                }
            }
            break;
        }
        default: { // 1, 15..18
            if (auto* const pd = ped->GetPlayerData()) {
                pd->m_vecFightMovement = CVector2D{0.f, 0.f};
                SetPlayerMoveAnim(static_cast<CPlayerPed*>(ped)); // 0x61C9B0
                if (cmd == 0x11) {
                    pd->m_fMoveBlendRatio = 2.f;
                } else if (cmd == 0x10) {
                    pd->m_fMoveBlendRatio = 1.f;
                }
            } else {
                ped->SetMoveState(cmd == 0x11 ? PEDMOVE_RUN : cmd == 0x10 ? PEDMOVE_WALK : PEDMOVE_STILL);
                ped->m_nSwimmingMoveState = ped->m_nMoveState;
            }

            if (cmd == 0x11) {
                CAnimManager::BlendAnimation(clump, ped->m_nAnimGroup, ANIM_ID_RUN, 8.f);
            } else if (cmd == 0x10) {
                CAnimManager::BlendAnimation(clump, ped->m_nAnimGroup, ANIM_ID_WALK, 8.f);
            } else if (cmd == 0xF) {
                CAnimManager::BlendAnimation(clump, ped->m_nAnimGroup, ANIM_ID_IDLE, 4.f);
            } else if (cmd == 0x12 && ped->bIsDucking && ped->GetIntelligence()->GetTaskDuck(true)) { // 0x6010A0
                CAnimManager::BlendAnimation(clump, ANIM_GROUP_DEFAULT, ANIM_ID_WEAPON_CROUCH, 4.f);
            } else {
                CAnimManager::BlendAnimation(clump, ped->m_nAnimGroup, ANIM_ID_IDLE, 2.f);
            }

            if (m_pIdleAnim) {
                m_pIdleAnim->m_Flags &= ~ANIMATION_IS_PLAYING;
            } else {
                m_bIsFinished = true;
            }
            m_nNextCommand = 0x10;
            break;
        }
        }
    }

    m_nLastCommand = m_nNextCommand;
    m_nNextCommand = 0;
}

// 0x61C9B0 - Blends the walking-while-fighting anims according to the player's fight movement (stick) input
void CTaskSimpleFight::SetPlayerMoveAnim(CPlayerPed* player) {
    auto* const clump = player->GetRpClump();

    auto* fwd   = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_FIGHTSH_FWD);
    auto* left  = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_FIGHTSH_LEFT);
    auto* bwd   = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_FIGHTSH_BWD);
    auto* right = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_FIGHTSH_RIGHT);

    if (m_nNextCommand != 0 || m_nComboSet != 0) {
        const auto  mv  = player->GetPlayerData()->m_vecFightMovement;
        const double len = std::sqrt((double)mv.x * mv.x + (double)mv.y * mv.y); // x87
        if (!(len < (double)0.1f)) { // 0x858B1C (NaN normalises)
            // Normalise so that |x| + |y| == 1
            const double scale = (double)1.f / (std::fabs((double)mv.y) + std::fabs((double)mv.x)); // 0x858624
            const float  x     = (float)(scale * (double)mv.x);
            const float  y     = (float)(scale * (double)mv.y);

            if (x > 0.f) {
                if (left) {
                    left->m_BlendAmount = 0.f;
                }
                if (!right) {
                    right = CAnimManager::AddAnimation(clump, ANIM_GROUP_DEFAULT, ANIM_ID_FIGHTSH_RIGHT); // 0x4D3AA0
                }
                right->m_BlendAmount = x;
            } else if (x < 0.f) {
                if (right) {
                    right->m_BlendAmount = 0.f;
                }
                if (!left) {
                    left = CAnimManager::AddAnimation(clump, ANIM_GROUP_DEFAULT, ANIM_ID_FIGHTSH_LEFT);
                }
                left->m_BlendAmount = -x;
            }

            if (y < 0.f) {
                if (bwd) {
                    bwd->m_BlendAmount = 0.f;
                }
                if (!fwd) {
                    fwd = CAnimManager::AddAnimation(clump, ANIM_GROUP_DEFAULT, ANIM_ID_FIGHTSH_FWD);
                }
                fwd->m_BlendAmount = -y;
            } else if (y > 0.f) {
                if (fwd) {
                    fwd->m_BlendAmount = 0.f;
                }
                if (!bwd) {
                    bwd = CAnimManager::AddAnimation(clump, ANIM_GROUP_DEFAULT, ANIM_ID_FIGHTSH_BWD);
                }
                bwd->m_BlendAmount = y;
            }

            m_nComboSet    = 1;
            m_nLastCommand = m_nNextCommand;
            m_nNextCommand = 0;
            return;
        }
    }

    // No input => fade all of them out
    for (auto* const anim : {fwd, left, bwd, right}) {
        if (anim) {
            anim->m_BlendDelta = -8.f; // 0xC1000000
        }
    }
    m_nComboSet    = 0;
    m_nLastCommand = 0;
    m_nNextCommand = 0;
}

// 0x6239F0
bool CTaskSimpleFight::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority != ABORT_PRIORITY_URGENT && priority != ABORT_PRIORITY_IMMEDIATE) {
        m_nNextCommand = 1;
        return false;
    }

    if (event) {
        // NOTE: It's `GetEventPriority` (vtable +8), not `GetEventType`
        const auto evPriority = event->GetEventPriority();
        if (evPriority <= 31 || evPriority == 60) {
            return false;
        }
    }

    if (m_pAnim) {
        if (priority == ABORT_PRIORITY_IMMEDIATE) {
            m_pAnim->m_BlendDelta = -1000.f; // 0xC47A0000
        }
        m_pAnim->SetDefaultDeleteCallback();
        m_pAnim = nullptr;
    }

    if (m_pIdleAnim) {
        m_pIdleAnim->SetDefaultDeleteCallback();
        if (m_pIdleAnim->m_BlendAmount > 0.f && m_pIdleAnim->m_BlendDelta >= 0.f) { // 0x858B50 = 0.0f; `>` / `>=` (JNZ 0x41 / JNZ 0x1)
            CAnimManager::BlendAnimation(
                ped->GetRpClump(),
                ped->m_nAnimGroup,
                ANIM_ID_IDLE,
                priority == ABORT_PRIORITY_IMMEDIATE ? 1000.f : 16.f // 0x447A0000 / 0x41800000
            );
        }
        m_pIdleAnim = nullptr;
    }

    if (ped && ped->IsPlayer()) {
        ped->GetPlayerData()->m_vecFightMovement = CVector2D{ 0.f, 0.f };
        SetPlayerMoveAnim(static_cast<CPlayerPed*>(ped));
    }

    m_bIsFinished = true;
    return true;
}

// 0x629920
bool CTaskSimpleFight::ProcessPed(CPed* ped) {
    if (m_bIsFinished) {
        if (m_pIdleAnim) {
            m_pIdleAnim->SetDefaultDeleteCallback();
            if (m_pIdleAnim->m_BlendAmount > 0.f && m_pIdleAnim->m_BlendDelta >= 0.f) {
                CAnimManager::BlendAnimation(ped->GetRpClump(), ped->m_nAnimGroup, ANIM_ID_IDLE, 8.f); // 0x41000000
            }
            m_pIdleAnim = nullptr;
        }
        return true;
    }

    if (m_nComboSet == 0 || !m_bIsInControl) {
        // x87: 0.02 (0x858B38) and 1000.0 (0x858C4C) are applied in extended precision, then truncated (0x821B40)
        m_nIdleCounter += (int16)(int32)((double)CTimer::GetTimeStep() * (double)0.02f * (double)1000.0f);
    } else {
        m_nIdleCounter = 0;
    }

    if (!m_bIsInControl) {
        if ((int8)m_nNextCommand < 1) {
            return false;
        }
        if (m_pAnim && m_pAnim->m_AnimId == ANIM_ID_FIGHT2IDLE) {
            return false;
        }
    }

    if (!m_pIdleAnim) {
        switch (m_nLastCommand) {
        case 1:
        case 0xF:
        case 0x10:
        case 0x11:
            m_bIsFinished = true;
            break;
        default:
            if (!m_pAnim && ((int32)ped->m_nMoveState <= PEDMOVE_WALK || !ped->IsPlayer())) {
                const auto* const wi = CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, (eWeaponSkill)1);
                int8 comboSet = (int8)wi->m_nBaseCombo;
                if (comboSet == 4) {
                    comboSet = (int8)ped->m_nFightingStyle;
                }
                const auto& combo = m_aComboData[comboSet - 4];

                bool bUseSet = true; // false => fall back to the standard set
                if (!(combo.m_wFlags & 0x400)) { // (+0x85 & 4)
                    bUseSet = false;
                } else if (combo.m_nAnimGroup != 0x21) {
                    bool bGotAnims = true;
                    if (combo.m_nAnimGroup == m_nRequiredAnimGroup) {
                        bGotAnims = m_bAnimsReferenced;
                    } else {
                        if (m_bAnimsReferenced) {
                            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex(m_nRequiredAnimGroup)); // 0x4D3FD0
                            m_bAnimsReferenced = false;
                        }
                        m_nRequiredAnimGroup = combo.m_nAnimGroup;
                        bGotAnims = false;
                    }
                    if (!bGotAnims) {
                        auto* blk = CAnimManager::GetAnimationBlock(m_nRequiredAnimGroup);
                        if (!blk) {
                            blk = CAnimManager::GetAnimationBlock(CAnimManager::GetAnimBlockName(m_nRequiredAnimGroup));
                        }
                        const auto blkIdx = CAnimManager::GetAnimationBlockIndex(blk);
                        if (blk->IsLoaded) {
                            CAnimManager::AddAnimBlockRef(blkIdx);
                            m_bAnimsReferenced = true;
                        } else {
                            CStreaming::RequestModel(IFPToModelId(blkIdx), STREAMING_KEEP_IN_MEMORY); // 8
                            if ((int8)m_nNextCommand >= 11 && (int8)m_nNextCommand <= 14) {
                                m_nNextCommand = 11;
                            }
                            bUseSet = false;
                        }
                    }
                }
                if (!bUseSet) {
                    comboSet = 4;
                }

                m_nComboSet = comboSet;
                m_pIdleAnim = CAnimManager::BlendAnimation(
                    ped->GetRpClump(),
                    m_aComboData[comboSet - 4].m_nAnimGroup,
                    ANIM_ID_FIGHT_IDLE, // 0xDF
                    4.f // 0x40800000
                );
                m_pIdleAnim->SetDeleteCallback(FinishMeleeAnimCB, this);
                ped->SetMoveState(PEDMOVE_STILL);
                ped->m_nSwimmingMoveState = PEDMOVE_STILL; // (+0x538)
                m_nComboSet   = 0;
                m_nLastCommand = 0;
            }
            break;
        }
    }

    if (m_nRequiredAnimGroup != 0x21 && !m_bAnimsReferenced) {
        GetAvailableComboSet(ped, -1); // 0x61C7F0 (result unused)
    }

    if (!m_pAnim) {
        if (ped->IsPlayer() && m_nIdlePeriod < m_nIdleCounter && m_nNextCommand == 0 && m_nComboSet == 0) {
            m_nNextCommand = 1;
        }
        if ((m_nNextCommand != 0 || m_nComboSet != 0) && m_nLastCommand != 0x10) {
            m_nComboSet = GetAvailableComboSet(ped, (int8)m_nNextCommand); // 0x61C7F0
            if (ped->IsPlayer()) {
                if ((int8)m_nNextCommand >= 3 && (int8)m_nNextCommand <= 6) {
                    SetPlayerMoveAnim(static_cast<CPlayerPed*>(ped)); // 0x61C9B0 (Skips `StartAnim`)
                } else {
                    StartAnim(ped, ChooseAttackPlayer(ped)); // 0x624710, 0x623B10
                }
            } else {
                StartAnim(ped, ChooseAttackAI(ped)); // 0x624A40, 0x623B10
            }
        }
    } else {
        if (ped->GetActiveWeapon().m_Type == WEAPON_CHAINSAW) {
            ped->GetWeaponAE().AddAudioEvent(AE_WEAPON_CHAINSAW_ACTIVE); // 0x4E69F0
        }

        if (m_nComboSet < 4) {
            auto* const anim = m_pAnim;
            if (m_nLastCommand == 8 || m_nLastCommand == 10) {
                // x87: Division is done in extended precision
                if ((double)anim->m_CurrentTime / (double)anim->m_BlendHier->m_fTotalTime > (double)StaticRef<float>(0x8D2E94) // 0.4f
                 && anim->m_BlendDelta > -4.f // 0x859C3C
                ) {
                    anim->m_BlendDelta = -4.f; // 0xC0800000
                    goto tail;
                }
            }
            if (m_nLastCommand == 3) {
                if (m_nNextCommand == 3) {
                    m_nNextCommand = 0;
                } else if (anim->m_BlendDelta > -4.f) {
                    anim->m_BlendDelta = -4.f;
                }
            }
        } else {
            const auto& combo = m_aComboData[m_nComboSet - 4];
            if (m_nLastCommand == 2) {
                if (m_nNextCommand == 2) {
                    auto* const anim = m_pAnim;
                    if (anim->m_Flags & ANIMATION_IS_PLAYING) {
                        const double cur  = anim->m_CurrentTime;
                        const double next = (double)anim->m_TimeStep + cur;
                        if ((cur < combo.ABlockHit && combo.ABlockHit <= next)
                         || (cur < combo.ABlockChain && combo.ABlockChain <= next)
                        ) {
                            anim->m_Flags &= ~ANIMATION_IS_PLAYING;
                            m_pAnim->SetCurrentTime(combo.ABlockHit);
                        }
                    }
                } else {
                    auto* const anim = m_pAnim;
                    if (!(anim->m_Flags & ANIMATION_IS_PLAYING) && anim->m_BlendAmount > 0.f && anim->m_BlendDelta >= 0.f) {
                        anim->m_BlendDelta = -4.f;
                    }
                    if ((int8)m_nNextCommand >= 11) {
                        m_pAnim->SetDefaultDeleteCallback();
                        m_pAnim = nullptr;
                    }
                }
                if (m_nNextCommand == 2) {
                    m_nNextCommand = 0;
                }
            } else {
                auto* const anim = m_pAnim;
                if (anim->m_BlendAmount > 0.9f && anim->m_BlendDelta >= 0.f) { // 0x858C20
                    const int32  move = m_nCurrentMove;
                    const double cur  = anim->m_CurrentTime;
                    const double hit  = combo.m_fHit[move];
                    if (cur > hit && cur - (double)anim->m_TimeStep < hit) {
                        // The strike frame
                        ped->GetAE().AddAudioEvent(AE_PED_SWING, 0.f, 1.f, nullptr, (eSurfaceType)0, combo.m_Hit[move], 0); // 0x4E2BB0
                        if (m_nComboSet == 5 && m_nCurrentMove >= 0 && m_nCurrentMove <= 2) {
                            ped->GetAE().AddAudioEvent(AE_PED_SWING, 0.f, 1.f, nullptr, (eSurfaceType)0, combo.m_Hit[m_nCurrentMove], s_HitSoundMaxVolume[m_nCurrentMove]);
                        }
                        if ((int8)m_nNextCommand >= 11 && (int8)m_nNextCommand <= 14) {
                            m_nNextCommand = 0;
                        }
                        const auto hitLevel = combo.m_nHitLevel[m_nCurrentMove];
                        if (hitLevel != 7) {
                            CVector strikePos = TransformPointExt(*ped->m_matrix, m_aHitOffsets[hitLevel]); // 0x59C890
                            if (m_nCurrentMove == 4) {
                                strikePos += ped->m_vecMoveSpeed * CTimer::GetTimeStep(); // 0x40FEC0, 0x411A00
                            }
                            FightStrike(ped, strikePos);
                        }
                    } else if (anim->m_CurrentTime >= combo.m_fChain[move]
                        && (int8)m_nNextCommand >= 11 && (int8)m_nNextCommand <= 14
                    ) {
                        switch (m_nCurrentMove) {
                        case 0:
                        case 1: {
                            m_nComboSet = GetAvailableComboSet(ped, (int8)m_nNextCommand);
                            StartAnim(ped, ped->IsPlayer() ? ChooseAttackPlayer(ped) : m_nCurrentMove + 1);
                            break;
                        }
                        case 3: {
                            if (ped->IsPlayer() && ChooseAttackPlayer(ped) == 3) {
                                StartAnim(ped, 3);
                            }
                            break;
                        }
                        case 4: {
                            if (m_nComboSet == 12 && ped->IsPlayer()) {
                                m_pAnim->SetCurrentTime((float)((double)combo.m_fHit[m_nCurrentMove] - (double)0.01f)); // 0x858C58
                            } else {
                                m_nComboSet = GetAvailableComboSet(ped, (int8)m_nNextCommand);
                                StartAnim(ped, ped->IsPlayer() ? ChooseAttackPlayer(ped) : m_nCurrentMove + 1);
                            }
                            break;
                        }
                        }
                    }
                }
            }
        }
    }

tail:
    if (m_pTargetEntity) {
        // x87: The differences aren't rounded to float
        const auto& pedPos = ped->GetPosition();
        const auto& tgtPos = m_pTargetEntity->GetPosition();
        const double dx = (double)tgtPos.x - (double)pedPos.x;
        const double dy = (double)tgtPos.y - (double)pedPos.y;
        ped->m_fAimingRotation = (float)std::atan2(-dx, dy); // +0x55C
    } else if (ped->IsPlayer() && CCamera::m_bUseMouse3rdPerson && static_cast<CPlayerPed*>(ped)->GetPadFromPlayer()->GetTarget()) {
        const auto& v = StaticRef<CVector>(0xB6F32C);
        ped->m_fAimingRotation = (float)std::atan2(-(double)v.x, (double)v.y);
    }

    m_bIsInControl = false;
    return false;
}

// 0x61C650
bool CTaskSimpleFight::BeHitWhileBlocking(CPed* victim, CPed* creator, int8 comboSet, int8 move) {
    if (m_nLastCommand != 2) { // Not blocking
        return false;
    }

    auto* const anim = m_pAnim;
    if (!anim || (anim->m_Flags & ANIMATION_IS_PLAYING)) {
        return false;
    }

    // `!(total > current)` (NaN fails as well)
    if (!(anim->m_BlendHier->m_fTotalTime > anim->m_CurrentTime)) {
        return false;
    }

    // Is the attacker in front of the victim? (Dot product is accumulated in extended precision)
    // BUG: The original doesn't check for the victim's matrix, and would crash if it had none
    const auto& victimPos  = victim->GetPosition();
    const auto& creatorPos = creator->GetPosition();
    const auto& fwd        = victim->m_matrix->GetForward();
    const double dx = (double)creatorPos.x - (double)victimPos.x;
    const double dy = (double)creatorPos.y - (double)victimPos.y;
    const double dz = (double)creatorPos.z - (double)victimPos.z;
    const double dot = (dz * fwd.z + dy * fwd.y) + dx * fwd.x;
    const bool   bNotFacing = dot < (double)0.3f; // 0x858C24

    switch (comboSet) {
    case 8:
    case 10:
    case 11:
    case 12:
        if (m_nComboSet < 8 || m_nComboSet > 12 || m_nComboSet == 9) {
            return false;
        }
        break;
    case 9:
        if (m_nComboSet < 8 || m_nComboSet > 12) {
            return false;
        }
        break;
    case 7:
        if (move == 1) {
            return false;
        }
        break;
    }

    if (bNotFacing) {
        return false;
    }

    anim->m_Flags |= ANIMATION_IS_PLAYING;
    return true;
}

// Strike damage the way the original leaves it in st0 (the products are not rounded to float, and `FightHitPed` truncates them)
static double GetStrikeDamageExt(const CTaskSimpleFight& task, CPed* ped) {
    const double base = (double)CTaskSimpleFight::m_aComboData[task.m_nComboSet - 4].m_nDamage[task.m_nCurrentMove];

    if (ped->IsPlayer()) {
        if (ped->GetPlayerData()->m_bAdrenaline) {
            return 50.0; // 0x858B40
        }
        return base * (double)CStats::GetFatAndMuscleModifier(STAT_MOD_4);
    }

    switch (ped->GetActiveWeapon().m_Type) {
    case WEAPON_BRASSKNUCKLE: return base * (double)1.5f; // 0x858CE8
    case WEAPON_UNARMED:      return base * (double)ped->m_pStats->m_fAttackStrength;
    default:                  return base;
    }
}

// 0x61C740
float CTaskSimpleFight::GetStrikeDamage(CPed* ped) {
    return (float)GetStrikeDamageExt(*this, ped);
}

// 0x61CBA0
CPed* CTaskSimpleFight::FightHitPed(CPed* creator, CPed* victim, const CVector& point, const CVector& dir, int16 piece) {
    // Can't hit a player that's getting up
    // NOTE: The original doesn't check if there's an active task (there always is for the player)
    if (victim->IsPlayer() && victim->GetTaskManager().GetActiveTask()->GetTaskType() == TASK_SIMPLE_GET_UP) {
        return nullptr;
    }

    const auto& combo = m_aComboData[m_nComboSet - 4];

    // Blocked?
    if (const auto victimFight = victim->GetIntelligence()->GetTaskFighting()) {
        if (victimFight->BeHitWhileBlocking(victim, creator, m_nComboSet, m_nCurrentMove)) {
            creator->GetAE().AddAudioEvent((eAudioEvents)combo.m_AltHit[m_nCurrentMove], -9.f, 1.f, victim, SURFACE_DEFAULT, 0, 0); // 0xC1100000
            if (m_nComboSet == 5 && m_nCurrentMove >= 0 && m_nCurrentMove <= 2) {
                creator->GetAE().AddAudioEvent((eAudioEvents)combo.m_AltHit[m_nCurrentMove], -9.f, 1.f, victim, SURFACE_DEFAULT, 0, s_HitSoundMaxVolume[m_nCurrentMove]);
            }
            return nullptr;
        }
    }

    // (The original also calls `CWeaponInfo::GetWeaponInfo(<creator's weapon>, 1)` here, but doesn't use the result)

    const int32 damage = (int32)GetStrikeDamageExt(*this, creator); // 0x821B40 (ftol) => truncated

    m_nContinueStrike = -1;

    const auto side = CPedGeometryAnalyser::ComputePedShotSide(*victim, creator->GetPosition()); // 0x5F13F0
    const bool bHit = CWeapon::GenerateDamageEvent(victim, creator, creator->GetActiveWeapon().m_Type, damage, (ePedPieceTypes)piece, (uint8)side);

    if (creator->GetActiveWeapon().m_Type == WEAPON_CHAINSAW) {
        creator->GetWeaponAE().AddAudioEvent(AE_WEAPON_CHAINSAW_CUTTING); // 0x4E69F0
    }

    auto& creatorAE = creator->GetAE();
    if (side == 0) {
        if (m_nComboSet == 5 && m_nCurrentMove >= 0 && m_nCurrentMove <= 2) {
            creatorAE.AddAudioEvent((eAudioEvents)combo.m_Hit[m_nCurrentMove], 0.f, 1.f, victim, SURFACE_DEFAULT, 0, 0);
            creatorAE.AddAudioEvent((eAudioEvents)combo.m_Hit[m_nCurrentMove], 0.f, 1.f, victim, SURFACE_DEFAULT, 0, s_HitSoundMaxVolume[m_nCurrentMove]);
        } else if (m_nComboSet == 7 && m_nCurrentMove == FIGHT_ATTACK_HIT_2) {
            creatorAE.AddAudioEvent((eAudioEvents)combo.m_Hit[1], 0.f, 1.f, victim, SURFACE_DEFAULT, 0, s_HitSoundMaxVolume[1]);
            creatorAE.AddAudioEvent(
                (eAudioEvents)combo.m_Hit[m_nCurrentMove],
                0.f,
                1.f,
                victim,
                SURFACE_DEFAULT,
                0,
                (uint32)(int32)((double)s_HitSoundMaxVolume[m_nCurrentMove] * (double)2.8f) // 0x86D6A8 (ftol)
            );
        } else {
            creatorAE.AddAudioEvent((eAudioEvents)combo.m_Hit[m_nCurrentMove], 0.f, 1.f, victim, SURFACE_DEFAULT, 0, 0);
        }
    } else {
        creatorAE.AddAudioEvent((eAudioEvents)combo.m_AltHit[m_nCurrentMove], 0.f, 1.f, victim, SURFACE_DEFAULT, 0, 0);
        if (m_nComboSet == 5 && m_nCurrentMove >= 0 && m_nCurrentMove <= 2) {
            creatorAE.AddAudioEvent((eAudioEvents)combo.m_AltHit[m_nCurrentMove], 0.f, 1.f, victim, SURFACE_DEFAULT, 0, s_HitSoundMaxVolume[m_nCurrentMove]);
        }
    }

    if (creator->IsPlayer()) {
        creator->Say(CTX_GLOBAL_FIGHT, 0, 1.f, false, false, false);
    }

    // Make some noise
    CEventSoundQuiet event{creator, 55.f, (uint32)-1, CVector{}}; // 0x425C0000
    GetEventGlobalGroup()->Add(&event, false);

    // Blood
    const bool bIsHeavyMelee = m_nComboSet >= 8 && m_nComboSet <= 12;

    int32 bloodChance;
    if (bIsHeavyMelee) {
        bloodChance = 100;
    } else if (m_nComboSet == 4 && m_nCurrentMove == FIGHT_ATTACK_FIGHTIDLE) {
        bloodChance = -1;
    } else {
        bloodChance = (int32)((double)100.f - (double)victim->m_fHealth); // 0x858628 (ftol)
    }

    const auto roll = (int32)(((double)(CGeneral::GetRandomNumber() & 0xFFFF) * (double)(1.f / 32768.f)) * (double)100.f); // 0x858B14, 0x858628 (ftol)
    if (roll < bloodChance) {
        const CVector bloodPos = point;

        const auto& victimPos  = victim->GetPosition();
        const auto& creatorPos = creator->GetPosition();
        CVector     bloodDir{
            creatorPos.x - victimPos.x,
            creatorPos.y - victimPos.y,
            creatorPos.z - victimPos.z,
        };
        NormaliseExt(bloodDir);

        if (!victim->IsAlive()) {
            bloodDir = CVector{0.f, 0.f, 2.f};
        }

        int32 amount = 8;
        if (bIsHeavyMelee) {
            amount = 16;
            if (victim->IsAlive()) {
                bloodDir.x *= 1.5f; // 0x858CE8
                bloodDir.y *= 1.5f;
                bloodDir.z *= 1.5f;
            }
        }

        g_fx.AddBlood(bloodPos, bloodDir, amount, victim->m_fContactSurfaceBrightness);
    }

    return bHit ? victim : nullptr;
}

// 0x61D400
void CTaskSimpleFight::FightHitObj(CPed* ped, CObject* object, const CVector& point, const CVector& normal, int16 piece, int8 surface) {
    const CVector dir = normal; // The original works on a copy

    const float strikeDamage = GetStrikeDamage(ped);

    if (object->m_nColDamageEffect < 200
        && !object->physicalFlags.bDisableCollisionForce
        && object->m_pObjectInfo->m_fColDamageMultiplier < 99.9f // 0x86D6B0
    ) {
        // Wake it up if it was static (and could be uprooted)
        if (object->GetIsStatic() && object->m_pObjectInfo->m_fUprootLimit <= 0.f) { // 0x858B50 == 0.0f
            object->SetIsStatic(false);
            object->AddToMovingList();
        }

        if (!object->GetIsStatic()) {
            const float forceMult = object->physicalFlags.bDisableZ
                ? -0.1f  // 0x858EF4
                : -0.5f; // 0x858F40
            const auto& objPos = object->GetPosition();
            object->ApplyForce(
                CVector{dir.x * forceMult, dir.y * forceMult, dir.z * forceMult},
                CVector{point.x - objPos.x, point.y - objPos.y, point.z - objPos.z},
                true
            );
        }
    }

    object->ObjectDamage(strikeDamage * 10.f, &point, &dir, ped, ped->GetActiveWeapon().m_Type); // 0x85862C

    if (ped->GetActiveWeapon().m_Type == WEAPON_CHAINSAW) {
        ped->GetWeaponAE().AddAudioEvent(AE_WEAPON_CHAINSAW_CUTTING); // 0x4E69F0
    }

    ped->GetAE().AddAudioEvent(
        (eAudioEvents)m_aComboData[m_nComboSet - 4].m_Hit[m_nCurrentMove],
        0.f,
        1.f,
        object,
        (eSurfaceType)surface,
        0,
        0
    );

    g_fx.AddPunchImpact(point, dir, 4);
}

// 0x61D5F0
void CTaskSimpleFight::FightSetUpCol(float radius) {
    if (!s_FightColModel.m_pColData) {
        s_FightColModel.m_pColData    = &s_FightColData;
        s_FightColData.m_pSpheres     = &s_FightColSphere;
        s_FightColData.m_nNumSpheres  = 1;
    }

    s_FightColSphere.Set(radius, CVector{0.f, 0.f, 0.f}, SURFACE_DEFAULT, 0, tColLighting{0xFF});

    s_FightColModel.m_boundBox.m_vecMin = CVector{-radius, -radius, -radius};
    s_FightColModel.m_boundBox.m_vecMax = CVector{radius, radius, radius};
    s_FightColModel.m_boundSphere.m_vecCenter = CVector{0.f, 0.f, 0.f};
    s_FightColModel.m_boundSphere.m_fRadius   = radius;
}

// 0x61D0B0
void CTaskSimpleFight::FightHitCar(CPed* ped, CVehicle* vehicle, const CVector& point, const CVector& normal, int16 piece, int8 surface) {
    const float healthBefore = vehicle->m_fHealth;

    const float strikeDamage = GetStrikeDamage(ped);
    const auto  weaponType   = ped->GetActiveWeapon().m_Type;

    if (weaponType == WEAPON_CHAINSAW) {
        // Sparks along the hand bone's `at` vector
        const auto hier = GetAnimHierarchyFromSkinClump(ped->GetRpClump());
        const auto idx  = RpHAnimIDGetIndex(hier, BONE_R_HAND);
        const auto& mat = RpHAnimHierarchyGetMatrixArray(hier)[idx];
        g_fx.AddSparks(point, CVector{mat.at.x, mat.at.y, mat.at.z}, 5.f, 32, CVector{}, SPARK_PARTICLE_SPARK, 0.3f, 1.f);

        vehicle->VehicleDamage(
            (float)((double)vehicle->m_pHandlingData->m_fMass * (double)strikeDamage * (double)0.00075f), // 0x86D6AC
            (eVehicleCollisionComponent)piece,
            ped,
            const_cast<CVector*>(&point),
            const_cast<CVector*>(&normal),
            WEAPON_CHAINSAW
        );
    } else {
        vehicle->VehicleDamage(
            (float)((double)vehicle->m_pHandlingData->m_fMass * (double)strikeDamage * (double)0.01f), // 0x858C58
            (eVehicleCollisionComponent)piece,
            ped,
            const_cast<CVector*>(&point),
            const_cast<CVector*>(&normal),
            weaponType
        );
    }

    CCrime::ReportCrime(CRIME_HIT_CAR, vehicle, ped);

    // Let the occupants know
    if (vehicle->m_pDriver) {
        CEventVehicleDamageWeapon event{vehicle, ped, WEAPON_BASEBALLBAT};
        vehicle->m_pDriver->GetIntelligence()->m_eventGroup.Add(&event, false);
    }
    for (auto i = 0u; i < vehicle->m_nMaxPassengers; i++) { // NOTE: Loops up to `m_nMaxPassengers` (0x488), not `m_nNumPassengers`
        if (const auto passenger = vehicle->m_apPassengers[i]) {
            CEventVehicleDamageWeapon event{vehicle, ped, WEAPON_BASEBALLBAT};
            passenger->GetIntelligence()->m_eventGroup.Add(&event, false);
        }
    }

    if (vehicle->m_fHealth < healthBefore) {
        vehicle->m_nLastWeaponDamageType = (uint8)weaponType;
        vehicle->m_pLastDamageEntity     = ped;
        ped->RegisterReference(&vehicle->m_pLastDamageEntity);
    }

    if (ped->GetActiveWeapon().m_Type == WEAPON_CHAINSAW) {
        ped->GetWeaponAE().AddAudioEvent(AE_WEAPON_CHAINSAW_CUTTING); // 0x4E69F0
    }

    ped->GetAE().AddAudioEvent(
        (eAudioEvents)m_aComboData[m_nComboSet - 4].m_Hit[m_nCurrentMove],
        0.f,
        1.f,
        vehicle,
        (eSurfaceType)surface,
        0,
        0
    );

    g_fx.AddPunchImpact(point, normal, 4);
}

// 0x6240B0
bool CTaskSimpleFight::FightStrike(CPed* ped, CVector& pos) {
    // NOTE: A lot of the math here is done in extended precision by the original, hence the `double`s
    bool bNoPedHit = true;

    // (The original also calls `CWeaponInfo::GetWeaponInfo(<ped's weapon>, 1)` here, but doesn't use the result)

    const auto& combo = m_aComboData[m_nComboSet - 4];

    if (ped == FindPlayerPed(-1) && ped->GetActiveWeapon().m_Type != WEAPON_UNARMED) {
        CGlass::BreakGlassPhysically(pos, combo.m_fRadius[m_nCurrentMove]);
    }

    auto* const intel = ped->GetIntelligence();

    // Objects close to the strike
    int16                          numObjects = 0;
    std::array<CEntity*, 16>       objects{};
    CPed*                          hitPed     = nullptr;
    if (ped->IsPlayer()) {
        CWorld::FindObjectsInRange(pos, 5.f, true, &numObjects, (int16)objects.size(), objects.data(), false, false, false, true, false);
    }

    FightSetUpCol(combo.m_fRadius[m_nCurrentMove]);

    CMatrix hitMat{*ped->m_matrix};
    hitMat.SetTranslateOnly(pos);

    for (auto i = 0; i < numObjects + 32; i++) {
        CPed*     candPed = nullptr;
        CVehicle* candVeh = nullptr;
        CEntity*  candObj = nullptr;
        CEntity*  entity;
        if (i < 16) {
            entity = candPed = static_cast<CPed*>(intel->m_pedScanner.m_apEntities[i]);
        } else if (i < 32) {
            entity = candVeh = static_cast<CVehicle*>(intel->m_vehicleScanner.m_apEntities[i - 16]);
        } else {
            entity = candObj = objects[i - 32];
        }
        if (!entity) {
            continue;
        }

        // Max distance to the entity
        float maxDist = (float)((double)CModelInfo::GetModelInfo(entity->m_nModelIndex)->GetColModel()->GetBoundRadius() + (double)combo.m_fRadius[m_nCurrentMove]);

        if (candPed) {
            if (combo.m_nHitLevel[m_nCurrentMove] >= 4) {
                maxDist = (float)((double)combo.m_fRadius[m_nCurrentMove] * (double)0.5f + (double)maxDist);
            }

            // Skip peds that don't use collision, are alive, and aren't riding a bike
            if (!candPed->m_bUsesCollision
                && candPed->IsAlive()
                && !(candPed->bInVehicle && candPed->m_pVehicle && candPed->m_pVehicle->m_nVehicleType == VEHICLE_TYPE_BIKE)
            ) {
                continue;
            }
        } else if (candVeh) {
            if (candVeh->m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE) {
                continue;
            }
        } else if (!candObj->m_bUsesCollision) {
            continue;
        }

        // Is it close enough?
        double distSq;
        if (candPed) {
            const auto c  = candPed->GetBoundCentre();
            const auto dx = (double)c.x - (double)pos.x;
            const auto dy = (double)c.y - (double)pos.y;
            distSq = dx * dx + dy * dy;
        } else {
            const auto c  = entity->GetBoundCentre();
            const auto dx = (double)c.x - (double)pos.x;
            const auto dy = (double)c.y - (double)pos.y;
            const auto dz = (double)c.z - (double)pos.z;
            distSq = (dx * dx + dy * dy) + dz * dz;
        }
        if (!(distSq < (double)maxDist * (double)maxDist)) {
            continue;
        }

        if (candPed) {
            // Test against the ped's collision spheres
            const auto colModel = static_cast<CPedModelInfo*>(CModelInfo::GetModelInfo(candPed->m_nModelIndex))->AnimatePedColModelSkinnedWorld(candPed->GetRpClump());
            const auto colData  = colModel->m_pColData;

            for (auto tries = 0;;) {
                for (auto s = 0; s < (int16)colData->m_nNumSpheres; s++) {
                    const auto& sphere = colData->m_pSpheres[s];

                    const float  dxf = (float)((double)sphere.m_vecCenter.x - (double)pos.x);
                    const double dyd = (double)sphere.m_vecCenter.y - (double)pos.y;
                    const double dzd = (double)sphere.m_vecCenter.z - (double)pos.z;
                    const CVector diff{dxf, (float)dyd, (float)dzd};

                    const double sphereDistSq = ((double)dxf * (double)dxf + dyd * dyd) + dzd * dzd;
                    const double r            = (double)sphere.m_fRadius + (double)combo.m_fRadius[m_nCurrentMove];
                    if (sphereDistSq < r * r) {
                        if (const auto hit = FightHitPed(ped, candPed, pos, diff, 3)) {
                            hitPed = hit;
                        }
                        bNoPedHit = false;
                        goto NextEntity;
                    }
                }

                if (combo.m_nHitLevel[m_nCurrentMove] < 4) {
                    break;
                }

                // Nothing hit yet => move the strike position forward a bit, and try again (once)
                const auto&  fwd    = ped->m_matrix->GetForward();
                const double fx     = (double)fwd.x * (double)1.5f; // 0x858CE8
                const double fy     = (double)fwd.y * (double)1.5f;
                const double fz     = (double)fwd.z * (double)1.5f;
                const float  fxf    = (float)fx;
                const double radius = combo.m_fRadius[m_nCurrentMove];
                tries++;
                pos.x = (float)((double)fxf * radius + (double)pos.x);
                pos.y = (float)(fy * radius + (double)pos.y);
                const float zOffs = (float)(fz * radius);
                pos.z = (float)((double)zOffs + (double)pos.z);
                if (tries >= 2) {
                    break;
                }
            }
        } else {
            // Vehicle/object => collide against the strike's col. model
            entity->GetMatrix(); // Makes sure the matrix is allocated
            const auto colB   = entity->GetColModel();
            const auto numCPs = CCollision::ProcessColModels(hitMat, col1[0], *entity->m_matrix, *colB, CWorld::m_aTempColPts, nullptr, nullptr, false);
            if (numCPs > 0) {
                const auto& cp = CWorld::m_aTempColPts[0];
                if (candVeh) {
                    FightHitCar(ped, candVeh, cp.m_vecPoint, cp.m_vecNormal, (int16)cp.m_nPieceTypeB, (int8)cp.m_nSurfaceTypeB);
                } else if (candObj) {
                    FightHitObj(ped, static_cast<CObject*>(candObj), cp.m_vecPoint, cp.m_vecNormal, (int16)cp.m_nPieceTypeB, (int8)cp.m_nSurfaceTypeB);
                }
            }
        }
    NextEntity:;
    }

    // Nobody got hit => make some noise
    if (bNoPedHit && ped->IsPlayer()) {
        CEventSoundQuiet event{ped, 40.f, (uint32)-1, CVector{0.f, 0.f, 0.f}};
        GetEventGlobalGroup()->Add(&event, false);
    }

    // Stop the "ground attack" animation unless it hit someone who's also doing it
    if (m_nComboSet == 7 && m_nCurrentMove == FIGHT_ATTACK_HIT_2 && m_pAnim) {
        if (!hitPed || !RpAnimBlendClumpGetAssociation(hitPed->GetRpClump(), ANIM_ID_FIGHT_HIT_2)) {
            m_pAnim->m_BlendDelta = -4.f;
            m_pAnim->m_Flags &= ~ANIMATION_IS_PLAYING;
            m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
        }
    }

    // Last strike position
    ped->field_720 = std::bit_cast<int32>(pos.x);
    ped->field_724 = std::bit_cast<int32>(pos.y);
    ped->field_728 = std::bit_cast<int32>(pos.z);

    return false;
}

CTaskSimpleFight* CTaskSimpleFight::Constructor(CEntity* entity, int32 nCommand, uint32 nIdlePeriod) {
    this->CTaskSimpleFight::CTaskSimpleFight(entity, nCommand, nIdlePeriod);
    return this;
}

CTaskSimpleFight* CTaskSimpleFight::Destructor() {
    this->CTaskSimpleFight::~CTaskSimpleFight();
    return this;
}
