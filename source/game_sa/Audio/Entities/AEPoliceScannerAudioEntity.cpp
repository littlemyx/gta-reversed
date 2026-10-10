#include "StdInc.h"

#include "AEPoliceScannerAudioEntity.h"

#include "AEAudioHardware.h"
#include "AEAudioUtility.h"
#include "AESoundManager.h"
#include "TheZones.h"
#include "Zone.h"

namespace {
//! Zone -> scanner "area" sound (`SND_BANK_SCRIPT_SCANNER_AREAS`) lookup
struct tScannerZone {
    char  Name[8];      //!< Compared against `CZone::m_TextLabel`
    int16 Sound;        //!< Sound ID in the areas bank (If negative the zone is skipped)
    bool  HasDirection; //!< Whether the "north of"/"west of"/... directions are said
};

// 0x8C8198 (Names), 0x8C87A8 (Sounds), 0x8C8930 (Directions)
constexpr tScannerZone SCANNER_ZONES[194] = {
    { "ALDEA", 0, true },
    { "ANGPI", 1, true },
    { "ARCO", 2, true },
    { "CUNTC", 3, false },
    { "BACKO", 4, true },
    { "BATTP", 5, true },
    { "SUNNN", 6, true },
    { "SUNMA", 7, false },
    { "BYTUN", 8, true },
    { "BEACO", 9, true },
    { "BFLD", 10, true },
    { "BFC", 11, true },
    { "BINT", 12, true },
    { "BLUAC", 13, true },
    { "BLUEB", 14, true },
    { "BONE", 15, true },
    { "CALI", 16, false },
    { "CALT", 17, true },
    { "CALT", 18, true },
    { "CITYS", 19, false },
    { "LOT", 20, false },
    { "COM", 21, true },
    { "CONF", 22, false },
    { "CRANB", 23, false },
    { "DILLI", 24, true },
    { "DOH", 25, true },
    { "SFDWT", 26, true },
    { "LDT", 27, true },
    { "ELS", 28, false },
    { "EBE", 29, false },
    { "EASB", 30, true },
    { "SFAIR", 31, false },
    { "EBAY", 32, false },
    { "ETUNN", 33, false },
    { "ELCA", 34, true },
    { "ELCO", 35, true },
    { "ELQUE", 36, true },
    { "ESPE", 37, false },
    { "ESPN", 38, false },
    { "HAUL", 39, true },
    { "FALLO", 40, false },
    { "FERN", 41, true },
    { "FINA", 42, true },
    { "FISH", 43, true },
    { "FLINTC", 44, true },
    { "FLINTI", 45, true },
    { "FLINTR", 46, true },
    { "FLINW", 47, true },
    { "CARSO", 48, false },
    { "SILLY", 49, true },
    { "FRED", 50, false },
    { "GAN", 51, true },
    { "GANTB", 52, false },
    { "GARC", 53, true },
    { "GARV", 54, false },
    { "GLN", 55, true },
    { "GGC", 56, false },
    { "PALMS", 57, true },
    { "HBARNS", 58, true },
    { "HANKY", 59, true },
    { "HGP", 60, true },
    { "HASH", 61, true },
    { "TOPFA", 62, false },
    { "QUARY", 63, false },
    { "IWD", 64, true },
    { "JTE", 65, false },
    { "JTN", 66, false },
    { "JTS", 67, false },
    { "JTW", 68, false },
    { "JUNIHI", 69, true },
    { "JUNIHO", 70, true },
    { "KACC", 71, false },
    { "KINC", 72, false },
    { "THEA", 73, true },
    { "BARRA", 74, true },
    { "BRUJA", 75, true },
    { "PAYAS", 76, true },
    { "LDM", 77, false },
    { "LEAFY", 79, true },
    { "PROBE", 80, false },
    { "LDS", 81, true },
    { "LINDEN", 82, false },
    { "LMEX", 83, true },
    { "CHC", 84, true },
    { "LFL", 85, true },
    { "LSINL", 87, false },
    { "LAIR", 88, false },
    { "???", 89, true },
    { "VAIR", 90, false },
    { "LVA", 91, false },
    { "MAR", 92, false },
    { "MKT", 93, false },
    { "MARKST", 94, false },
    { "MART", 95, false },
    { "HILLP", 96, true },
    { "MONT", 97, true },
    { "MONINT", 98, true },
    { "MTCHI", 99, true },
    { "MUL", 100, true },
    { "MULINT", 101, true },
    { "NROCK", 102, true },
    { "LDOC", 103, true },
    { "OCEAF", 104, false },
    { "OCTAN", 105, false },
    { "OVS", 106, true },
    { "OPEN", 107, false },
    { "BAYV", 108, true },
    { "PALO", 109, true },
    { "PARA", 110, true },
    { "PILG", 111, true },
    { "PINT", 112, true },
    { "PLS", 113, true },
    { "PRP", 114, true },
    { "????", 115, true },
    { "RIE", 116, false },
    { "RED", 117, true },
    { "REDE", 118, false },
    { "REDW", 119, false },
    { "TOM", 120, false },
    { "RIH", 121, true },
    { "ROCE", 122, true },
    { "RSE", 123, false },
    { "RSW", 124, false },
    { "ROD", 125, true },
    { "ROY", 126, false },
    { "SASO", 127, false },
    { "SANB", 129, true },
    { "CIVI", 130, true },
    { "SMB", 131, true },
    { "CREEK", 132, true },
    { "SHERR", 133, false },
    { "SRY", 134, false },
    { "SPIN", 135, true },
    { "STAR", 136, false },
    { "SUN", 137, true },
    { "SUN", 138, true },
    { "BIGE", 139, false },
    { "CAM", 140, false },
    { "RING", 141, false },
    { "ISLE", 142, false },
    { "FARM", 143, false },
    { "DRAG", 144, false },
    { "HIGH", 145, false },
    { "MAKO", 146, false },
    { "PANOP", 147, false },
    { "PINK", 148, false },
    { "PIRA", 149, false },
    { "DAM", 150, false },
    { "VISA", 151, false },
    { "ROBAD", 152, true },
    { "UNITY", 153, false },
    { "VALLE", 154, true },
    { "BLUF", 155, false },
    { "MEAD", 156, true },
    { "VERO", 157, true },
    { "VIN", 158, true },
    { "WHET", 159, true },
    { "WWE", 160, true },
    { "LIND", 161, true },
    { "YBELL", 162, false },
    { "YELLOW", 163, false },
    { "VE", 78, true },
    { "LA", 86, true },
    { "SF", 128, true },
    { "SAN_AND", -1, true },
    { "JEF", -1, true },
    { "PER1", 86, false },
    { "YBELL1", 162, false },
    { "YBELL2", 162, false },
    { "STRIP", 78, false },
    { "WESTP", -1, true },
    { "CUNTC1", 3, false },
    { "CUNTC2", 3, false },
    { "CUNTC3", 3, false },
    { "KINC1", 72, false },
    { "KINC2", 72, false },
    { "GARV1", 54, false },
    { "GARV2", 54, false },
    { "MONT1", 97, false },
    { "MTCHI1", 99, false },
    { "MTCHI2", 99, false },
    { "MTCHI3", 99, false },
    { "MTCHI4", 99, false },
    { "SHACA", -1, true },
    { "REST", -1, true },
    { "SANB2", 129, false },
    { "SFBAG1", 31, false },
    { "SFBAG2", 31, false },
    { "SFBAG3", 31, false },
    { "LVBAG", 90, false },
    { "LBAG1", 88, false },
    { "LBAG2", 88, false },
    { "LBAG3", 88, false },
    { "CONST1", 78, false },
};

// 0x8C8160 - Random "instruction" (what the officer does) sounds
constexpr int16 SCANNER_INSTRUCTION_SOUNDS[4] = { 0, 5, 8, 0 };

// 0x8C8168 - Crime type (2..22) -> number sound
constexpr int16 SCANNER_CRIME_SOUNDS[23] = { -1, -1, 0, 7, 5, 8, 2, 1, 1, 1, 0, 8, 10, 3, 8, 3, 10, 4, 5, 8, 9, 6, 6 };

// 0x8C89F8 - `eAEVehicleAudioTypeForName` -> vehicle sound (`SND_BANK_SCRIPT_SCANNER_VEHICLES`)
constexpr int16 SCANNER_VEHICLE_SOUNDS[46] = { 34, 0, 47, 44, 49, -1, 56, 17, 20, 30, 31, 57, 2, 25, 52, 36, 27, 7, 39, 6, 11, 51, 14, 24, 56, 32, 55, 22, 37, 33, 40, 15, 8, 18, 53, 12, 23, 29, 38, 54, 26, 5, 43, 41, 10, 1 };

// 0x8C8A54 - `eAEVehicleAudioTypeForName` -> whether the vehicle's colour is mentioned
constexpr bool SCANNER_VEHICLE_HAS_COLOUR[46] = { true, true, true, true, true, true, true, false, false, true, true, true, true, true, false, true, true, true, false, true, true, false, true, false, true, true, false, true, true, true, true, true, false, true, true, true, true, true, false, false, true, true, true, false, true, true };

// 0x8C8A88 - Vehicle colour ID -> colour sound (`SND_BANK_SCRIPT_SCANNER_COLOURS`)
constexpr int16 SCANNER_COLOUR_SOUNDS[127] = {
    0, 14, 1, 12, 1, 6, -1, 1, 1, 10, 1, 9, 1, 9, 10, 9,
    8, 12, 12, 10, 1, 12, 12, 9, 9, 6, 9, 9, 1, 9, 6, 12,
    1, 9, 10, 9, 6, 8, 10, 1, 6, 6, 12, 12, 12, 12, 10, 8,
    10, 9, 9, 8, 8, 1, 1, 6, 1, 10, 12, 1, 10, 10, 12, 9,
    9, 10, 6, 1, 10, 10, 12, 1, 9, 10, 12, 6, 9, 10, 12, 1,
    12, 10, 12, 8, 6, 12, 10, 1, 12, 10, 13, 1, 9, 10, 1, 1,
    13, 1, 1, 10, 1, 6, 10, 1, 10, 9, 1, 10, 10, 9, 6, 9,
    1, 6, 8, 12, 1, 12, 10, 6, 10, 12, 9, 6, 12, 1, 11,
};
}

// 0x4E6E00
CAEPoliceScannerAudioEntity::~CAEPoliceScannerAudioEntity() {
    if (s_pPSControlling == this && s_nScannerPlaybackState != STATE_INITIAL) {
        s_bStoppingScanner = true;
        if (s_pSound) {
            s_pSound->StopSoundAndForget();
            s_pSound = nullptr;
        }
        FinishedPlayingScannerDialogue();
    }
}

// 0x5B9C30
void CAEPoliceScannerAudioEntity::StaticInitialise() {
    s_NextNewScannerDialogueTime = 0;
    s_bScannerDisabled           = false;
    s_nScannerPlaybackState      = STATE_INITIAL;
    s_pPSControlling             = nullptr;
    s_pCurrentSlots              = nullptr;
    s_fVolumeOffset              = 0.0f;
}

// 0x4E6E90
void CAEPoliceScannerAudioEntity::Reset() {
    StopScanner(true);
    FinishedPlayingScannerDialogue();
}

// 0x4E71E0
void CAEPoliceScannerAudioEntity::AddAudioEvent(eAudioEvents event, eCrimeType crimeType, const CVector& point) {
    tScannerSlot first[NUM_POLICE_SCANNER_SLOTS]{};
    tScannerSlot second[NUM_POLICE_SCANNER_SLOTS]{};

    if (event != AE_CRIME_COMMITTED) {
        return;
    }

    if ((int32)crimeType <= 1 || (int32)crimeType >= 23) {
        return;
    }

    // "<Unit>, we have a <crime number> in <area>" (-ish)
    first[0] = { SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS, SCANNER_INSTRUCTION_SOUNDS[CAEAudioUtility::GetRandomNumberInRange(0, 3)] };
    first[1] = { SND_BANK_SCRIPT_SCANNER_NUMBERS,      SCANNER_CRIME_SOUNDS[(int32)crimeType] };

    const auto zone = CTheZones::FindSmallestZoneForPosition(point, true);
    if (!zone) {
        return;
    }

    size_t zoneIdx = 0;
    for (; zoneIdx < std::size(SCANNER_ZONES); zoneIdx++) {
        const auto& sz = SCANNER_ZONES[zoneIdx];
        if (std::memcmp(zone->m_TextLabel, sz.Name, sizeof(sz.Name)) == 0 && sz.Sound >= 0) {
            break;
        }
    }
    if (zoneIdx == std::size(SCANNER_ZONES)) {
        return;
    }

    // 0x4E7301 - Direction from the center of the zone
    bool hasDirection = false;
    if (SCANNER_ZONES[zoneIdx].HasDirection) {
        const auto sizeX = (float)(zone->m_fX2 - zone->m_fX1);
        const auto sizeY = (float)(zone->m_fY2 - zone->m_fY1);

        const float centerX = sizeX * 0.5f + (float)zone->m_fX1;
        const float centerY = 0.5f * sizeY + (float)zone->m_fY1;
        const float halfX   = sizeX * 0.25f;
        const float halfY   = sizeY * 0.25f;

        if (point.y > centerY + halfY) {
            first[2] = { SND_BANK_SCRIPT_SCANNER_DIRECTIONS, 2 };
            hasDirection = true;
        } else if (point.y < centerY - halfY) {
            first[2] = { SND_BANK_SCRIPT_SCANNER_DIRECTIONS, 3 };
            hasDirection = true;
        }

        if (halfX + centerX < point.x) {
            first[3] = { SND_BANK_SCRIPT_SCANNER_DIRECTIONS, 1 };
        } else if (centerX - halfX > point.x) {
            first[3] = { SND_BANK_SCRIPT_SCANNER_DIRECTIONS, 4 };
        } else if (!hasDirection) {
            first[3] = { SND_BANK_SCRIPT_SCANNER_DIRECTIONS, 0 };
        }
    }

    // 0x4E73F5
    first[4] = { SND_BANK_SCRIPT_SCANNER_AREAS, SCANNER_ZONES[zoneIdx].Sound };

    // Find the player whose scanner this is
    CPed* ped = nullptr;
    for (auto i = 0; i < 2; i++) {
        const auto wanted = FindPlayerWanted(i);
        if (wanted && &wanted->m_PoliceScannerAudioEntity == this) {
            ped = FindPlayerPed(i);
        }
    }

    // "<Suspect> is on foot / in a <colour> <vehicle>"
    if (ped) { // 0x4E743F
        if (ped->bInVehicle) { // 0x4E7454
            const auto veh = ped->m_pVehicle;
            if (veh) {
                const auto vehType = (int32)veh->m_vehicleAudio.m_AuSettings.VehicleAudioTypeForName;
                if (vehType >= 0 && vehType < 46 && SCANNER_VEHICLE_SOUNDS[vehType] >= 0) {
                    second[0] = { SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS, 7 };
                    second[1] = { SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS, 1 };
                    switch (vehType) { // Two wheelers & co.
                    case AE_VAT_MOPED:
                    case AE_VAT_BIKE:
                    case AE_VAT_QUADBIKE:
                    case AE_VAT_MOWER:
                    case AE_VAT_BICYCLE:
                    case AE_VAT_TRACTOR:
                        second[1].SoundID = 3;
                        break;
                    }

                    if (SCANNER_VEHICLE_HAS_COLOUR[vehType]) {
                        if (veh->GetRemapIndex() != -1) {
                            second[3] = { SND_BANK_SCRIPT_SCANNER_COLOURS, 4 };
                        } else {
                            const auto colour = veh->m_nPrimaryColor;
                            if (colour > 0 && colour < 127 && SCANNER_COLOUR_SOUNDS[colour] >= 0) {
                                second[3] = { SND_BANK_SCRIPT_SCANNER_COLOURS, SCANNER_COLOUR_SOUNDS[colour] };
                            }
                        }
                    }

                    second[4] = { SND_BANK_SCRIPT_SCANNER_VEHICLES, SCANNER_VEHICLE_SOUNDS[vehType] };
                }
            }
        } else { // 0x4E7529
            const auto intel = ped->GetIntelligence();
            if (intel->GetTaskSwim()) {
                second[0] = { SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS, 7 };
                second[4] = { SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS, 2 };
            } else if (!intel->GetTaskJetPack()) {
                second[0] = { SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS, 7 };
                second[4] = { SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS, 4 };
            }
        }
    }

    PlayPoliceScannerDialogue(first, second);
}

// 0x4E6BC0
void CAEPoliceScannerAudioEntity::PrepSlots() {
    if (!s_pCurrentSlots) {
        return;
    }
    for (auto i = 0; i < NUM_POLICE_SCANNER_SLOTS; i++) {
        s_SlotState[i] = s_pCurrentSlots[i].IsActive();
    }
}

// 0x4E6CD0
void CAEPoliceScannerAudioEntity::LoadSlots() {
    if (!s_pCurrentSlots) {
        return;
    }

    bool canPlay = true;
    for (auto i = 0; i < NUM_POLICE_SCANNER_SLOTS; i++) {
        const auto slot = (eSoundBankSlot)(SND_BANK_SLOT_SCANNER_FIRST + i);
        auto& currentSlot = s_pCurrentSlots[i];

        if (s_SlotState[i]) {
            if (s_SlotState[i] == 2) {
                bool loaded = AEAudioHardware.IsSoundLoaded(currentSlot.Bank, currentSlot.SoundID, slot);
                if (loaded) {
                    s_SlotState[i] = 3;
                } else {
                    canPlay = false;
                }
            }
        } else if (currentSlot.IsActive()) {
            s_SlotState[i] = 1;
        } else {
            if (!CStreaming::IsVeryBusy()) {
                AEAudioHardware.LoadSound(currentSlot.Bank, currentSlot.SoundID, slot);
                s_SlotState[i] = 2;
            }
            canPlay = false;
        }
    }
    if (canPlay) {
        s_nScannerPlaybackState = FOUR;
    }
}

// 0x4E6DB0
void CAEPoliceScannerAudioEntity::EnableScanner() {
    s_bScannerDisabled = false;
}

// 0x4E71B0
void CAEPoliceScannerAudioEntity::DisableScanner(bool a1, bool bStopSound) {
    s_bScannerDisabled = true;
    if (a1 && s_nScannerPlaybackState != STATE_INITIAL) {
        if (s_pPSControlling) {
            StopScanner(bStopSound);
        }
    }
}

// 0x4E6DC0
void CAEPoliceScannerAudioEntity::StopScanner(bool bStopSound) {
    if (s_nScannerPlaybackState == STATE_INITIAL)
        return;

    s_bStoppingScanner = true;
    if (bStopSound) {
        if (s_pSound) {
            s_pSound->StopSoundAndForget();
            s_pSound = nullptr;
        }
        FinishedPlayingScannerDialogue();
    }
}

// 0x4E6C30
void CAEPoliceScannerAudioEntity::FinishedPlayingScannerDialogue() {
    AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_NOISE_STOP);
    s_nScannerPlaybackState      = STATE_INITIAL;
    s_pPSControlling             = nullptr;
    s_pCurrentSlots              = nullptr;
    s_bStoppingScanner           = false;
    s_NextNewScannerDialogueTime = CTimer::GetTimeInMS() + 10000; // 0x8C8140
    s_fVolumeOffset              = 0.0f;

    std::ranges::fill(s_SlotState, 1);

    rng::fill(s_ScannerSlotFirst, tScannerSlot{});
    rng::fill(s_ScannerSlotSecond, tScannerSlot{});
}

// 0x4E6F60
void CAEPoliceScannerAudioEntity::PlayLoadedDialogue() {
    constexpr float clickVolume = 0.0f; // 0xB61D54 (Never written to)

    // Find the slot that's currently being played (If any), otherwise start from the first one
    int16 i = 0;
    for (; i < NUM_POLICE_SCANNER_SLOTS; ++i) {
        if (s_SlotState[i] == FIVE) {
            break;
        }
    }
    if (i == NUM_POLICE_SCANNER_SLOTS) {
        i = 0;
    }

    // Find the next loaded slot
    for (; i < NUM_POLICE_SCANNER_SLOTS; ++i) {
        if (s_SlotState[i] == THREE) {
            break;
        }
    }

    if (i >= NUM_POLICE_SCANNER_SLOTS) { // 0x4E6FF8 - Nothing left to play in this section
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_CLICK, clickVolume + s_fVolumeOffset, 1.0f);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_NOISE_STOP, 0.0f, 1.0f);
        if (s_nSectionPlaying) {
            FinishedPlayingScannerDialogue();
        } else { // Start playing the second section
            s_pCurrentSlots   = s_ScannerSlotSecond;
            s_nSectionPlaying = 1;
            for (auto j = 0; j < NUM_POLICE_SCANNER_SLOTS; j++) {
                s_SlotState[j] = s_pCurrentSlots[j].IsActive() ? 1 : 0;
            }
            s_nPlaybackStartTime    = 0;
            s_nAbortPlaybackTime    = 5000 + CTimer::GetTimeInMS(); // 0x8C8148
            s_nScannerPlaybackState = TWO;
        }
        return;
    }

    // 0x4E707F - Play the slot
    CAESound sound;
    sound.Initialise(
        (eSoundBankSlot)(SND_BANK_SLOT_SCANNER_FIRST + i),
        s_pCurrentSlots[i].SoundID,
        this,
        CVector{ 0.0f, 1.0f, 0.0f },
        GetDefaultVolume(AE_CRIME_COMMITTED) + s_fVolumeOffset,
        1.0f,
        1.0f,
        1.0f,
        0,
        SOUND_DEFAULT,
        0.0f,
        0
    );
    sound.m_ClientVariable = (float)i;
    sound.m_Flags          = SOUND_FRONT_END | SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES | SOUND_IS_DUCKABLE;
    sound.m_Event          = AE_CRIME_COMMITTED;

    s_pSound = AESoundManager.RequestNewSound(&sound);
    if (s_pSound) {
        s_SlotState[i]          = FIVE;
        s_nScannerPlaybackState = SEVEN;
    }
}

// 0x4E6B60
void CAEPoliceScannerAudioEntity::PopulateScannerDialogueLists(const tScannerSlot* first, const tScannerSlot* second) {
    assert(first && second);
    if (s_nScannerPlaybackState == STATE_INITIAL) {
        for (auto slotIndex = 0; slotIndex < NUM_POLICE_SCANNER_SLOTS; slotIndex++) {
            s_ScannerSlotFirst[slotIndex] = first[slotIndex];
            s_ScannerSlotSecond[slotIndex] = second[slotIndex];
        }
    }
}

// inlined
// 0x4E6C00
bool CAEPoliceScannerAudioEntity::CanWePlayNewScannerDialogue() {
    if (s_nScannerPlaybackState != STATE_INITIAL)
        return false;

    if (CTimer::GetTimeInMS() < s_NextNewScannerDialogueTime)
        return false;

    if (TheCamera.m_bWideScreenOn)
        return false;

    if (s_bScannerDisabled)
        return false;

    return true;
}

// 0x4E6ED0
void CAEPoliceScannerAudioEntity::PlayPoliceScannerDialogue(tScannerSlot* first, tScannerSlot* second) {
    assert(first && second);
    if (CanWePlayNewScannerDialogue()) { // todo: maybe little bit wrong
        PopulateScannerDialogueLists(first, second);
        s_pPSControlling = this;
        s_nSectionPlaying = 0;
        s_pCurrentSlots = s_ScannerSlotFirst;
        PrepSlots();
        s_nScannerPlaybackState = TWO;
        s_nPlaybackStartTime = CTimer::GetTimeInMS() + 2000; // todo: gSpeechContextLookup[365][6]
        s_nAbortPlaybackTime = CTimer::GetTimeInMS() + 5000; // todo: gSpeechContextLookup[366][0]
    }
}

// 0x4E7590
void CAEPoliceScannerAudioEntity::UpdateParameters(CAESound* sound, int16 curPlayPos) {
    if (curPlayPos == -1) {
        s_pSound = nullptr;
        if (s_bStoppingScanner) {
            if (s_nScannerPlaybackState != STATE_INITIAL) {
                s_bStoppingScanner = true; // V1048 [CWE-1164] The 's_bStoppingScanner' variable was assigned the same value
                FinishedPlayingScannerDialogue();
            }
            return;
        }
        PlayLoadedDialogue();
        return;
    }

    if (s_bStoppingScanner) {
        sound->m_Volume = sound->m_Volume - 6.0f; // todo: *(float*)&gSpeechContextLookup[366][4]
        return;
    }

    if (sound->m_Length > 0 && curPlayPos > sound->m_Length - 40 && sound->m_BankSlot != 37) { // todo: -40 should be replaced with by gSpeechContextLookup[366][2]
        sound->SetFlags(eSoundEnvironment::SOUND_REQUEST_UPDATES, false);
        s_pSound = nullptr;
        PlayLoadedDialogue();
        return;
    }
}

// 0x4E7630
void CAEPoliceScannerAudioEntity::Service() {
    static constexpr uint32 startDelay = 300;   // 0x8C8154
    static constexpr float noiseVolume = -6.0f; // 0x8C8158
    static constexpr float clickVolume = +0.0f; // 0xB61D54

    bool finishPlaying;
    if (TheCamera.m_bWideScreenOn && s_nScannerPlaybackState != STATE_INITIAL) {
        finishPlaying = true;
        s_bStoppingScanner = true;
    } else {
        finishPlaying = s_bStoppingScanner;
    }

    switch (s_nScannerPlaybackState) {
    case TWO:
        if (CTimer::GetTimeInMS() > s_nAbortPlaybackTime || finishPlaying) {
            FinishedPlayingScannerDialogue();
            break;
        }

        LoadSlots();
        break;
    case FOUR:
        if (finishPlaying) {
            FinishedPlayingScannerDialogue();
            break;
        }

        if (CTimer::GetTimeInMS() >= s_nPlaybackStartTime) {
            s_fVolumeOffset = CAEVehicleAudioEntity::s_pPlayerAttachedForRadio ? 0.0f : -8.0f; // 0x4E769B: tests the driver at 0xB6B98C. todo: -8 is gSpeechContextLookup[367][2]
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_CLICK,       s_fVolumeOffset + clickVolume, 1.0f);
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_NOISE_START, s_fVolumeOffset + noiseVolume, 1.0f); // todo: noiseVolume is gSpeechContextLookup[367][0]
            s_nPlaybackStartTime = startDelay + CTimer::GetTimeInMS(); // todo: startDelay is gSpeechContextLookup[366][6]
            s_nScannerPlaybackState = FIVE;
        }
        break;
    case FIVE:
        if (finishPlaying) {
            FinishedPlayingScannerDialogue();
            break;
        }

        if (CTimer::GetTimeInMS() >= s_nPlaybackStartTime && s_pPSControlling) {
            s_nScannerPlaybackState = SEVEN;
            s_pPSControlling->PlayLoadedDialogue();
        }
        break;
    default:
        return;
    }
}

void CAEPoliceScannerAudioEntity::InjectHooks() {
    RH_ScopedVirtualClass(CAEPoliceScannerAudioEntity, 0x85F368, 1);
    RH_ScopedCategory("Audio/Entities");

    RH_ScopedInstall(Constructor, 0x56DA00);
    RH_ScopedInstall(Destructor, 0x4E6E00);
    RH_ScopedInstall(StaticInitialise, 0x5B9C30);
    RH_ScopedInstall(Reset, 0x4E6E90);
    RH_ScopedInstall(AddAudioEvent, 0x4E71E0);
    RH_ScopedInstall(PrepSlots, 0x4E6BC0);
    RH_ScopedInstall(LoadSlots, 0x4E6CD0);
    RH_ScopedInstall(EnableScanner, 0x4E6DB0);
    RH_ScopedInstall(DisableScanner, 0x4E71B0);
    RH_ScopedInstall(StopScanner, 0x4E6DC0);
    RH_ScopedInstall(FinishedPlayingScannerDialogue, 0x4E6C30);
    RH_ScopedInstall(PlayLoadedDialogue, 0x4E6F60);
    RH_ScopedInstall(PopulateScannerDialogueLists, 0x4E6B60);
    RH_ScopedInstall(CanWePlayNewScannerDialogue, 0x4E6C00);
    RH_ScopedInstall(PlayPoliceScannerDialogue, 0x4E6ED0);
    RH_ScopedVMTInstall(UpdateParameters, 0x4E7590);
    RH_ScopedInstall(Service, 0x4E7630);
}

CAEPoliceScannerAudioEntity* CAEPoliceScannerAudioEntity::Constructor() {
    this->CAEPoliceScannerAudioEntity::CAEPoliceScannerAudioEntity();
    return this;
}

CAEPoliceScannerAudioEntity* CAEPoliceScannerAudioEntity::Destructor() {
    this->CAEPoliceScannerAudioEntity::~CAEPoliceScannerAudioEntity();
    return this;
}
