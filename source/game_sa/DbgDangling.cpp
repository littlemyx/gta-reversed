#include "StdInc.h"
#ifdef NOTSA_INPUT_INJECT
#include "DbgDangling.h"
#include "InterestingEvents.h"
#include "EntryExitManager.h"
#include "References.h"
#include "IdleCam.h"
#include "Audio/Managers/AESoundManager.h"
#include <string>

namespace dbg3 {
template<typename P>
static bool Find(P* pool, const void* p, const char* name, std::string& out) {
    if (!pool) return false;
    for (auto& e : pool->GetAllValid()) {
        const auto b = (const uint8*)&e;
        if ((const uint8*)p >= b && (const uint8*)p < b + sizeof(e)) {
            out = std::format("{} idx={} model={} off={:#x}", name, pool->GetIndex(&e), (int)e.m_nModelIndex, (uint32)((const uint8*)p - b));
            return true;
        }
    }
    return false;
}
std::string Classify(const void* p) {
    std::string s;
    if (Find(GetPedPool(), p, "ped", s) || Find(GetVehiclePool(), p, "vehicle", s) || Find(GetObjectPool(), p, "object", s)) return s;
    return "other";
}

void PeriodicReport() {
    static uint32 s_Last = 0;
    const auto now = CTimer::GetTimeInMS();
    if (now - s_Last < 10'000) return;
    s_Last = now;
    uint32 freeRefs = CReferences::ListSize(CReferences::pEmptyList);
    uint32 enex = 0;
    if (auto* pool = CEntryExitManager::GetPool()) for (auto& e : pool->GetAllValid()) { (void)e; ++enex; }
    uint32 peds = 0, vehs = 0, objs = 0;
    if (GetPedPool()) for (auto& e : GetPedPool()->GetAllValid()) { (void)e; ++peds; }
    if (GetVehiclePool()) for (auto& e : GetVehiclePool()->GetAllValid()) { (void)e; ++vehs; }
    if (GetObjectPool()) for (auto& e : GetObjectPool()->GetAllValid()) { (void)e; ++objs; }
    {
        int live = 0; std::string types;
        for (auto& ev : g_InterestingEvents.m_Events) { if (ev.entity) { ++live; types += std::format("{}:{} ", ev.type, (void*)ev.entity); } }
        NOTSA_LOG_ERR("DBG3 interesting events: m_b1={} live={} cur={} [{}] idleTicker={}", (int)g_InterestingEvents.m_b1, live, (int)g_InterestingEvents.m_nInterestingEvent, types, gIdleCam.m_IdleTickerFrames);
    }
    NOTSA_LOG_ERR("DBG3 t={}s: free CReferences={}/3000, enex pool used={}/400, peds={} vehicles={} objects={}", now / 1000, freeRefs, enex, peds, vehs, objs);
}

template<typename T, typename U>
static int FreedSlot(CPool<T, U>* pool, const void* p) { // slot index when `p` lies in a FREE slot of the pool, else -1
    if (!pool) return -1;
    auto* t = (const T*)p;
    if (!pool->IsPtrFromPool(t)) return -1;
    const auto idx = pool->GetIndex(t);
    return pool->IsFreeSlotAtIndex(idx) ? (int)idx : -1;
}
static std::string FreedWhere(const void* p) {
    if (int i = FreedSlot(GetPedPool(), p); i >= 0)     return std::format("FREE ped slot {}", i);
    if (int i = FreedSlot(GetVehiclePool(), p); i >= 0) return std::format("FREE vehicle slot {}", i);
    if (int i = FreedSlot(GetObjectPool(), p); i >= 0)  return std::format("FREE object slot {}", i);
    return {};
}

void CheckSounds() {
    static std::array<const void*, 300> s_Reported{};
    for (size_t i = 0; i < 300; ++i) {
        auto& snd = AESoundManager.m_VirtuallyPlayingSoundList[i];
        if (!snd.IsActive() || !snd.m_AudioEntity || !snd.GetRequestUpdates()) continue;
        const auto w = FreedWhere(snd.m_AudioEntity);
        if (w.empty() || s_Reported[i] == snd.m_AudioEntity) continue;
        s_Reported[i] = snd.m_AudioEntity;
        const auto& sh = g_sndShadow[i];
        NOTSA_LOG_ERR("DBG3 SOUND OWNED BY FREED OBJECT: sound slot={} event={} bank={} sound={} owner={} ({}) vtbl@add={:#x} class@add=[{}] addedMs={} nowMs={}", i, snd.m_Event, (int)snd.m_BankSlot, (int)snd.m_SoundID, (void*)snd.m_AudioEntity, w, sh.vtbl, sh.cls, sh.ms, CTimer::GetTimeInMS());
    }
}

void CheckEvents() {
    PeriodicReport();
    CheckSounds();
    auto& ie = g_InterestingEvents;
    for (int i = 0; i < MAX_INTERESTING_EVENTS; ++i) {
        auto& ev = ie.m_Events[i];
        if (!ev.entity) continue;
        if (const auto w = FreedWhere(ev.entity); !w.empty()) {
            NOTSA_LOG_ERR("DBG3 DANGLING interesting event #{} (entity slot already freed: {}): entity={} type={} time={} shadow(etype={} model={} addedMs={}) nowMs={}", i, w, (void*)ev.entity, ev.type, ev.time, g_evShadow[i].etype, g_evShadow[i].model, g_evShadow[i].timeMs, CTimer::GetTimeInMS());
            ev.entity = nullptr; ev.type = 0; continue;
        }
        { // latent: a live pointer without a CReferences entry will dangle as soon as the entity dies
            bool reg = false;
            for (auto* r = ev.entity->m_pReferences; r; r = r->m_pNext) { reg |= r->m_ppEntity == &ev.entity; }
            static std::array<const void*, 8> s_Lat{};
            if (!reg && s_Lat[i] != ev.entity) { s_Lat[i] = ev.entity; NOTSA_LOG_ERR("DBG3 LATENT dangling: interesting event #{} holds entity {} (type {}) without a CReferences entry (ev.type={}, time={}, nowMs={})", i, (void*)ev.entity, (int)ev.entity->GetType(), ev.type, ev.time, CTimer::GetTimeInMS()); }
        }
        if (*(const uint32*)ev.entity != 0xDDDDDDDDu) continue;
        const auto& sh = g_evShadow[i];
        const auto* r = FindResolved(ev.entity);
        NOTSA_LOG_ERR("DBG3 DANGLING interesting event #{}: entity={} type={} time={} shadow(e={} etype={} model={} addedFrame={} addedMs={}) now frame={} ms={}; resolved-before={} (frame {}, etype {}, model {}); events.m_b1={} cur={}; free CReferences={}",
            i, (void*)ev.entity, ev.type, ev.time, (const void*)sh.e, sh.etype, sh.model, sh.frame, sh.timeMs, CTimer::GetFrameCounter(), CTimer::GetTimeInMS(),
            r != nullptr, r ? (int)r->frame : -1, r ? r->type : -1, r ? r->model : -1, (int)ie.m_b1, (int)ie.m_nInterestingEvent, CReferences::ListSize(CReferences::pEmptyList));
        ev.entity = nullptr; ev.type = 0; // keep running
    }
}
}
#endif
