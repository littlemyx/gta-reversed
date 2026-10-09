#include "StdInc.h"

#include "platform.h"

// RenderWare file interface (RwOsGetFileInterface), see rwplcore.h in the SDK
struct PresetRwFileFunctions {
    void*  rwfexist;
    void*  (__cdecl* rwfopen)(const char* path, const char* mode);
    int32  (__cdecl* rwfclose)(void* file);
    size_t (__cdecl* rwfread)(void* buf, size_t size, size_t count, void* file);
    size_t (__cdecl* rwfwrite)(const void* buf, size_t size, size_t count, void* file);
    char*  (__cdecl* rwfgets)(char* buf, int32 maxLen, void* file);
    int32  (__cdecl* rwfputs)(const char* str, void* file);
    void*  rwfeof;
    void*  rwfseek;
    void*  rwfflush;
    void*  rwftell;
};
static_assert(sizeof(PresetRwFileFunctions) == 0x2C);

// 0x804130
static PresetRwFileFunctions* RwOsGetFileInterface() {
    return plugin::CallAndReturn<PresetRwFileFunctions*, 0x804130>();
}

struct PresetView {
    CVector m_Translation;
    float m_RotX;
    float m_RotY;
    float m_NearClip;
    float m_FarClip;
    char* m_Description;
    PresetView* m_Next;
};

// 3x3 identity matrix
constexpr CVector PresetViewAxisX = {1.0f, 0.0f, 0.0f}; // 0x8D2E00
constexpr CVector PresetViewAxisY = {0.0f, 1.0f, 0.0f}; // 0x8D2E0C
constexpr CVector PresetViewAxisZ = {0.0f, 0.0f, 1.0f}; // 0x8D2E18

constexpr auto ViewsFileName = "./views.txt"; // 0x8D2E24;

auto& PresetViews = StaticRef<PresetView*>(0xC1707C);
auto& NumPresetViews = StaticRef<int32>(0xC17080);
auto& CurrentPresetView = StaticRef<int32>(0x8D2E30); // -1

// 0x619780
bool RsSetPresetView(RwCamera* camera, int32 viewNum) {
    if (!camera || !NumPresetViews || viewNum >= NumPresetViews || viewNum < 0)
        return false;

    auto* pv = PresetViews;
    int v3 = NumPresetViews - viewNum - 1u;
    for (CurrentPresetView = viewNum; v3 > 0; --v3) {
        if (!pv) {
            NOTSA_UNREACHABLE();
            break;
        }

        pv = pv->m_Next;
    }

    RwFrame* parent = RwCameraGetFrame(camera);
    RwFrameSetIdentity(parent);
    RwFrameRotate(parent, &PresetViewAxisX, -pv->m_RotX, rwCOMBINEREPLACE);
    RwFrameRotate(parent, &PresetViewAxisY, pv->m_RotY, rwCOMBINEPOSTCONCAT);
    RwFrameTranslate(parent, &pv->m_Translation, rwCOMBINEPOSTCONCAT);
    RwFrameUpdateObjects(parent);
    RwCameraSetNearClipPlane(camera, pv->m_NearClip);
    RwCameraSetFarClipPlane(camera, pv->m_FarClip);

    return true;
}

// 0x619840
void RsSetNextPresetView(RwCamera* camera) {
    if (!camera)
        return;

    if (!NumPresetViews)
        return;

    auto viewNum = ++CurrentPresetView;
    if (CurrentPresetView >= NumPresetViews) {
        viewNum = 0;
        CurrentPresetView = 0;
    }

    RsSetPresetView(camera, viewNum);
}

// 0x619880
void RsSetPreviousPresetView(RwCamera* camera) {
    if (!camera || !NumPresetViews)
        return;

    if (--CurrentPresetView < 0)
        CurrentPresetView = NumPresetViews - 1;

    RsSetPresetView(camera, CurrentPresetView);
}

// 0x6198C0
void RsDestroyPresetViews() {
    auto pv = PresetViews;
    if (PresetViews) {
        PresetView* m_Next;
        do {
            m_Next = pv->m_Next;
            if (pv->m_Description) {
                CMemoryMgr::Free(pv->m_Description);
            }
            CMemoryMgr::Free(pv);
            pv = m_Next;
        } while (m_Next);
    }
    PresetViews = nullptr;
    NumPresetViews = 0;
}

// 0x619AB0
void* RsGetPresetViewDescription() {
    if (!NumPresetViews || CurrentPresetView == -1) {
        return nullptr;
    }

    auto* pv = PresetViews;
    for (auto i = NumPresetViews - CurrentPresetView - 1; i > 0; --i) {
        if (!pv) {
            break;
        }
        pv = pv->m_Next;
    }
    if (!pv) {
        NOTSA_UNREACHABLE();
    }

    return pv->m_Description;
}

// 0x619910 - usercall: EAX = camera, EDI = pos, EBX = yaw, stack = elevation
// Gets the parameters of the camera that are saved into a preset view.
static void GetPresetViewParams(RwCamera* camera, CVector& pos, float& elevation, float& yaw) {
    static_assert(offsetof(RwCamera, nearPlane) == 0x80 && offsetof(RwCamera, farPlane) == 0x84);

    const RwMatrix* const ltm = RwFrameGetLTM(RwCameraGetFrame(camera));

    pos = *reinterpret_cast<const CVector*>(&ltm->pos);

    // Elevation: the angle between the camera's `at` and the Y axis (computed in extended precision in the original)
    const double dotAtY = ((double)PresetViewAxisY.x * ltm->at.x + (double)PresetViewAxisY.y * ltm->at.y) + (double)PresetViewAxisY.z * ltm->at.z;
    float cosElevation = (float)dotAtY;
    if (dotAtY > 1.0) { // NOTE: compares the unrounded value
        cosElevation = 1.0f;
    } else if (cosElevation < -1.0f) {
        cosElevation = -1.0f;
    }
    elevation = (float)(90.0f - std::acos((double)cosElevation) * 57.2957763671875f); // 0x859878, 0x85991C

    // Project `at` onto the XZ plane
    RwV3d at = {
        ltm->at.x - PresetViewAxisY.x * cosElevation,
        ltm->at.y - PresetViewAxisY.y * cosElevation,
        ltm->at.z - PresetViewAxisY.z * cosElevation,
    };
    RwV3dNormalize(&at, &at);

    // BUG: The original multiplies the Z axis' Y component with `at.z` (not `at.y`), the result is the same only because the axis is (0, 0, 1)
    double dotAtZ = ((double)PresetViewAxisZ.y * at.z + (double)PresetViewAxisZ.x * at.x) + (double)PresetViewAxisZ.z * at.z;
    if (dotAtZ > 1.0) {
        dotAtZ = 1.0;
    } else if (dotAtZ < -1.0) {
        dotAtZ = -1.0;
    }
    const double yawAngle = std::acos(dotAtZ) * 57.2957763671875f;
    yaw = (float)yawAngle;

    const double dotAtX = ((double)PresetViewAxisX.y * at.y + (double)PresetViewAxisX.x * at.x) + (double)PresetViewAxisX.z * at.z;
    if (dotAtX < 0.0) {
        yaw = (float)(-yawAngle);
    }
}

// 0x619D60
bool RsLoadPresetViews() {
    RsDestroyPresetViews();
    CurrentPresetView = -1;

    if (auto* const fi = RwOsGetFileInterface()) {
        auto* const path = psPathnameCreate(ViewsFileName);
        void* const file = fi->rwfopen(path, "r"); // 0x85A53C
        psPathnameDestroy(path);

        if (file) {
            char line[1024];
            while (fi->rwfgets(line, 0x3FF, file)) {
                // Strip everything that isn't printable
                char* out = line;
                for (const char* in = line; *in; in++) {
                    if (isprint((uint8)*in)) {
                        *out++ = *in;
                    }
                }
                *out = '\0';

                float  x, y, z, rotX, rotY, nearClip, farClip;
                char   description[1024];
                // NOTE: The original format string has an embedded NUL after `%[^`, so the scanset is never terminated (0x86D3E4)
                const auto numRead = RwEngineInstance->stringFuncs.vecSscanf(line, "%f%f%f%f%f%f%f %[^", &x, &y, &z, &rotX, &rotY, &nearClip, &farClip, description);
                if (numRead <= 6) {
                    continue;
                }

                if (numRead == 7) { // No description
                    RwEngineInstance->stringFuncs.vecSprintf(description, "%s%d", "View", NumPresetViews); // 0x86D3D4, 0x86D3DC
                }

                auto* const pv = static_cast<PresetView*>(CMemoryMgr::Malloc(sizeof(PresetView), 0));
                if (!pv) {
                    RsErrorMessage("Out of memory - not all preset views loaded");
                    return false;
                }
                pv->m_Translation = CVector{x, y, z};
                pv->m_RotX        = rotX;
                pv->m_RotY        = rotY;
                pv->m_NearClip    = nearClip;
                pv->m_FarClip     = farClip;

                // NOTE: The original calls `rwstrlen` here a second time on the description (result unused otherwise)
                pv->m_Description = static_cast<char*>(CMemoryMgr::Malloc(RwEngineInstance->stringFuncs.vecStrlen(description) + 1, 0));
                if (!pv->m_Description) {
                    RsErrorMessage("Out of memory - not all preset views loaded");
                    CMemoryMgr::Free(pv);
                    return false;
                }
                RwEngineInstance->stringFuncs.vecStrcpy(pv->m_Description, description);

                pv->m_Next  = PresetViews;
                PresetViews = pv;
                NumPresetViews++;
            }
            fi->rwfclose(file);
        }
    }

    return NumPresetViews != 0;
}

// 0x619FA0
bool RsSavePresetView(RwCamera* camera) {
    auto* const fi = RwOsGetFileInterface();
    if (!fi) {
        return false;
    }

    auto* const path = psPathnameCreate(ViewsFileName);
    void* const file = fi->rwfopen(path, "a"); // 0x863A2C
    psPathnameDestroy(path);

    if (!file) {
        RsKeyStatus ks{};
        ks.keyScanCode = 0x420;
        if (RsGlobal.keyboard.used) {
            RsKeyboardEventHandler(rsKEYUP, &ks);
        }
        psErrorMessage("Cannot open preset view file");
        return false;
    }

    CVector pos;
    float   elevation, yaw;
    GetPresetViewParams(camera, pos, elevation, yaw);

    char line[1028];
    RwEngineInstance->stringFuncs.vecSprintf(
        line,
        "%0.6f %0.6f %0.6f  %0.6f %0.6f  %0.6f %0.6f %s%d\n", // 0x86D440
        (double)pos.x, (double)pos.y, (double)pos.z,
        (double)elevation,
        (double)yaw,
        (double)camera->nearPlane,
        (double)camera->farPlane,
        "View", // 0x86D3DC
        NumPresetViews
    );
    // NOTE: The original calls `rwstrlen(line)` here, the result is unused

    bool ret = false;
    if (fi->rwfputs(line, file) > 0) {
        // NOTE: The file isn't closed (flushed) yet when the views are loaded
        ret = RsLoadPresetViews();
        if (ret) {
            RsSetPresetView(camera, NumPresetViews - 1);
        }
    } else {
        RsErrorMessage("Cannot write to preset view file");
    }
    fi->rwfclose(file);
    return ret;
}
