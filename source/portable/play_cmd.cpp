// MAC-PLAY1: gameplay smoke-test helpers for the native Mac build, reachable through the run-build input injector (NOTSA_STANDALONE_INPUT="...;cmd:poolstat;..."),
// called from AudioCmd (audio_cmd.cpp) which InjRunCmd already calls under NOTSA_PORTABLE. Portable-only file; runs on the main thread like every injector command.
//   cmd:poolstat          logs used / capacity of the entity, node and ped/vehicle/object pools ([play] line)
//   cmd:savemenu          what the script command ACTIVATE_SAVE_MENU does (the main loop then opens the save-game menu): m_bIsSaveDone = true, player velocity cleared
//   cmd:lighting:old|new     R-12: gl3 lighting model at run time
//   cmd:interior:X:Y:Z:AREA  R-12: the player into an interior (CGame::currArea + area code + streaming)
//   cmd:roadveh:MODEL     the player is put 3 m beside the closest vehicle path node, a car of MODEL (script-created, no driver) stands on the node pointing along its first link
//   cmd:pickup:WEAPON:AMMO a one-off weapon pickup 2 m in front of the player (walk onto it to collect it)
//   cmd:weather:ID        R-11: CWeather::ForceWeatherNow(ID) (4 rainy, 8 foggy, 9 thunderstorm, 10 extra sunny...)
//   cmd:fx:NAME[:0|1]     R-11: post effect switches: night, ir, grain, cctv, haze, radiosity, darkness, speed (default 1)
//   cmd:playerstat        logs player position / heading / camera mode / money / game clock / vehicle (what a save + load must preserve)
#include "StdInc.h"

#include <string>

#include "Camera.h"
#include "CarCtrl.h"
#include "Clock.h"
#include "PostEffects.h"
#include "StencilShadows.h"
#include "Weather.h"
#include "PathFind.h"
#include "Pickups.h"
#include "Pools/Pools.h"
#include "World.h"
#include "Draw.h"
#include "Renderer.h"
#include "portable/render/pipelines_gl3.h"

namespace rw::gl3 { extern rw::int32 lightingModel; } // librw gl3 (vendor/librw/src/gl/gl3device.cpp): 1 = D3D9 FF lighting order (R-12), 0 = librw's original

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
    if (!std::strcmp(name, "weather")) { // R-11: cmd:weather:ID
        CWeather::ForceWeatherNow((eWeatherType)std::atoi(args.c_str()));
        NOTSA_LOG_INFO("[play] weather forced to {}", std::atoi(args.c_str()));
        return true;
    }
    if (!std::strcmp(name, "gfxq")) { // R-11: cmd:gfxq  logs what GraphicsHighQuality() (stencil shadows, pole shadows, vehicle blobs) is derived from
        auto* camRas = RwCameraGetRaster(Scene.m_pRwCamera);
        NOTSA_LOG_INFO("[play] gfxq: GraphicsHighQuality={} fxQuality={} camera raster depth={} type={} format=0x{:x}", (int)GraphicsHighQuality(), (int)g_fx.GetFxQuality(), (int)RwRasterGetDepth(camRas), (int)camRas->type, (unsigned)camRas->format);
        int nStencil = 0;
        for (auto* o = CStencilShadows::pFirstActiveStencilShadowObject; o && nStencil < 100; o = o->m_pNext) { nStencil++; }
        NOTSA_LOG_INFO("[play] gfxq: active stencil shadow objects {}", nStencil);
        {
            int nVis = 0, nVisBld = 0, nVisA3 = 0;
            for (auto* e : CRenderer::GetVisibleEntityPtrs()) {
                nVis++; nVisBld += e->GetIsTypeBuilding(); nVisA3 += (int)e->GetAreaCode() == 3;
                if (nVis <= 6) {
                    auto* ro = e->GetRwObject();
                    int nAtomics = 0, nTris = 0;
                    if (ro && RwObjectGetType(ro) == rpATOMIC) { nAtomics = 1; nTris = RpAtomicGetGeometry((RpAtomic*)ro)->numTriangles; }
                    else if (ro && RwObjectGetType(ro) == rpCLUMP) { RpClumpForAllAtomics((RpClump*)ro, [](RpAtomic* a, void* d) { ((int*)d)[0]++; ((int*)d)[1] += RpAtomicGetGeometry(a)->numTriangles; return a; }, &nAtomics); }
                    NOTSA_LOG_INFO("[play] gfxq: visible #{} model {} entType {} modelType {} rwType {} atomics {} tris {} pos ({:.1f},{:.1f},{:.1f})", nVis, (int)e->m_nModelIndex, (int)e->GetType(), (int)CModelInfo::GetModelInfo(e->m_nModelIndex)->GetModelType(), ro ? (int)RwObjectGetType(ro) : -1, nAtomics, nTris, e->GetPosition().x, e->GetPosition().y, e->GetPosition().z);
                }
            }
            NOTSA_LOG_INFO("[play] gfxq: visible entities {} (buildings {}, area 3: {}), CGame::currArea {}", nVis, nVisBld, nVisA3, (int)CGame::currArea);
            if (CGame::currArea != 0) { // R-11: why are the interior's buildings not in the render list? (results of CRenderer::SetupEntityVisibility per building within 25 m of the player)
                int hist[8] = {}, inArea = 0, onScreen = 0, visible = 0, rw = 0, occl = 0, n = 0;
                const auto ppos = FindPlayerPed() ? FindPlayerPed()->GetPosition() : CVector{};
                for (auto& b : GetBuildingPool()->GetAllValid()) {
                    if ((b.GetPosition() - ppos).Magnitude() > 25.f) { continue; }
                    n++;
                    float d = 0.f;
                    const auto r = CRenderer::SetupEntityVisibility(&b, d);
                    hist[std::clamp(r, 0, 7)]++;
                    if (r == RENDERER_CULLED && n <= 40) {
                        static int dumped = 0;
                        if (dumped++ < 6) {
                            CVector pp = b.GetBoundCentre();
                            RwV3dTransformPoints(&pp, &pp, 1, (RwMatrix*)&TheCamera.m_mMatInverse);
                            const auto& fn = TheCamera.m_avecFrustumNormals;
                            NOTSA_LOG_INFO("[play] gfxq: culled bld model {} centre ({:.1f},{:.1f},{:.1f}) r {:.1f} -> camspace ({:.1f},{:.1f},{:.1f}) near {:.2f} far {:.0f} normals ({:.2f},{:.2f}) ({:.2f},{:.2f}) ({:.2f},{:.2f}) ({:.2f},{:.2f})",
                                           (int)b.m_nModelIndex, b.GetBoundCentre().x, b.GetBoundCentre().y, b.GetBoundCentre().z, CModelInfo::GetModelInfo(b.m_nModelIndex)->GetColModel()->GetBoundRadius(), pp.x, pp.y, pp.z, CDraw::ms_fNearClipZ, CDraw::ms_fFarClipZ,
                                           fn[0].x, fn[0].y, fn[1].x, fn[1].y, fn[2].z, fn[2].y, fn[3].z, fn[3].y);
                        }
                    }
                    inArea += b.IsInCurrentArea(); onScreen += b.GetIsOnScreen(); visible += b.GetIsVisible(); rw += b.GetRwObject() != nullptr; occl += b.IsEntityOccluded();
                }
                NOTSA_LOG_INFO("[play] gfxq: interior buildings {}: visibility enum counts [0]={} [1]={} [2]={} [3]={} [4]={}; inArea {} onScreen {} isVisible {} rwObject {} occluded {}; camera ({:.1f},{:.1f},{:.1f})",
                               n, hist[0], hist[1], hist[2], hist[3], hist[4], inArea, onScreen, visible, rw, occl, CRenderer::ms_vecCameraPosition.x, CRenderer::ms_vecCameraPosition.y, CRenderer::ms_vecCameraPosition.z);
            }
        }
        return true;
    }
    if (!std::strcmp(name, "fx")) { // R-11: cmd:fx:NAME[:0|1]  post effect switches (the game re-derives most of them each frame from camera / weather, so the ones set here persist only for the pure flags)
        char fx[32] = {};
        int  on = 1;
        std::sscanf(args.c_str(), "%31[^:]:%d", fx, &on);
        const bool b = on != 0;
        if (!std::strcmp(fx, "night")) { CPostEffects::m_bNightVision = b; }
        else if (!std::strcmp(fx, "ir")) { CPostEffects::m_bInfraredVision = b; }
        else if (!std::strcmp(fx, "grain")) { CPostEffects::m_bGrainEnable = b; }
        else if (!std::strcmp(fx, "cctv")) { CPostEffects::m_bCCTV = b; }
        else if (!std::strcmp(fx, "haze")) { CPostEffects::m_bHeatHazeFX = b; }
        else if (!std::strcmp(fx, "radiosity")) { CPostEffects::m_bRadiosity = b; }
        else if (!std::strcmp(fx, "darkness")) { CPostEffects::m_bDarknessFilter = b; }
        else if (!std::strcmp(fx, "speed")) { CPostEffects::m_bSpeedFX = b; }
        else if (!std::strcmp(fx, "color")) { CPostEffects::m_bColorEnable = b; }
        else { NOTSA_LOG_INFO("[play] fx: unknown '{}'", fx); }
        NOTSA_LOG_INFO("[play] fx {} = {}", fx, (int)b);
        return true;
    }
    if (!std::strcmp(name, "lighting")) { // R-12: cmd:lighting:old|new  switches the gl3 vertex-shader lighting model at run time (A/B frames of one scene)
        rw::gl3::lightingModel = args.find("old") != std::string::npos ? 0 : 1;
        NOTSA_LOG_INFO("[play] lighting model = {}", (int)rw::gl3::lightingModel);
        return true;
    }
    if (!std::strcmp(name, "interior")) { // R-12: cmd:interior:X:Y:Z:AREA  puts the player into an interior (what SET_AREA_VISIBLE + SET_CHAR_AREA_VISIBLE + SET_CHAR_COORDINATES do)
        float x = 0, y = 0, z = 0;
        int   area = 0;
        std::sscanf(args.c_str(), "%f:%f:%f:%d", &x, &y, &z, &area);
        if (pl) {
            const CVector pos{ x, y, z };
            CGame::currArea = (eAreaCodes)area;
            pl->SetAreaCode((eAreaCodes)area);
            CStreaming::RemoveBuildingsNotInArea((eAreaCodes)area);
            pl->Teleport(pos, false);
            CStreaming::LoadScene(pos);
            CStreaming::LoadSceneCollision(pos);
            CStreaming::LoadAllRequestedModels(false);
            {
                int nb = 0, nbArea = 0, nbLoaded = 0;
                for (auto& b : GetBuildingPool()->GetAllValid()) {
                    if ((b.GetPosition() - pos).Magnitude() > 25.f) { continue; }
                    nb++;
                    nbArea += (int)b.GetAreaCode() == area;
                    nbLoaded += b.GetRwObject() != nullptr;
                }
                NOTSA_LOG_INFO("[play] interior: {} buildings within 25 m, {} with area code {}, {} with an RwObject (streamed in)", nb, nbArea, area, nbLoaded);
            }
            TheCamera.RestoreWithJumpCut(); // R-11: the camera still sat at the outside position (frames showed 3 shelf boards in a black void)
            NOTSA_LOG_INFO("[play] interior area {} at ({:.1f},{:.1f},{:.1f}), camera {:.1f} {:.1f} {:.1f}", area, x, y, z, TheCamera.GetPosition().x, TheCamera.GetPosition().y, TheCamera.GetPosition().z);
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
