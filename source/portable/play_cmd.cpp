// MAC-PLAY1: gameplay smoke-test helpers for the native Mac build, reachable through the run-build input injector (NOTSA_STANDALONE_INPUT="...;cmd:poolstat;..."),
// called from AudioCmd (audio_cmd.cpp) which InjRunCmd already calls under NOTSA_PORTABLE. Portable-only file; runs on the main thread like every injector command.
//   cmd:poolstat          logs used / capacity of the entity, node and ped/vehicle/object pools ([play] line)
//   cmd:savemenu          what the script command ACTIVATE_SAVE_MENU does (the main loop then opens the save-game menu): m_bIsSaveDone = true, player velocity cleared
//   cmd:roadveh:MODEL     the player is put 3 m beside the closest vehicle path node, a car of MODEL (script-created, no driver) stands on the node pointing along its first link
//   cmd:pickup:WEAPON:AMMO a one-off weapon pickup 2 m in front of the player (walk onto it to collect it)
//   cmd:playerstat        logs player position / heading / camera mode / money / game clock / vehicle (what a save + load must preserve)
#include "StdInc.h"

#include <string>

#include "Camera.h"
#include "CarCtrl.h"
#include "Clock.h"
#include "PathFind.h"
#include "Pickups.h"
#include "Pools/Pools.h"
#include "World.h"
#include "portable/render/pipelines_gl3.h"

namespace notsa::portable {
template<typename PoolT>
static void LogPool(const char* name, PoolT* pool) {
    if (!pool) {
        NOTSA_LOG_INFO("[play] pool {}: null", name);
        return;
    }
    NOTSA_LOG_INFO("[play] pool {}: used {} / {}", name, (int)pool->GetNoOfUsedSpaces(), (int)pool->GetSize());
}

bool PlayCmd(const char* name, const std::string& args) {
    auto* pl = FindPlayerPed();
    if (!std::strcmp(name, "poolstat")) {
        LogPool("EntryInfoNode", GetEntryInfoNodePool());
        LogPool("PtrNodeSingle", GetPtrNodeSingleLinkPool());
        LogPool("PtrNodeDouble", GetPtrNodeDoubleLinkPool());
        LogPool("Ped", GetPedPool());
        LogPool("Vehicle", GetVehiclePool());
        LogPool("Object", GetObjectPool());
        LogPool("Building", GetBuildingPool());
        LogPool("Dummy", GetDummyPool());
        return true;
    }
    if (!std::strcmp(name, "skinstat")) { // R-10: gl3 skin pipeline counters
        NotsaGl3LogSkinStats();
        return true;
    }
    if (!std::strcmp(name, "savemenu")) {
        FrontEndMenuManager.m_bIsSaveDone = true;
        if (pl) {
            pl->m_vecMoveSpeed = CVector{};
            pl->m_vecTurnSpeed = CVector{};
        }
        NOTSA_LOG_INFO("[play] savemenu requested (m_bIsSaveDone = 1)");
        return true;
    }
    if (!std::strcmp(name, "playerstat")) {
        if (!pl) {
            NOTSA_LOG_INFO("[play] playerstat: no player");
            return true;
        }
        const auto p = pl->GetPosition();
        const auto& cam = TheCamera.m_aCams[TheCamera.m_nActiveCam];
        NOTSA_LOG_INFO("[play] camstat mode={} dist={:.2f} dz={:.2f} fov={:.1f}", (int)cam.m_nMode, (TheCamera.GetPosition() - p).Magnitude(), TheCamera.GetPosition().z - p.z, cam.m_fFOV);
        NOTSA_LOG_INFO("[play] playerstat pos=({:.2f},{:.2f},{:.2f}) heading={:.3f} hp={:.0f} arm={:.0f} money={} clock={:02}:{:02} weapon={} ammo={} inveh={} model={}", p.x, p.y, p.z, pl->GetHeading(),
                       pl->m_fHealth, pl->m_fArmour, FindPlayerInfo().m_nMoney, (int)CClock::GetGameClockHours(), (int)CClock::GetGameClockMinutes(), (int)pl->GetActiveWeapon().GetType(),
                       (int)pl->GetActiveWeapon().GetTotalAmmo(), (int)pl->bInVehicle, (int)pl->m_nModelIndex);
        return true;
    }
    if (!std::strcmp(name, "pickup")) {
        int w = (int)WEAPON_AK47, n = 50;
        std::sscanf(args.c_str(), "%d:%d", &w, &n);
        if (pl) {
            auto pos = pl->GetPosition() + pl->GetForward() * 2.f;
            bool found = false;
            const auto gz = CWorld::FindGroundZFor3DCoord({ pos.x, pos.y, pos.z + 2.f }, &found, nullptr);
            if (found) { pos.z = gz + 0.7f; }
            CStreaming::RequestModel(CWeaponInfo::GetWeaponInfo((eWeaponType)w)->m_nModelId1, STREAMING_GAME_REQUIRED);
            CStreaming::LoadAllRequestedModels(false);
            const auto ref = CPickups::GenerateNewOne_WeaponType(pos, (eWeaponType)w, PICKUP_ONCE, (uint32)n, false, nullptr);
            NOTSA_LOG_INFO("[play] pickup weapon {} ammo {} at ({:.1f},{:.1f},{:.1f}) ref {}", w, n, pos.x, pos.y, pos.z, (int)ref.index);
        }
        return true;
    }
    if (!std::strcmp(name, "roadveh")) {
        if (!pl) {
            return true;
        }
        const int model = std::atoi(args.c_str());
        auto&     paths = ThePaths;
        const auto addr = paths.FindNodeClosestToCoors(pl->GetPosition(), PATH_TYPE_VEH, 200.f);
        if (addr.m_wAreaId == (uint16)-1 || !paths.IsAreaNodesAvailable(addr)) {
            NOTSA_LOG_INFO("[play] roadveh: no vehicle node near the player");
            return true;
        }
        const auto* node = paths.GetPathNode(addr);
        const CVector np = node->GetPosition();
        CVector dir{ 1.f, 0.f, 0.f };
        for (auto& linked : paths.GetNodeLinkedNodes(*node)) {
            const CVector d = linked.GetPosition() - np;
            if (d.Magnitude2D() > 5.f) {
                dir = d;
                break;
            }
        }
        dir.z = 0.f;
        dir.Normalise();
        const CVector side{ -dir.y, dir.x, 0.f };
        const CVector carPos = np;
        const CVector plPos  = np + side * 3.f;
        CStreaming::LoadScene(np);
        CStreaming::LoadSceneCollision(np);
        pl->Teleport(plPos, false);
        CStreaming::LoadScene(np);
        CStreaming::RequestModel((eModelID)model, STREAMING_GAME_REQUIRED);
        CStreaming::LoadAllRequestedModels(false);
        bool  found = false;
        auto  pos   = carPos;
        const auto gz = CWorld::FindGroundZFor3DCoord({ pos.x, pos.y, pos.z + 2.f }, &found, nullptr);
        pos.z         = (found ? gz : pos.z) + 1.f;
        auto* veh     = CCarCtrl::CreateCarForScript(model, pos, false);
        if (veh) {
            veh->SetHeading(std::atan2(-dir.x, dir.y)); // heading 0 = +Y, increasing counter-clockwise
        }
        NOTSA_LOG_INFO("[play] roadveh model {} veh {} node ({:.0f},{:.0f},{:.0f}) dir ({:.2f},{:.2f})", model, (void*)veh, np.x, np.y, np.z, dir.x, dir.y);
        return true;
    }
    return false;
}
} // namespace notsa::portable
