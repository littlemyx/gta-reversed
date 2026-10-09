#include "StdInc.h"

#include "FxInfoManager.h"
#include "FxManager.h"

#include "EmissionInfo.h"
#include "MovementInfo.h"
#include "RenderInfo.h"

#include "FxInfoEmRate.h"
#include "FxInfoEmSize.h"
#include "FxInfoEmSpeed.h"
#include "FxInfoEmDir.h"
#include "FxInfoEmAngle.h"
#include "FxInfoEmLife.h"
#include "FxInfoEmPos.h"
#include "FxInfoEmWeather.h"
#include "FxInfoEmRotation.h"
#include "FxInfoNoise.h"
#include "FxInfoForce.h"
#include "FxInfoFriction.h"
#include "FxInfoAttractPt.h"
#include "FxInfoAttractLine.h"
#include "FxInterpInfoFloat.h"
#include "FxInterpInfo255.h"
#include "FxInterpInfoU255.h"
#include "FxInfoGroundCollide.h"
#include "FxInfoWind.h"
#include "FxInfoJitter.h"
#include "FxInfoRotSpeed.h"
#include "FxInfoFloat.h"
#include "FxInfoUnderwater.h"
#include "FxInfoColour.h"
#include "FxInfoSize.h"
#include "FxInfoSpriteRect.h"
#include "FxInfoHeatHaze.h"
#include "FxInfoTrail.h"
#include "FxInfoFlat.h"
#include "FxInfoDir.h"
#include "FxInfoAnimTexture.h"
#include "FxInfoColourRange.h"
#include "FxInfoSelfLit.h"
#include "FxInfoColourBright.h"
#include "FxInfoSmoke.h"
#include "FxInfoRotate.h"
#include "FxInfoTexCoords.h"
#include "FxInfoRotateOffset.h"


void FxInfoManager_c::InjectHooks() {
    RH_ScopedClass(FxInfoManager_c);
    RH_ScopedCategory("Fx");

    RH_ScopedInstall(AddFxInfo, 0x4A7B00);
    RH_ScopedInstall(Load, 0x5C0B70);
    RH_ScopedInstall(ProcessEmissionInfo, 0x4A4960);
    RH_ScopedInstall(ProcessMovementInfo, 0x4A4A10);
    RH_ScopedInstall(ProcessRenderInfo, 0x4A4A80);

    // NOTSA: the Fx blueprint classes (FxInfoAttractLine_c, FxInterpInfo{Float,32,255,U255}_c) are registered in InjectHooksMain.cpp;
    // a second registration here trips the "Category already contains an item" assert at startup
}

// 0x4A7B00
FxInfo_c* FxInfoManager_c::AddFxInfo(int32 info) {
    switch (info) {
    case FX_INFO_EMRATE_DATA:        return new FxInfoEmRate_c();
    case FX_INFO_EMSIZE_DATA:        return new FxInfoEmSize_c();
    case FX_INFO_EMSPEED_DATA:       return new FxInfoEmSpeed_c();
    case FX_INFO_EMDIR_DATA:         return new FxInfoEmDir_c();
    case FX_INFO_EMANGLE_DATA:       return new FxInfoEmAngle_c();
    case FX_INFO_EMLIFE_DATA:        return new FxInfoEmLife_c();
    case FX_INFO_EMPOS_DATA:         return new FxInfoEmPos_c();
    case FX_INFO_EMWEATHER_DATA:     return new FxInfoEmWeather_c();
    case FX_INFO_EMROTATION_DATA:    return new FxInfoEmRotation_c();
    case FX_INFO_NOISE_DATA:         return new FxInfoNoise_c();
    case FX_INFO_FORCE_DATA:         return new FxInfoForce_c();
    case FX_INFO_FRICTION_DATA:      return new FxInfoFriction_c();
    case FX_INFO_ATTRACTPT_DATA:     return new FxInfoAttractPt_c();
    case FX_INFO_ATTRACTLINE_DATA:   return new FxInfoAttractLine_c();
    case FX_INFO_GROUNDCOLLIDE_DATA: return new FxInfoGroundCollide_c();
    case FX_INFO_WIND_DATA:          return new FxInfoWind_c();
    case FX_INFO_JITTER_DATA:        return new FxInfoJitter_c();
    case FX_INFO_ROTSPEED_DATA:      return new FxInfoRotSpeed_c();
    case FX_INFO_FLOAT_DATA:         return new FxInfoFloat_c();
    case FX_INFO_UNDERWATER_DATA:    return new FxInfoUnderwater_c();
    case FX_INFO_COLOUR_DATA:        return new FxInfoColour_c();
    case FX_INFO_SIZE_DATA:          return new FxInfoSize_c();
    case FX_INFO_SPRITERECT_DATA:    return new FxInfoSpriteRect_c();
    case FX_INFO_HEATHAZE_DATA:      return new FxInfoHeatHaze_c();
    case FX_INFO_TRAIL_DATA:         return new FxInfoTrail_c();
    case FX_INFO_FLAT_DATA:          return new FxInfoFlat_c();
    case FX_INFO_DIR_DATA:           return new FxInfoDir_c();
    case FX_INFO_ANIMTEX_DATA:       return new FxInfoAnimTexture_c();
    case FX_INFO_COLOURRANGE_DATA:   return new FxInfoColourRange_c();
    case FX_INFO_SELFLIT_DATA:       return new FxInfoSelfLit_c();
    case FX_INFO_COLOURBRIGHT_DATA:  return new FxInfoColourBright_c();
    case FX_INFO_SMOKE_DATA:         return new FxInfoSmoke_c();
    // NOTSA case FX_INFO_ROTATE_DATA:        return new FxInfoRotate_c();
    // NOTSA case FX_INFO_TEXCOORDS_DATA:     return new FxInfoTexCoords_c();
    // NOTSA case FX_INFO_ROTATEOFFSET_DATA:  return new FxInfoRotateOffset_c();
    default:                         return nullptr;
    };
}

constexpr struct { int32 type; const char* name; } FXINFOMANAGER_C_LOAD_MAPPING[] = {
    { FX_INFO_EMRATE_DATA,           "FX_INFO_EMRATE_DATA:"        },
    { FX_INFO_EMSIZE_DATA,           "FX_INFO_EMSIZE_DATA:"        },
    { FX_INFO_EMSPEED_DATA,          "FX_INFO_EMSPEED_DATA:"       },
    { FX_INFO_EMDIR_DATA,            "FX_INFO_EMDIR_DATA:"         },
    { FX_INFO_EMANGLE_DATA,          "FX_INFO_EMANGLE_DATA:"       },
    { FX_INFO_EMLIFE_DATA,           "FX_INFO_EMLIFE_DATA:"        },
    { FX_INFO_EMPOS_DATA,            "FX_INFO_EMPOS_DATA:"         },
    { FX_INFO_EMWEATHER_DATA,        "FX_INFO_EMWEATHER_DATA:"     },
    { FX_INFO_EMROTATION_DATA,       "FX_INFO_EMROTATION_DATA:"    },
    { FX_INFO_NOISE_DATA,            "FX_INFO_NOISE_DATA:"         },
    { FX_INFO_FORCE_DATA,            "FX_INFO_FORCE_DATA:"         },
    { FX_INFO_FRICTION_DATA,         "FX_INFO_FRICTION_DATA:"      },
    { FX_INFO_ATTRACTPT_DATA,        "FX_INFO_ATTRACTPT_DATA:"     },
    { FX_INFO_ATTRACTLINE_DATA,      "FX_INFO_ATTRACTLINE_DATA:"   },
    { FX_INFO_GROUNDCOLLIDE_DATA,    "FX_INFO_GROUNDCOLLIDE_DATA:" },
    { FX_INFO_WIND_DATA,             "FX_INFO_WIND_DATA:"          },
    { FX_INFO_JITTER_DATA,           "FX_INFO_JITTER_DATA:"        },
    { FX_INFO_ROTSPEED_DATA,         "FX_INFO_ROTSPEED_DATA:"      },
    { FX_INFO_FLOAT_DATA,            "FX_INFO_FLOAT_DATA:"         },
    { FX_INFO_UNDERWATER_DATA,       "FX_INFO_UNDERWATER_DATA:"    },
    { FX_INFO_COLOUR_DATA,           "FX_INFO_COLOUR_DATA:"        },
    { FX_INFO_SIZE_DATA,             "FX_INFO_SIZE_DATA:"          },
    { FX_INFO_SPRITERECT_DATA,       "FX_INFO_SPRITERECT_DATA:"    },
    { FX_INFO_HEATHAZE_DATA,         "FX_INFO_HEATHAZE_DATA:"      },
    { FX_INFO_TRAIL_DATA,            "FX_INFO_TRAIL_DATA:"         },
    { FX_INFO_FLAT_DATA,             "FX_INFO_FLAT_DATA:"          },
    { FX_INFO_DIR_DATA,              "FX_INFO_DIR_DATA:"           },
    { FX_INFO_ANIMTEX_DATA,          "FX_INFO_ANIMTEX_DATA:"       },
    { FX_INFO_COLOURRANGE_DATA,      "FX_INFO_COLOURRANGE_DATA:"   },
    { FX_INFO_SELFLIT_DATA,          "FX_INFO_SELFLIT_DATA:"       },
    { FX_INFO_COLOURBRIGHT_DATA,     "FX_INFO_COLOURBRIGHT_DATA:"  },
    { FX_INFO_SMOKE_DATA,            "FX_INFO_SMOKE_DATA:"         },
    // NOTSA { FX_INFO_ROTATE_DATA,           "FX_INFO_ROTATE_DATA:"        },
    // NOTSA { FX_INFO_TEXCOORDS_DATA,        "FX_INFO_TEXCOORDS_DATA:"     },
    // NOTSA { FX_INFO_ROTATEOFFSET_DATA,     "FX_INFO_ROTATEOFFSET_DATA:"  },
};

// 0x5C0B70
void FxInfoManager_c::Load(FILESTREAM file, int32 version) {
    ReadField<void>(file); // 1st line is skipped
    m_nNumInfos = ReadField<int32>(file, "NUM_INFOS:");
    m_MovementOffset = -1;
    m_RenderOffset = -1;

    m_pInfos = g_fxMan.Allocate<FxInfo_c*>(m_nNumInfos);
    assert(m_pInfos);

    char line[256], field[128];
    for (int32 i = 0; i < m_nNumInfos; i++) {
        int32 timeModePrt = 1;

        ReadLine(file, line, sizeof(line));
        VERIFY(sscanf(line, "%s", field) == 1);

        const auto GetType = [=]() -> int32 {
            for (auto& [type, name] : FXINFOMANAGER_C_LOAD_MAPPING) {
                if (strcmp(field, name) == 0) {
                    return type;
                }
            }
            NOTSA_UNREACHABLE("Unknown FX Info: %s", field); // NOTSA
        };
        const auto infoType = GetType();

        // Only the emission infos (0x1xxx) don't have the TIMEMODEPRT line
        if ((infoType & 0xF000) > 0x1000 && (double)version >= 0.7) { // 0x86B1D0 (double 0.7)
            timeModePrt = ReadField<int32>(file, "TIMEMODEPRT:");
        }

        auto* const info = AddFxInfo(infoType);
        assert(info); // printf("Failed to load info: %s\n", field)
        m_pInfos[i] = info;
        info->Load(file, version);
        ReadField<void>(file);

        info->m_bTimeModeParticle = (uint8)timeModePrt != 0; // The original stores the low byte of the value

        const auto type = (uint16)info->m_nType;
        if (!(type & 0x1000)) {
            if (!(type & 0x2000)) {
                if ((type & 0xC000) && m_RenderOffset == -1) {
                    m_RenderOffset = (int8)i;
                }
            } else if (m_MovementOffset == -1) {
                m_MovementOffset = (int8)i;
            }
        }
    }

    if (m_RenderOffset == -1) {
        m_RenderOffset = (int8)m_nNumInfos;
    }

    if (m_MovementOffset == -1) {
        m_MovementOffset = m_RenderOffset;
    }
}

/* todo:
 *  info->m_nType
 *  replace raw iteration to *irange*
 *  ProcessRenderInfo: mult = prt->m_fCurrentLife / prt->m_fTotalLife;
 */

// 0x4A4960
void FxInfoManager_c::ProcessEmissionInfo(float currentTime, float mult, float totalTime, bool useConst, EmissionInfo_t* emission) {
    emission->Process(mult);

    for (auto i = 0; i < m_MovementOffset; i++) {
        auto& info = m_pInfos[i];
        if ((info->m_nType & 0x1000) != 0) {
            info->GetValue(currentTime, 0.0f, mult, totalTime, useConst, emission);
        }
    }
}

// a6 - always false
// 0x4A4A10
void FxInfoManager_c::ProcessMovementInfo(float currentTime, float mult, float totalTime, float length, bool useConst, MovementInfo_t* movementInfo) {
    movementInfo->Process();

    for (int i = m_MovementOffset; i < m_RenderOffset; i++) {
        auto& info = m_pInfos[i];
        if ((info->m_nType & 0x2000) != 0) {
            info->GetValue(currentTime, mult, totalTime, length, useConst, movementInfo);
        }
    }
}

// a6 - always false
// 0x4A4A80
void FxInfoManager_c::ProcessRenderInfo(float currentTime, float mult, float totalTime, float length, bool useConst, RenderInfo_t* renderInfo) {
    renderInfo->Process();

    for (int i = m_RenderOffset; i < m_nNumInfos; i++) {
        auto& info = m_pInfos[i];
        if ((info->m_nType & 0xC000) != 0) { // 0xC000 == -0x4000u | m_nType >= 0x4000u
            info->GetValue(currentTime, mult, totalTime, length, useConst, renderInfo);
        }
    }
}
