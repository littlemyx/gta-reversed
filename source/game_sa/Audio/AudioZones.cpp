#include "StdInc.h"

#include "AudioZones.h"

void CAudioZones::InjectHooks() {
    RH_ScopedClass(CAudioZones);
    RH_ScopedCategory("Audio");

    RH_ScopedInstall(Init, 0x5081A0);
    RH_ScopedInstall(RegisterAudioSphere, 0x5081C0);
    RH_ScopedInstall(RegisterAudioBox, 0x508240);
    RH_ScopedInstall(SwitchAudioZone, 0x508320);
    RH_ScopedInstall(Update, 0x5083C0);
}

// 0x5081A0
void CAudioZones::Init() {
    ZoneScoped;

    m_NumSpheres = 0;
    m_NumBoxes = 0;
    m_NumActiveSpheres = 0;
    m_NumActiveBoxes = 0;
}

// 0x508240
void CAudioZones::RegisterAudioBox(char name[8], int32 id, bool isActive, CVector min, CVector max) {

    tAudioZoneBox audioZoneBox;
    strncpy(audioZoneBox.m_szName, name, sizeof(audioZoneBox.m_szName)); // exe 0x508260: unbounded strcpy into name[8]; its NUL spill lands in a field that is stored right after. An 8 char name aborted strcpy_s
    audioZoneBox.m_IsActive = isActive; // TODO: m_nFlags field has only 1 flag - Active or inactive and takes only 1 bit. Although gta uses 2 bytes for this, but how is the idea to define this single flag so as not to be confused in the future
    audioZoneBox.m_nAudioZone = id;
    audioZoneBox.m_Box = CompressedBox{ .m_vecMin = min, .m_vecMax = max };
    m_aBoxes[m_NumBoxes++] = audioZoneBox;
}

// 0x5081C0
void CAudioZones::RegisterAudioSphere(char name[8], int32 id, bool isActive, CVector position, float radius) {
    tAudioZoneSphere audioZoneSphere;
    strncpy(audioZoneSphere.m_szName, name, sizeof(audioZoneSphere.m_szName)); // exe 0x5081D5: unbounded strcpy, see RegisterAudioBox
    audioZoneSphere.m_nAudioZone = id;
    audioZoneSphere.m_IsActive = isActive; // TODO: m_nFlags field has only 1 flag - Active or inactive and takes only 1 bit. Although gta uses 2 bytes for this, but how is the idea to define this single flag so as not to be confused in the future
    audioZoneSphere.m_Sphere = {position, radius};

    m_aSpheres[m_NumSpheres++] = audioZoneSphere;
}

// 0x508320
void CAudioZones::SwitchAudioZone(const char* zoneName, bool enable) {
    for (auto& sphere : GetAvailableSpheres()) {
        if (!_stricmp(zoneName, sphere.m_szName))
            sphere.m_IsActive = enable;
    }

    for (auto& box : GetAvailableBoxes()) {
        if (!_stricmp(zoneName, box.m_szName))
            box.m_IsActive = enable;
    }
}

// 0x5083C0
void CAudioZones::Update(bool forceUpdate, CVector posn) {
    ZoneScoped;

    static CVector LastUpdateCoors{};

    // TODO: FPS-independent counter
    // 0x5083C0: the distance is `sqrt(dz*dz + dy*dy + dx*dx)` summed z,y,x on unrounded x87 values (fsub/fmul/fsqrt stay in registers) and compared with `<` (NaN continues)
    const auto DistZYX = [](const CVector& a, const CVector& b) -> x87d {
        const x87d dx = (x87d)a.x - (x87d)b.x, dy = (x87d)a.y - (x87d)b.y, dz = (x87d)a.z - (x87d)b.z;
        return std::sqrt(((dz * dz) + (dy * dy)) + (dx * dx));
    };
    if (!forceUpdate && (CTimer::m_FrameCounter % 16) != 6 && DistZYX(posn, LastUpdateCoors) < (x87d)20.0f)
        return;

    LastUpdateCoors = posn;
    m_NumActiveSpheres = m_NumActiveBoxes = 0;

    for (auto&& [idx, sphere] : rngv::enumerate(GetAvailableSpheres())) {
        // Do not add more to list if it's filled.
        if (m_NumActiveSpheres >= std::size(m_aActiveSpheres))
            break;

        if (!sphere.m_IsActive)
            continue;

        // 0x508465: dist(z,y,x order, unrounded) < radius, strict
        if (DistZYX(posn, sphere.m_Sphere.m_vecCenter) < (x87d)sphere.m_Sphere.m_fRadius)
            m_aActiveSpheres[m_NumActiveSpheres++] = idx;
    }

    for (auto&& [idx, box] : rngv::enumerate(GetAvailableBoxes())) {
        // Do not add more to list if it's filled.
        if (m_NumActiveBoxes >= std::size(m_aActiveBoxes))
            break;

        if (!box.m_IsActive)
            continue;

        // 0x5084D1: min < p < max on every axis, both strict (`test ah,5` / `test ah,0x41`); the exe takes the sphere/box limit AFTER the test (same result)
        const CBox b = box.m_Box;
        if (b.m_vecMin.x < posn.x && b.m_vecMax.x > posn.x
         && b.m_vecMin.y < posn.y && b.m_vecMax.y > posn.y
         && b.m_vecMin.z < posn.z && b.m_vecMax.z > posn.z)
            m_aActiveBoxes[m_NumActiveBoxes++] = idx;
    }
}
