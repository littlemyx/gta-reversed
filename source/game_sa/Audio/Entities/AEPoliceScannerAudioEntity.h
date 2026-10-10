#pragma once

#include "AEAudioEntity.h"

struct tScannerSlot {
    eSoundBank Bank{SND_BANK_UNK};
    eSoundID   SoundID{-1};

    // bad name?
    [[nodiscard]] bool IsActive() const {
        return Bank < 0 || SoundID < 0;
    }
};
template<typename T> inline constexpr T kScannerZero = std::bit_cast<T>(std::array<std::byte, sizeof(T)>{}); // an all-zero object: the exe's .bss value; the NSDMI -1 defaults must not run for these globals in detached mode
#define NUM_POLICE_SCANNER_SLOTS 5

class NOTSA_EXPORT_VTABLE CAEPoliceScannerAudioEntity : public CAEAudioEntity {
public:
    enum State : int16 { // NOTE: Is a 16 bit value (the byte after it - `s_bScannerDisabled` - is a separate variable)
        STATE_INITIAL = 0,
        ONE           = 1,
        TWO           = 2,
        THREE         = 3,
        FOUR          = 4,
        FIVE          = 5,
        SIX           = 6,
        SEVEN         = 7,
    };

    static inline NOTSA_GLOBAL(s_fVolumeOffset, 0xB61CF8, (float), {});
    static inline NOTSA_GLOBAL(s_bStoppingScanner, 0xB61CFC, (bool), {});
    static inline NOTSA_GLOBAL(s_pSound, 0xB61D00, (CAESound*), {});
    static inline NOTSA_GLOBAL(s_nAbortPlaybackTime, 0xB61D08, (uint32), {});
    static inline NOTSA_GLOBAL(s_nPlaybackStartTime, 0xB61D0C, (uint32), {});
    static inline NOTSA_GLOBAL(s_nSectionPlaying, 0xB61D04, (int16), {});

    static inline NOTSA_GLOBAL(s_SlotState, 0xB61D14, (int16[NUM_POLICE_SCANNER_SLOTS]), {});
    static inline NOTSA_GLOBAL(s_pCurrentSlots, 0xB61D10, (tScannerSlot*), {});
    static inline NOTSA_GLOBAL(s_ScannerSlotFirst, 0xB61D34, (tScannerSlot[NUM_POLICE_SCANNER_SLOTS]), { kScannerZero<tScannerSlot>, kScannerZero<tScannerSlot>, kScannerZero<tScannerSlot>, kScannerZero<tScannerSlot>, kScannerZero<tScannerSlot> });
    static inline NOTSA_GLOBAL(s_ScannerSlotSecond, 0xB61D20, (tScannerSlot[NUM_POLICE_SCANNER_SLOTS]), { kScannerZero<tScannerSlot>, kScannerZero<tScannerSlot>, kScannerZero<tScannerSlot>, kScannerZero<tScannerSlot>, kScannerZero<tScannerSlot> });

    static inline NOTSA_GLOBAL(s_pPSControlling, 0xB61D48, (CAEPoliceScannerAudioEntity*), {});
    static inline NOTSA_GLOBAL(s_nScannerPlaybackState, 0xB61D4C, (CAEPoliceScannerAudioEntity::State), {});
    static inline NOTSA_GLOBAL(s_bScannerDisabled, 0xB61D4E, (bool), {});
    static inline NOTSA_GLOBAL(s_NextNewScannerDialogueTime, 0xB61D50, (uint32), {});

public:
    CAEPoliceScannerAudioEntity() = default; // 0x56DA00
    ~CAEPoliceScannerAudioEntity();

    static void StaticInitialise();
    static void Reset();

    void AddAudioEvent(eAudioEvents event, eCrimeType crimeType, const CVector& point);

    static void PrepSlots();
    static void LoadSlots();

    static void EnableScanner();
    static void DisableScanner(bool a1, bool bStopSound);
    static void StopScanner(bool bStopSound);

    static void FinishedPlayingScannerDialogue();
    void PlayLoadedDialogue();

    static void PopulateScannerDialogueLists(const tScannerSlot* first, const tScannerSlot* second);
    static bool CanWePlayNewScannerDialogue();
    void PlayPoliceScannerDialogue(tScannerSlot* first, tScannerSlot* second);

    void UpdateParameters(CAESound* sound, int16 curPlayPos) override;

    static void Service();

private:
    friend void InjectHooksMain();
    static void InjectHooks();

    CAEPoliceScannerAudioEntity* Constructor();
    CAEPoliceScannerAudioEntity* Destructor();

};

VALIDATE_SIZE(CAEPoliceScannerAudioEntity, 0x7C);
