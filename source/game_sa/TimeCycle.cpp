#include "StdInc.h"

#include "TimeCycle.h"
#include "PostEffects.h"
#include "Shadows.h"

auto& TunnelWeather = StaticRef<int>(0x8CDEE0); // 9 = WEATHER_FOGGY_SF, unchanged

void CTimeCycle::InjectHooks() {
    RH_ScopedClass(CTimeCycle);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x5BBAC0);
    RH_ScopedInstall(InitForRestart, 0x5601F0);
    RH_ScopedInstall(Shutdown, 0x5601E0);
    RH_ScopedInstall(Update, 0x561760);
    RH_ScopedInstall(StartExtraColour, 0x55FEC0);
    RH_ScopedInstall(StopExtraColour, 0x55FF20);
    RH_ScopedInstall(AddOne, 0x55FF40);
    RH_ScopedInstall(CalcColoursForPoint, 0x5603D0);
    RH_ScopedInstall(FindFarClipForCoors, 0x5616E0);
    RH_ScopedInstall(FindTimeCycleBox, 0x55FFD0);
    RH_ScopedInstall(SetConstantParametersForPostFX, 0x560210);
    RH_ScopedInstall(GetAmbientRed, 0x560330);
    RH_ScopedInstall(GetAmbientGreen, 0x560340);
    RH_ScopedInstall(GetAmbientBlue, 0x560350);
    RH_ScopedInstall(GetAmbientRed_BeforeBrightness, 0x560390);
    RH_ScopedInstall(GetAmbientGreen_BeforeBrightness, 0x5603A0);
    RH_ScopedInstall(GetAmbientBlue_BeforeBrightness, 0x5603B0);
    RH_ScopedInstall(GetAmbientRed_Obj, 0x560360);
    RH_ScopedInstall(GetAmbientGreen_Obj, 0x560370);
    RH_ScopedInstall(GetAmbientBlue_Obj, 0x560380);
}

// 0x5BBAC0
void CTimeCycle::Initialise(bool padFile) {
    CFileMgr::SetDir("DATA");
    auto file = CFileMgr::OpenFile("TIMECYC.DAT", "rb");
    CFileMgr::SetDir("");

    if (!file) { // NOTSA
        NOTSA_LOG_WARN("[CTimeCycle] Failed to open TIMECYC.DAT");
        CFileMgr::CloseFile(file);
        return;
    }

    int32 ambR, ambG, ambB;
    int32 ambObjR, ambObjG, ambObjB;
    int32 dirR, dirG, dirB;
    int32 skyTopR, skyTopG, skyTopB;
    int32 skyBotR, skyBotG, skyBotB;
    int32 sunCoreR, sunCoreG, sunCoreB;
    int32 sunCoronaR, sunCoronaG, sunCoronaB;
    float sunSize, spriteSize, spriteBrightness;
    int32 shadowStrength, lightShadowStrength, poleShadowStrength;
    float farClip, fogStart, lightOnGround;
    int32 lowCloudR, lowCloudG, lowCloudB;
    int32 bottomCloudR, bottomCloudG, bottomCloudB;
    float waterR, waterG, waterB, waterA;
    float postFx1A, postFx1R, postFx1G, postFx1B;
    float postFx2A, postFx2R, postFx2G, postFx2B;
    float cloudAlpha;
    int32 highLightMinIntensity;
    int32 waterFogAlpha;
    float dirMult;

    char* line;
    for (auto w = 0; w < NUM_WEATHERS; w++) {
        for (auto h = 0; h < NUM_HOURS; h++) {
            while (line = CFileLoader::LoadLine(file)) {
                if (line[0] != '/' && line[0] != '\0') {
                    break;
                }
            }

            const auto n = sscanf(line,
                "%d %d %d " // Static ambience color
                "%d %d %d " // Dynamic ambience color
                "%d %d %d " // Direct light color - NOP
                "%d %d %d " // Sky top color
                "%d %d %d " // Sky bottom color
                "%d %d %d " // Sun core color
                "%d %d %d " // Sun corona color
                "%f "       // Sun core size
                "%f "       // Sun corona size
                "%f "       // Sprite brightness
                "%d "       // Pole shading value
                "%d "       // Light shading value
                "%d "       // Pole shading value
                "%f "       // Far clipping offset
                "%f "       // Fog start offset
                "%f "       // Light on ground
                "%d %d %d " // Lower clouds color
                "%d %d %d " // Upper clouds bottom color
                "%f %f %f " // Water color
                "%f "       // Water alpha level
                "%f "       // Color correction 1 alpha
                "%f %f %f " // Color correction 1
                "%f "       // Color correction 2 alpha
                "%f %f %f " // Color correction 2
                "%f "       // Lower clouds alpha level
                "%d "       // Highlight min intensity
                "%d "       // Water fog alpha
                "%f",       // Directional multiplier
                &ambR, &ambG, &ambB,
                &ambObjR, &ambObjG, &ambObjB,
                &dirR, &dirG, &dirB,
                &skyTopR, &skyTopG, &skyTopB,
                &skyBotR, &skyBotG, &skyBotB,
                &sunCoreR, &sunCoreG, &sunCoreB,
                &sunCoronaR, &sunCoronaG, &sunCoronaB,
                &sunSize, &spriteSize, &spriteBrightness,
                &shadowStrength, &lightShadowStrength, &poleShadowStrength,
                &farClip, &fogStart, &lightOnGround,
                &lowCloudR, &lowCloudG, &lowCloudB,
                &bottomCloudR, &bottomCloudG, &bottomCloudB,
                &waterR, &waterG, &waterB, &waterA,
                &postFx1A, &postFx1R, &postFx1G, &postFx1B,
                &postFx2A, &postFx2R, &postFx2G, &postFx2B,
                &cloudAlpha, &highLightMinIntensity, &waterFogAlpha, &dirMult
            );
            if (n < 51) {
                // TODO:
                // R* made a mistake in line 320:
                // instead of the first 3 RGB values, only 255 is specified,
                // which causes the entire line to shift and be read incorrectly
                NOTSA_LOG_WARN("Bad timecyc line: '{}'", line);
            }

            m_nAmbientRed[h][w]   = ambR;
            m_nAmbientGreen[h][w] = ambG;
            m_nAmbientBlue[h][w]  = ambB;

            m_nAmbientRed_Obj[h][w]   = ambObjR;
            m_nAmbientGreen_Obj[h][w] = ambObjG;
            m_nAmbientBlue_Obj[h][w]  = ambObjB;

            // code dir RGB?

            m_nSkyTopRed[h][w]   = skyTopR;
            m_nSkyTopGreen[h][w] = skyTopG;
            m_nSkyTopBlue[h][w]  = skyTopB;

            m_nSkyBottomRed[h][w]   = skyBotR;
            m_nSkyBottomGreen[h][w] = skyBotG;
            m_nSkyBottomBlue[h][w]  = skyBotB;

            m_nSunCoreRed[h][w]   = sunCoreR;
            m_nSunCoreGreen[h][w] = sunCoreG;
            m_nSunCoreBlue[h][w]  = sunCoreB;

            m_nSunCoronaRed[h][w]   = sunCoronaR;
            m_nSunCoronaGreen[h][w] = sunCoronaG;
            m_nSunCoronaBlue[h][w]  = sunCoronaB;

            m_fSunSize[h][w]          = int8(sunSize * 10.0f + 0.5f);
            m_fSpriteSize[h][w]       = int8(spriteSize * 10.0f + 0.5f);
            m_fSpriteBrightness[h][w] = int8(spriteBrightness * 10.0f + 0.5f);

            m_nShadowStrength[h][w]      = shadowStrength;
            m_nLightShadowStrength[h][w] = lightShadowStrength;
            m_nPoleShadowStrength[h][w]  = poleShadowStrength;

            m_fFarClip[h][w]  = (int16)farClip;
            m_fFogStart[h][w] = (int16)fogStart;
            m_fLightsOnGroundBrightness[h][w] = uint8(lightOnGround * 10.0f + 0.5f);

            m_nLowCloudsRed[h][w]   = lowCloudR;
            m_nLowCloudsGreen[h][w] = lowCloudG;
            m_nLowCloudsBlue[h][w]  = lowCloudB;

            m_nFluffyCloudsBottomRed[h][w]   = (uint8)bottomCloudR;
            m_nFluffyCloudsBottomGreen[h][w] = (uint8)bottomCloudG;
            m_nFluffyCloudsBottomBlue[h][w]  = (uint8)bottomCloudB;

            m_fWaterRed[h][w]   = (uint8)waterR;
            m_fWaterGreen[h][w] = (uint8)waterG;
            m_fWaterBlue[h][w]  = (uint8)waterB;
            m_fWaterAlpha[h][w] = (uint8)waterA;

            m_fPostFx1Red[h][w]   = (uint8)postFx1R;
            m_fPostFx1Green[h][w] = (uint8)postFx1G;
            m_fPostFx1Blue[h][w]  = (uint8)postFx1B;

            m_fPostFx2Red[h][w]   = (uint8)postFx2R;
            m_fPostFx2Green[h][w] = (uint8)postFx2G;
            m_fPostFx2Blue[h][w]  = (uint8)postFx2B;

            m_fPostFx1Alpha[h][w] = uint8(postFx1A * 2.f);
            m_fPostFx2Alpha[h][w] = uint8(postFx2A * 2.f);

            m_fCloudAlpha[h][w]            = (uint8)cloudAlpha;
            m_nHighLightMinIntensity[h][w] = (uint8)highLightMinIntensity;
            m_nWaterFogAlpha[h][w]         = (uint8)waterFogAlpha;
            m_nDirectionalMult[h][w]       = uint8((uint8)dirMult * 100.0f);
        }
    }

    CFileMgr::CloseFile(file);
    // exe (0x5BC026): x87 `fcos`/`fsin` of the doubles at 0x86A6C8 (-3PI/4 as widened float) and 0x86A6D0 (PI/4 as widened float), products stored as floats
    constexpr double ANGLE_XY = -2.356194496154785;  // 0x86A6C8 (float 0xC016CBE4)
    constexpr double ANGLE_Z  = 0.7853981852531433;  // 0x86A6D0 (float 0x3F490FDB)
    const double     cosZ     = x87::cos(ANGLE_Z);
    m_vecDirnLightToSun.x = (float)(x87::cos(ANGLE_XY) * cosZ); // -0.5f
    m_vecDirnLightToSun.y = (float)(x87::sin(ANGLE_XY) * cosZ); // -0.5f
    m_vecDirnLightToSun.z = (float)x87::sin(ANGLE_Z);
    m_vecDirnLightToSun.Normalise();
    m_FogReduction = 0;
    m_bExtraColourOn = false;
}

// 0x5601F0
void CTimeCycle::InitForRestart() {
    StopExtraColour(false);
}

// 0x5601E0
void CTimeCycle::Shutdown() {
    // NOP
}

// 0x561760
void CTimeCycle::Update() {
    ZoneScoped;

    CalcColoursForPoint(TheCamera.GetPosition(), &m_CurrentColours);
}

// 0x55FEC0
void CTimeCycle::StartExtraColour(int32 color, bool bNoExtraColorInterior) {
    m_ExtraColourWeatherType = color / NUM_HOURS + WEATHER_EXTRA_START;
    m_ExtraColour = color % NUM_HOURS;
    m_bExtraColourOn = true;
    m_ExtraColourInter = 0.0f;
    if (!bNoExtraColorInterior) {
        m_ExtraColourInter = 1.0f;
    }
}

// 0x55FF20
void CTimeCycle::StopExtraColour(bool bNoExtraColorInterior) {
    m_bExtraColourOn = false;
    if (!bNoExtraColorInterior) {
        m_ExtraColourInter = 0.0f;
    }
}

// 0x55FF40
void CTimeCycle::AddOne(CBox& box, int16 farClip, int32 m_ExtraColor, float strength, float falloff, float lodDistMult) {
    m_aBoxes[m_NumBoxes].Box = box;
    m_aBoxes[m_NumBoxes].FarClip = farClip;
    m_aBoxes[m_NumBoxes].ExtraColor = m_ExtraColor;
    m_aBoxes[m_NumBoxes].Strength = strength * ExeRecip(100.0f);
    m_aBoxes[m_NumBoxes].Falloff = falloff;
    m_aBoxes[m_NumBoxes].LodDistMult = (uint8)(std::min(lodDistMult, 4.0f) * 32.0f);
    m_NumBoxes++;
}

// 0x5603D0
void CTimeCycle::CalcColoursForPoint(CVector point, CColourSet* set) {
    constexpr auto TimeSamples         = std::to_array({ 0, 5, 6, 7, 12, 19, 20, 22, 24 }); // 0x8CDECC
    constexpr auto GreyValuesDuringDay = std::to_array({ 30, 30, 30, 50, 60, 60, 50, 35 }); // 0x8CDED8

    CTimeCycleBox *lodBoxA, *farBoxA, *farBoxB, *weatherBox;
    float lodBoxA_T, farBoxA_T, farBoxB_T, weatherBox_T;

    // Find LOD box
    FindTimeCycleBox(point, &lodBoxA, &lodBoxA_T, true, false, nullptr);

    // Find far boxes
    FindTimeCycleBox(point, &farBoxA, &farBoxA_T, false, true, nullptr);
    if (farBoxA) {
        FindTimeCycleBox(point, &farBoxB, &farBoxB_T, false, true, farBoxA);
        if (farBoxB && farBoxB->Box.GetWidth() > farBoxA->Box.GetWidth()) {
            std::swap(farBoxA, farBoxB);
            std::swap(farBoxA_T, farBoxB_T);
        }
    } else {
        farBoxB = nullptr;
    }

    // Find current weather box
    FindTimeCycleBox(point, &weatherBox, &weatherBox_T, false, false, nullptr);

    // Rewritten from the asm (0x5603D0..0x5616CD): the x87 stack keeps hours / t / f / the box factors unrounded (double here), every colour channel goes
    // through `_ftol`, and the table-range tests of the exe differ from the old port's (`cmp eax, -1` on a zero-extended byte never skips).
    // 0x5604C8 - hours today: (minutes * (1/60f) + seconds * (1/3600f)) + hours, in extended precision
    double hours = ((double)CClock::GetGameClockMinutes() * (double)std::bit_cast<float>(0x3C888889u) + (double)CClock::GetGameClockSeconds() * (double)std::bit_cast<float>(0x3991A2B4u)) + (double)CClock::GetGameClockHours();
    hours = (hours < (double)std::bit_cast<float>(0x41BFFDF4u)) ? hours : (double)std::bit_cast<float>(0x41BFFDF4u); // 23.999f, NaN -> 23.999f

    // 0x560528 - Find sample index for current hour
    int32 currSampleIdx = 0;
    while (!(hours < (double)TimeSamples[currSampleIdx + 1])) {
        currSampleIdx++;
    }
    const auto nextSampleIdx = (currSampleIdx + 1) % NUM_HOURS;

    const float timeT    = (float)((hours - (double)TimeSamples[currSampleIdx]) / (double)(TimeSamples[currSampleIdx + 1] - TimeSamples[currSampleIdx])); // fidiv, spilled
    const float invTimeT = (float)(1.0 - (double)timeT);

    // 0x5605B3 - the exe stores the (16 bit) old / new weather types in a pair of globals
    *reinterpret_cast<uint16*>(0xB7CB20) = (uint16)CWeather::OldWeatherType;
    *reinterpret_cast<uint16*>(0xB7CB24) = (uint16)CWeather::NewWeatherType;

    const float t = CWeather::InterpolationValue;

    // 0x5605D5
    eWeatherType boxWeather{ WEATHER_UNDEFINED };
    int32        boxHour;
    if (weatherBox) {
        boxHour    = weatherBox->ExtraColor % 8;
        boxWeather = weatherBox->ExtraColor >= 8
            ? WEATHER_EXTRACOLOURS_2
            : WEATHER_EXTRACOLOURS_1;
    }

    // 0x56064B - camera height factor: ((z - 20) * 0.005f) clamped to [0, 1] (NaN passes), spilled to float
    const auto& camPos = TheCamera.GetPosition();
    float f = (float)(((double)camPos.z - 20.0f) * (double)std::bit_cast<float>(0x3BA3D70Au));
    if (0.0f > f) {
        f = 0.0f;
    } else if (1.0f < f) {
        f = 1.0f;
    }
    const float invF = (float)(1.0 - (double)f);

    { // 0x5606B7
        CColourSet currentOld(currSampleIdx, CWeather::OldWeatherType);
        CColourSet nextOld(nextSampleIdx,   CWeather::OldWeatherType);

        CColourSet currentNew(currSampleIdx, CWeather::NewWeatherType);
        CColourSet nextNew(nextSampleIdx,   CWeather::NewWeatherType);

        if (f > 0.0f) { // 0x560691 (skipped for f <= 0 and NaN)
            if (CWeather::OldWeatherType == WEATHER_EXTRASUNNY_SMOG_LA) {
                CColourSet set1(currSampleIdx, WEATHER_EXTRASUNNY_LA);
                currentOld.Interpolate(&currentOld, &set1, invF, f, false);

                CColourSet set2(nextSampleIdx, WEATHER_EXTRASUNNY_LA);
                nextOld.Interpolate(&nextOld, &set2, invF, f, false);
            } else if (CWeather::OldWeatherType == WEATHER_SUNNY_SMOG_LA) {
                CColourSet set1(currSampleIdx, WEATHER_SUNNY_LA);
                currentOld.Interpolate(&currentOld, &set1, invF, f, false);

                CColourSet set2(nextSampleIdx, WEATHER_SUNNY_LA);
                nextOld.Interpolate(&nextOld, &set2, invF, f, false);
            }

            if (CWeather::NewWeatherType == WEATHER_EXTRASUNNY_SMOG_LA) {
                CColourSet set1(currSampleIdx, WEATHER_EXTRASUNNY_LA);
                currentNew.Interpolate(&currentNew, &set1, invF, f, false);

                CColourSet set2(nextSampleIdx, WEATHER_EXTRASUNNY_LA);
                nextNew.Interpolate(&nextNew, &set2, invF, f, false);
            } else if (CWeather::NewWeatherType == WEATHER_SUNNY_SMOG_LA) {
                CColourSet set1(currSampleIdx, WEATHER_SUNNY_LA);
                currentNew.Interpolate(&currentNew, &set1, invF, f, false);

                CColourSet set2(nextSampleIdx, WEATHER_SUNNY_LA);
                nextNew.Interpolate(&nextNew, &set2, invF, f, false);
            }
        }

        { // 0x560857
            CColourSet a{}, b{};
            a.Interpolate(&currentOld, &nextOld, invTimeT, timeT, false);
            b.Interpolate(&currentNew, &nextNew, invTimeT, timeT, false);
            set->Interpolate(&a, &b, (float)(1.0 - (double)t), t, false);
        }
    }

    // 0x5608C6 - sky colours: ftol(channel * ((1 / LightsMult + 3) * 0.25)), low 16 bits, clamped (unsigned) to 255
    {
        const double lightMult = (1.0 / (double)CCoronas::LightsMult + 3.0f) * 0.25f;
        const auto   Sky = [&](uint16& v) {
            const uint16 r = (uint16)(int32)((double)v * lightMult);
            v = r < 0xFF ? r : (uint16)0xFF;
        };
        Sky(set->m_nSkyTopRed);    Sky(set->m_nSkyTopGreen);    Sky(set->m_nSkyTopBlue);
        Sky(set->m_nSkyBottomRed); Sky(set->m_nSkyBottomGreen); Sky(set->m_nSkyBottomBlue);
    }

    if (m_FogReduction) { // 0x5609F8: `farClip > x ? farClip : x`
        const double x = (double)m_FogReduction * (double)std::bit_cast<float>(0x41228000u); // 10.15625f
        set->m_fFarClip = (set->m_fFarClip > x) ? set->m_fFarClip : (float)x;
    }

    // 0x560A26 - sun vector: the angle is not rounded to float before fsin / fcos
    m_CurrentStoredValue = (m_CurrentStoredValue + 1) & 15;
    {
        const double minutes  = (double)((int32)CClock::GetGameClockHours() * 60 + (int32)CClock::GetGameClockMinutes()) + (double)CClock::GetGameClockSeconds() * (double)std::bit_cast<float>(0x3C888889u);
        const double sunAngle = minutes * (double)std::bit_cast<float>(0x3B8EFA35u);
        auto& sun = m_VectorToSun[m_CurrentStoredValue];
        sun.x = (float)(x87::sin(sunAngle) + 0.7f);
        sun.y = -0.7f;
        sun.z = (float)(0.2f - x87::cos(sunAngle));
        sun.Normalise(); // 0x59C910
    }

    // 0x560AB3 - weather box
    if (weatherBox && weatherBox->ExtraColor >= 0) {
        const double boxf    = (double)weatherBox_T * weatherBox->Strength;
        const double invboxf = 1.0 - boxf;
        const auto   tbl     = [&](auto& table) -> double { return (double)table[boxHour][boxWeather]; };

        // sky: ftol(set * (1 - boxf) + table * boxf), 16 bit store. The `cmp eax, -1` guards on the byte tables never skip
        const auto Sky = [&](uint16& v, auto& table) { v = (uint16)(int32)((double)v * invboxf + tbl(table) * boxf); };
        Sky(set->m_nSkyTopRed, m_nSkyTopRed);       Sky(set->m_nSkyTopGreen, m_nSkyTopGreen);       Sky(set->m_nSkyTopBlue, m_nSkyTopBlue);
        Sky(set->m_nSkyBottomRed, m_nSkyBottomRed); Sky(set->m_nSkyBottomGreen, m_nSkyBottomGreen); Sky(set->m_nSkyBottomBlue, m_nSkyBottomBlue);

        // others: table * boxf + (1 - boxf) * set
        const auto Lerp = [&](float& v, auto& table) { v = (float)(tbl(table) * boxf + invboxf * (double)v); };
        Lerp(set->m_fWaterRed, m_fWaterRed); Lerp(set->m_fWaterGreen, m_fWaterGreen); Lerp(set->m_fWaterBlue, m_fWaterBlue); Lerp(set->m_fWaterAlpha, m_fWaterAlpha);
        Lerp(set->m_fAmbientRed, m_nAmbientRed); Lerp(set->m_fAmbientGreen, m_nAmbientGreen); Lerp(set->m_fAmbientBlue, m_nAmbientBlue);
        Lerp(set->m_fAmbientRed_Obj, m_nAmbientRed_Obj); Lerp(set->m_fAmbientGreen_Obj, m_nAmbientGreen_Obj); Lerp(set->m_fAmbientBlue_Obj, m_nAmbientBlue_Obj);

        if (m_fFarClip[boxHour][boxWeather] != -1) { // 0x560D08: int16 table, -1 = unused
            const double v = ((double)m_fFarClip[boxHour][boxWeather] < set->m_fFarClip) ? (double)m_fFarClip[boxHour][boxWeather] : (double)set->m_fFarClip;
            set->m_fFarClip = (float)(v * boxf + invboxf * (double)set->m_fFarClip); // always stored
        }
        if (m_fFogStart[boxHour][boxWeather] != -1) { // 0x560D3E
            set->m_fFogStart = (float)(tbl(m_fFogStart) * boxf + invboxf * (double)set->m_fFogStart);
        }
        Lerp(set->m_fPostFx1Red, m_fPostFx1Red); Lerp(set->m_fPostFx1Green, m_fPostFx1Green); Lerp(set->m_fPostFx1Blue, m_fPostFx1Blue); Lerp(set->m_fPostFx1Alpha, m_fPostFx1Alpha);
        Lerp(set->m_fPostFx2Red, m_fPostFx2Red); Lerp(set->m_fPostFx2Green, m_fPostFx2Green); Lerp(set->m_fPostFx2Blue, m_fPostFx2Blue); Lerp(set->m_fPostFx2Alpha, m_fPostFx2Alpha);
    }

    if (lodBoxA) { // 0x560E6F
        set->m_fLodDistMult = (float)(((double)lodBoxA->LodDistMult * (double)std::bit_cast<float>(0x3D000000u)) * (double)lodBoxA_T + (1.0 - (double)lodBoxA_T) * (double)set->m_fLodDistMult);
    }

    const auto FarClipBox = [&](const CTimeCycleBox* box, float T) { // 0x560EA5 / 0x560EE0
        const double clip = box->FarClip;
        const double v    = (clip < (double)set->m_fFarClip) ? clip : (double)set->m_fFarClip;
        set->m_fFarClip   = (float)(v * (double)T + (1.0 - (double)T) * (double)set->m_fFarClip);
    };
    if (farBoxA) {
        FarClipBox(farBoxA, farBoxA_T);
    }
    if (farBoxB) {
        FarClipBox(farBoxB, farBoxB_T);
    }

    // 0x560F1B - extra colour fade
    {
        const double inc   = (double)CTimer::GetTimeStep() * (double)std::bit_cast<float>(0x3C088889u);
        bool         apply = false;
        if (m_bExtraColourOn) {
            const double x = inc + (double)m_ExtraColourInter;
            if (1.0 < x) {
                m_ExtraColourInter = 1.0f;
                apply = true;
            } else {
                m_ExtraColourInter = (float)x;
                apply = m_ExtraColourInter > 0.0f;
            }
        } else {
            const double x = (double)m_ExtraColourInter - inc;
            if (0.0 > x) {
                m_ExtraColourInter = 0.0f;
            } else {
                m_ExtraColourInter = (float)x;
                apply = m_ExtraColourInter > 0.0f;
            }
        }
        if (apply) {
            const float   inv = (float)(1.0 - (double)m_ExtraColourInter);
            CColourSet    extra(m_ExtraColour, m_ExtraColourWeatherType);
            const bool    ignoreSky = m_nSkyTopRed[m_ExtraColour][m_ExtraColourWeatherType] == 0 && m_nSkyTopGreen[m_ExtraColour][m_ExtraColourWeatherType] == 0 && m_nSkyTopBlue[m_ExtraColour][m_ExtraColourWeatherType] == 0;
            set->Interpolate(set, &extra, inv, m_ExtraColourInter, ignoreSky);
        }
    }

    if (CWeather::UnderWaterness > 0.0f) { // 0x561003
        CColourSet current(currSampleIdx, 20);
        CColourSet next(nextSampleIdx, 20);
        CColourSet tmp{};
        tmp.Interpolate(&current, &next, invTimeT, timeT, false);
        set->Interpolate(set, &tmp, (float)(1.0 - (double)CWeather::UnderWaterness), CWeather::UnderWaterness, false);
    }

    if (CWeather::InTunnelness > 0.0f) { // 0x561124
        const int32 tHour = TunnelWeather % NUM_HOURS, tWeather = TunnelWeather / NUM_HOURS + WEATHER_EXTRA_START;
        CColourSet  tunnel(tHour, tWeather);
        const bool  ignoreSky = m_nSkyTopRed[tHour][tWeather] == 0 && m_nSkyTopGreen[tHour][tWeather] == 0 && m_nSkyTopBlue[tHour][tWeather] == 0;
        set->Interpolate(set, &tunnel, (float)(1.0 - (double)CWeather::InTunnelness), CWeather::InTunnelness, ignoreSky);
    }

    // 0x5611C2 - ambient: ftol(channel) * (1/255f): the channels are TRUNCATED to integers first
    for (float* v : { &set->m_fAmbientRed, &set->m_fAmbientGreen, &set->m_fAmbientBlue, &set->m_fAmbientRed_Obj, &set->m_fAmbientGreen_Obj, &set->m_fAmbientBlue_Obj }) {
        *v = (float)((double)(int32)*v * (double)std::bit_cast<float>(0x3B808081u));
    }

    // 0x561256
    CShadows::CalcPedShadowValues(
        m_VectorToSun[m_CurrentStoredValue],
        m_fShadowFrontX[m_CurrentStoredValue],
        m_fShadowFrontY[m_CurrentStoredValue],
        m_fShadowSideX[m_CurrentStoredValue],
        m_fShadowSideY[m_CurrentStoredValue],
        m_fShadowDisplacementX[m_CurrentStoredValue],
        m_fShadowDisplacementY[m_CurrentStoredValue]
    );

    if (TheCamera.m_mCameraMatrix.GetForward().z < -0.9f || !CWeather::bScriptsForceRain
        && (CCullZones::PlayerNoRain() || CCullZones::CamNoRain() || CCutsceneMgr::ms_running)) {
        m_FogReduction = std::min(m_FogReduction + 1, 64);
    } else {
        m_FogReduction = std::max(m_FogReduction - 1, 0);
    }

    // 0x56131B - far clip by altitude (z >= 200): z <= 500: (1 - t) * farClip + t * 1000 for farClip > 1000; above: min(farClip, 1000)
    if (!(camPos.z < 200.0f)) {
        const double fc = set->m_fFarClip;
        if (!(camPos.z > 500.0f)) {
            if (fc > 1000.0) {
                const double tt = ((double)camPos.z - 200.0f) * (double)std::bit_cast<float>(0x3B5A740Eu);
                set->m_fFarClip = (float)((1.0 - tt) * fc + tt * 1000.0f);
            }
        } else {
            set->m_fFarClip = (fc < 1000.0) ? (float)fc : 1000.0f;
        }
    }

    // 0x5613A6 - the colour below the horizon: an integer grey (ftol, low byte) blended with the previous sky-bottom colour
    {
        const int32 grey = (int32)((double)GreyValuesDuringDay[currSampleIdx] * invTimeT + (double)GreyValuesDuringDay[nextSampleIdx] * timeT);
        const double uw  = CWeather::UnderWaterness;
        const double g   = (double)(int32)(uint8)grey * (1.0 - uw);
        m_BelowHorizonGrey.red   = (uint8)(int32)((double)m_CurrentColours.m_nSkyBottomRed   * uw + g);
        m_BelowHorizonGrey.green = (uint8)(int32)((double)m_CurrentColours.m_nSkyBottomGreen * uw + g);
        m_BelowHorizonGrey.blue  = (uint8)(int32)((double)m_CurrentColours.m_nSkyBottomBlue  * uw + g);
    }

    set->m_fAmbientBeforeBrightnessRed   = set->m_fAmbientRed;
    set->m_fAmbientBeforeBrightnessGreen = set->m_fAmbientGreen;
    set->m_fAmbientBeforeBrightnessBlue  = set->m_fAmbientBlue;

    // 0x561468 - brightness setting; `fm` stays on the x87 stack (extended)
    const double brightness = (double)FrontEndMenuManager.m_PrefsBrightness;
    double fm;
    const auto MaxRGB = [&]() { // red > green ? red : green, then (that > blue) ? that : blue
        const double m1 = (set->m_fAmbientRed > set->m_fAmbientGreen) ? (double)set->m_fAmbientRed : (double)set->m_fAmbientGreen;
        return (m1 > (double)set->m_fAmbientBlue) ? m1 : (double)set->m_fAmbientBlue;
    };
    if (!(brightness < 256.0f)) {
        fm = (brightness - 256.0f) * (double)std::bit_cast<float>(0x3C000000u) + 1.0f; // 1/128
        const double add = MaxRGB() * fm - MaxRGB();
        set->m_fAmbientRed   = (float)((double)set->m_fAmbientRed   + add);
        set->m_fAmbientGreen = (float)((double)set->m_fAmbientGreen + add);
        set->m_fAmbientBlue  = (float)((double)set->m_fAmbientBlue  + add);
    } else {
        fm = (brightness * (double)std::bit_cast<float>(0x3B800000u)) * (double)std::bit_cast<float>(0x3F4CCCCDu) + (double)std::bit_cast<float>(0x3E4CCCCDu); // 1/256, 0.8f, 0.2f
        set->m_fAmbientRed   = (float)((double)set->m_fAmbientRed   * fm);
        set->m_fAmbientGreen = (float)((double)set->m_fAmbientGreen * fm);
        set->m_fAmbientBlue  = (float)((double)set->m_fAmbientBlue  * fm);
    }

    if (fm > 1.0f) { // 0x5614FE
        const double bonus = (fm - 1.0f) * (double)std::bit_cast<float>(0x3D75C28Fu); // 0.06f
        double       mx    = MaxRGB();
        const float  r = set->m_fAmbientRed, g = set->m_fAmbientGreen, b = set->m_fAmbientBlue;
        if (mx == 0.0) {
            mx = 0.001f;
            set->m_fAmbientRed = set->m_fAmbientGreen = set->m_fAmbientBlue = 0.001f;
        }
        if (bonus > mx) {
            const double k = bonus / mx;
            set->m_fAmbientRed   = (float)((double)set->m_fAmbientRed   * k);
            set->m_fAmbientGreen = (float)((double)set->m_fAmbientGreen * k);
            set->m_fAmbientBlue  = (float)((double)set->m_fAmbientBlue  * k);
        }
        m_BrightnessAddedToAmbientRed   = set->m_fAmbientRed   - r;
        m_BrightnessAddedToAmbientGreen = set->m_fAmbientGreen - g;
        m_BrightnessAddedToAmbientBlue  = set->m_fAmbientBlue  - b;
    }

    // 0x5615D2 - level of detail outside the world bounds
    {
        double d = 0.0;
        if (point.x < -3000.0f) {
            d = -3000.0f - (double)point.x;
        } else if (!(point.x <= 3000.0f) && !std::isnan(point.x)) {
            d = (double)point.x - 3000.0f;
        }
        if (point.y < -3000.0f) {
            d = d - ((double)point.y + 3000.0f);
        } else if (!(point.y <= 3000.0f) && !std::isnan(point.y)) {
            d = d + ((double)point.y - 3000.0f);
        }

        if (d >= 1000.0f) {
            set->m_fLodDistMult = set->m_fLodDistMult + set->m_fLodDistMult;
        } else if (d >= 0.0f) {
            set->m_fLodDistMult = (float)((d * (double)std::bit_cast<float>(0x3A83126Fu) + 1.0f) * (double)set->m_fLodDistMult);
        }
    }

    SetConstantParametersForPostFX();
}

// 0x5616E0
float CTimeCycle::FindFarClipForCoors(CVector cameraPos) {
    CColourSet set{};
    bool extraOn = m_bExtraColourOn != 0;
    float extraInter = m_ExtraColourInter;
    m_bExtraColourOn = 0;
    m_ExtraColourInter = 0.0f;
    CalcColoursForPoint(cameraPos, &set);
    m_bExtraColourOn = extraOn;
    m_ExtraColourInter = extraInter;
    return set.m_fFarClip;
}

// 0x55FFD0
void CTimeCycle::FindTimeCycleBox(
    CVector         pos,
    CTimeCycleBox** curr,
    float*          interpolation,
    bool            isLOD,
    bool            isFarClip,
    CTimeCycleBox*  ignored
) {
    *curr          = nullptr;
    *interpolation = 0.0f;

    for (auto& v : GetBoxes()) {
        if (isLOD && v.LodDistMult == 32.f) { // 0x560013
            continue;
        }
        if (isFarClip && !v.FarClip) { // 0x560038
            continue;
        }
        if (ignored == &v) {
            continue;
        }

        // Check if the point is at least within the `FallOff`. Every compare is of the form `fcomp; test ah, 0x41/1; jp/jne` => an unordered (NaN) compare SKIPS the box
        const float falloffZ = (float)((double)v.Falloff * (double)std::bit_cast<float>(0x3EAAAAABu)); // 0x560085 (spilled)
        const auto  Skip = [&](int32 i, float tolerance) {
            return !((double)v.Box.m_vecMin[i] - tolerance <= (double)pos[i]) || !((double)v.Box.m_vecMax[i] + tolerance >= (double)pos[i]);
        };
        if (Skip(0, v.Falloff) || Skip(1, v.Falloff) || Skip(2, falloffZ)) {
            continue;
        }

        // Calculate the distance to the box (0x5600E6, inlined `GetShortestVectorDistToPt`): `p > max ? p - max : (p >= min || NaN ? 0 : min - p)`
        const auto Axis = [&](int32 i) -> double {
            if (pos[i] > v.Box.m_vecMax[i]) {
                return (double)pos[i] - v.Box.m_vecMax[i];
            }
            if (pos[i] < v.Box.m_vecMin[i]) {
                return (double)v.Box.m_vecMin[i] - pos[i];
            }
            return 0.0;
        };
        const double dx = Axis(0), dy = Axis(1), dz = Axis(2) * 3.0f;
        const double dist = std::sqrt((dz * dz + dy * dy) + dx * dx); // 0x560178
        if (!(dist <= 0.0)) { // `fcom 0; test ah, 0x41; jp`: NaN takes this branch
            const double t = 1.0 - dist / v.Falloff; // Point not inside the box, but within `FallOff`
            if (t > *interpolation) {
                *curr          = &v;
                *interpolation = (float)t;
            }
        } else { // Point inside the box
            *curr          = &v;
            *interpolation = 1.f;
        }
    }
}

// 0x560210
void CTimeCycle::SetConstantParametersForPostFX() {
    if (!CPostEffects::IsVisionFXActive())
        return;

    if (CPostEffects::m_bNightVision) {
        m_CurrentColours.m_nShadowStrength = 0;
        m_CurrentColours.m_nLightShadowStrength = 0;
        m_CurrentColours.m_nPoleShadowStrength = 0;
        m_CurrentColours.m_fAmbientRed = 0.0f;
        m_CurrentColours.m_fAmbientGreen = 0.4f;
        m_CurrentColours.m_fAmbientBlue = 0.0f;
        m_CurrentColours.m_fAmbientRed_Obj = 0.0f;
        m_CurrentColours.m_fAmbientGreen_Obj = 0.4f;
        m_CurrentColours.m_fAmbientBlue_Obj = 0.0f;
        m_CurrentColours.m_nSkyTopRed = 0;
        m_CurrentColours.m_nSkyTopGreen = 128;
        m_CurrentColours.m_nSkyTopBlue = 0;
        m_CurrentColours.m_nSkyBottomRed = 0;
        m_CurrentColours.m_nSkyBottomGreen = 128;
        m_CurrentColours.m_nSkyBottomBlue = 0;
    }

    if (CPostEffects::m_bInfraredVision) {
        m_CurrentColours.m_nShadowStrength = 0;
        m_CurrentColours.m_nLightShadowStrength = 0;
        m_CurrentColours.m_nPoleShadowStrength = 0;
        m_CurrentColours.m_fLightsOnGroundBrightness = 0.0f;
        m_CurrentColours.m_nHighLightMinIntensity = 0;
        m_CurrentColours.m_nWaterFogAlpha = 0;
        m_CurrentColours.m_fAmbientRed = 0.0f;
        m_CurrentColours.m_fAmbientGreen = 0.0f;
        m_CurrentColours.m_fAmbientBlue = 1.0f;
        m_CurrentColours.m_fAmbientRed_Obj = 0.0f;
        m_CurrentColours.m_fAmbientGreen_Obj = 0.0f;
        m_CurrentColours.m_fAmbientBlue_Obj = 1.0f;
        m_CurrentColours.m_nSkyTopRed = 0;
        m_CurrentColours.m_nSkyTopGreen = 0;
        m_CurrentColours.m_nSkyTopBlue = 128;
        m_CurrentColours.m_nSkyBottomRed = 0;
        m_CurrentColours.m_nSkyBottomGreen = 0;
        m_CurrentColours.m_nSkyBottomBlue = 128;
    }
}

float CTimeCycle::GetAmbientRed()   { return gfLaRiotsLightMult * m_CurrentColours.m_fAmbientRed; }   // 0x560330
float CTimeCycle::GetAmbientGreen() { return gfLaRiotsLightMult * m_CurrentColours.m_fAmbientGreen; } // 0x560340
float CTimeCycle::GetAmbientBlue()  { return gfLaRiotsLightMult * m_CurrentColours.m_fAmbientBlue; }  // 0x560350

float CTimeCycle::GetAmbientRed_Obj()   { return m_CurrentColours.m_fAmbientRed_Obj; }   // 0x560360
float CTimeCycle::GetAmbientGreen_Obj() { return m_CurrentColours.m_fAmbientGreen_Obj; } // 0x560370
float CTimeCycle::GetAmbientBlue_Obj()  { return m_CurrentColours.m_fAmbientBlue_Obj; }  // 0x560380

float CTimeCycle::GetAmbientRed_BeforeBrightness()   { return gfLaRiotsLightMult * m_CurrentColours.m_fAmbientBeforeBrightnessRed; }   // 0x560390
float CTimeCycle::GetAmbientGreen_BeforeBrightness() { return gfLaRiotsLightMult * m_CurrentColours.m_fAmbientBeforeBrightnessGreen; } // 0x5603A0
float CTimeCycle::GetAmbientBlue_BeforeBrightness()  { return gfLaRiotsLightMult * m_CurrentColours.m_fAmbientBeforeBrightnessBlue; }  // 0x5603B0
