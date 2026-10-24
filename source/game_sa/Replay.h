#pragma once

#include "Bmx.h"
#include "CompressedMatrixNotAligned.h"
#include "PlayerInfo.h"
#include "ReplayBlockType.h"
#include "Vector.h"
#include "Vehicle.h"
#include "eReplay.h"
#include "ePedType.h"
#include "eWeatherType.h"

class CCamera;

enum eReplayBufferStatus : uint8 {
    REPLAYBUFFER_NOT_AVAILABLE = 0,
    REPLAYBUFFER_FULL = 1,
    REPLAYBUFFER_IN_USE = 2
};

// Used for compressing angle values while recording replays.
constexpr float HEADING_COMPRESS_VALUE = 40.764328f;

constexpr auto NUM_REPLAY_BUFFERS = 8u;
constexpr auto REPLAY_BUFFER_SIZE = 100'000u;

class CReplay {
public:
    // @notsa
    struct CVector_Reversed {
        float z, y, x;

        CVector_Reversed(const CVector& vec) {
            z = vec.z;
            y = vec.y;
            x = vec.x;
        }

        operator CVector() {
            return {x, y, z};
        }
    };
    VALIDATE_SIZE(CVector_Reversed, sizeof(CVector));

    // @notsa
    struct tReplayBuffer {
        std::array<uint8, REPLAY_BUFFER_SIZE> buffer;

        template <typename T>
            requires std::derived_from<T, tReplayBlockBase>
        T& Read(uint32 offset) const {
            auto* data = (T*)(buffer.data() + offset);
            if constexpr (!std::same_as<T, tReplayBlockBase>) {
                assert(data->type == T::Type);
            }
            return *data;
        }

        // Returns total bytes written
        template <typename T>
            requires std::derived_from<T, tReplayBlockBase> && (!std::same_as<tReplayBlockBase, T>)
        uint32 Write(uint32 offset, const T& data = {}) {
            if (T::Type == REPLAY_PACKET_END) {
                *(eReplayPacket*)(buffer.data() + offset) = REPLAY_PACKET_END;
                return 1;
            }
            constexpr auto size = FindSizeOfPacket(T::Type);

            T wr = data;
            wr.type = T::Type;
            std::memcpy(buffer.data() + offset, &wr, size);
            return size;
        }

        uint8& at(size_t index)             { return buffer.at(index); }
        const uint8& at(size_t index) const { return buffer.at(index); }

        // Returns its index from CReplay::Buffers array.
        size_t GetIndex() const  { return this - Buffers.data(); }   // element difference (the old extra "/ sizeof(tReplayBuffer)" made it 0 for every buffer)
        bool IsAvailable() const { return BufferStatus[GetIndex()] != REPLAYBUFFER_NOT_AVAILABLE; }

        struct Iterator {
            using value_type = tReplayBlockBase;
            using reference = value_type&;
            using pointer = value_type*;
            using iter_difference = ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            const tReplayBuffer* m_buffer{nullptr};
            uint32 m_offset{REPLAY_BUFFER_SIZE};

            Iterator() = default;
            Iterator(const tReplayBuffer* buffer, uint32 offset) : m_buffer(buffer), m_offset(offset) {}

            Iterator& operator++();

            Iterator operator++(int) {
                auto a = *this;
                ++*this;
                return a;
            }

            value_type& operator*() const {
                #ifdef NOTSA_DEBUG
                if (m_offset >= REPLAY_BUFFER_SIZE || !m_buffer) {
                    NOTSA_UNREACHABLE("Dereferencing of end()!");
                }
                #endif

                return *(value_type*)&m_buffer->at(m_offset);
            }

            value_type* operator->() const {
                #ifdef NOTSA_DEBUG
                if (m_offset >= REPLAY_BUFFER_SIZE || !m_buffer) {
                    NOTSA_UNREACHABLE("Dereferencing of end()!");
                }
                #endif

                return (value_type*)&m_buffer->at(m_offset);
            }

            bool operator==(const Iterator& rhs) const = default;
            bool operator!=(const Iterator& rhs) const = default;

            ptrdiff_t operator-(const Iterator& rhs) const {
                assert(m_buffer == rhs.m_buffer);
                return m_offset - rhs.m_offset;
            }
        };

        auto begin() const { return Iterator{this, 0}; }
        auto end() const   { return Iterator{this, REPLAY_BUFFER_SIZE}; }

    };

    class CAddressInReplayBuffer {
    public:
#ifdef __clang__
        // NOTSA: clang rejects 'default member initializer needed within definition of enclosing class' (Playback / Record below default-construct the nested type), so no DMIs and a
        // constexpr user ctor. Not used with cl.exe: there a user-provided ctor makes the static inline Playback / Record dynamically initialised (extra code in every TU).
        uint32 m_nOffset;
        tReplayBuffer* m_pBase;
        uint8 m_bSlot; // first slot to contain data

        constexpr CAddressInReplayBuffer() : m_nOffset(0), m_pBase(nullptr), m_bSlot(0) {}
#else
        uint32 m_nOffset{0};
        tReplayBuffer* m_pBase{nullptr};
        uint8 m_bSlot{0}; // first slot to contain data

        CAddressInReplayBuffer() = default;
#endif
        CAddressInReplayBuffer(tReplayBuffer& buffer, uint8 slot = 0u, uint32 offset = 0u)
            : m_pBase(&buffer), m_bSlot(slot), m_nOffset(offset)
        {}

        // Read helper
        template <typename T>
            requires std::derived_from<T, tReplayBlockBase>
        T& Read() const {
            return m_pBase->Read<T>(m_nOffset);
        }

        template <class T>
            requires std::derived_from<T, tReplayBlockBase>
        void Write(const T& data = {}) {
            const auto written = m_pBase->Write<T>(m_nOffset, data);
            if constexpr (T::Type != REPLAY_PACKET_END) {
                m_nOffset += written;
            }
        }

        void Next() {
            m_bSlot = GetNextSlot();
            m_pBase = &Buffers[m_bSlot];
            m_nOffset = 0u;
        }

        eReplayBufferStatus GetStatus() {
            return BufferStatus[m_bSlot];
        }

        uint8 GetNextSlot(uint8 stride = 1u) {
            return (stride + m_bSlot) % NUM_REPLAY_BUFFERS;
        }
    };

    static inline NOTSA_GLOBAL(Mode, 0xA43088, (eReplayMode), {});
    static inline NOTSA_GLOBAL(bReplayEnabled, 0x8A6160, (bool), { true });
    static inline NOTSA_GLOBAL(bPlayingBackFromFile, 0x97FAE1, (bool), {});
    static inline NOTSA_GLOBAL(bAllowLookAroundCam, 0x97FAE0, (bool), {});
    static inline NOTSA_GLOBAL(bDoLoadSceneWhenDone, 0x97FACC, (bool), {});
    static inline NOTSA_GLOBAL(bIsUsingRemoteCar, 0x97F66D, (bool), {});

    static inline NOTSA_GLOBAL(pReferences, 0x97FB0C, (void*), {});
    static inline NOTSA_GLOBAL(pPickups, 0x97FB08, (void*), {});
    static inline NOTSA_GLOBAL(pEmptyReferences, 0x97FB04, (void*), {});
    static inline NOTSA_GLOBAL(pWorld1, 0x97FB00, (void*), {});
    static inline NOTSA_GLOBAL(pStoredCam, 0x97FAF8, (CCamera*), {});
    static inline NOTSA_GLOBAL(pRadarBlips, 0x97FAF4, (void*), {});

    static inline NOTSA_GLOBAL(Time1, 0x97FB60, (uint32), {});
    static inline NOTSA_GLOBAL(Time2, 0x97FB5C, (uint32), {});
    static inline NOTSA_GLOBAL(Time3, 0x97FB58, (uint32), {});
    static inline NOTSA_GLOBAL(Time4, 0x97FB54, (uint32), {});
    static inline NOTSA_GLOBAL(Time5, 0x97FB50, (uint32), {});
    static inline NOTSA_GLOBAL(Time6, 0x97FB4C, (uint32), {});
    static inline NOTSA_GLOBAL(Time7, 0x97FB48, (uint32), {});
    static inline NOTSA_GLOBAL(Frame, 0x97FB44, (uint32), {});
    static inline NOTSA_GLOBAL(TimeStepNonClipped, 0x97FB34, (float), {});
    static inline NOTSA_GLOBAL(TimeStep, 0x97FB38, (float), {});
    static inline NOTSA_GLOBAL(TimeScale, 0x97FB3C, (float), {});

    // these are overwritten when OldWeatherType changes for some reason
    // they have no unreversed xref anyways so it's fine.
    // inline static auto& ClockHours = StaticRef<uint8>(0x97FAEF);
    // inline static auto& ClockMinutes = StaticRef<uint8>(0x97FAEE);
    inline static uint8 ClockHours{};
    inline static uint8 ClockMinutes{};

    static inline NOTSA_GLOBAL(OldWeatherType, 0x97FAEC, (eWeatherType), {});
    static inline NOTSA_GLOBAL(NewWeatherType, 0x97FAE8, (eWeatherType), {});
    static inline NOTSA_GLOBAL(WeatherInterpolationValue, 0x97FAE4, (float), {});

    static inline NOTSA_GLOBAL(ms_nNumCivMale_Stored, 0x97FAB4, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nNumCivFemale_Stored, 0x97FAB0, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nNumCop_Stored, 0x97FAAC, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nNumEmergency_Stored, 0x97FAA8, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nNumDealers_Stored, 0x97FA7C, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nTotalCarPassengerPeds_Stored, 0x97FA78, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nTotalCivPeds_Stored, 0x97FA74, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nTotalGangPeds_Stored, 0x97FA70, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nTotalPeds_Stored, 0x97FA6C, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nTotalMissionPeds_Stored, 0x97FA68, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nNumGang_Stored, 0x97FA80, (std::array<uint32, TOTAL_GANGS>), {});

    static inline NOTSA_GLOBAL(PlayerInfo, 0xA430B0, (CPlayerInfo), {}); VALIDATE_SIZE(PlayerInfo, 400);

    static inline NOTSA_GLOBAL(m_PedPoolConversion, 0x97F838, (std::array<int32, 140>), {});
    static inline NOTSA_GLOBAL(m_VehiclePoolConversion, 0x97F680, (std::array<int32, 110>), {});

    static inline NOTSA_GLOBAL(CameraFocus, 0x97F670, (CVector_Reversed), { CVector{ 0.0f, 0.0f, 0.0f } }); // no default ctor
    static inline NOTSA_GLOBAL(CameraFixed, 0x97FB24, (CVector_Reversed), { CVector{ 0.0f, 0.0f, 0.0f } }); // no default ctor
    static inline NOTSA_GLOBAL(LoadScene, 0x97FAC0, (CVector_Reversed), { CVector{ 0.0f, 0.0f, 0.0f } }); // no default ctor
    static inline NOTSA_GLOBAL(CameraMode, 0x97FB30, (eReplayCamMode), {});
    static inline NOTSA_GLOBAL(FramesActiveLookAroundCam, 0x97FAD0, (int32), {});
    static inline NOTSA_GLOBAL(OldRadioStation, 0x97FABC, (eRadioID), {});
    static inline NOTSA_GLOBAL(CurrArea, 0x97FAB8, (int8), {});
    static inline NOTSA_GLOBAL(BufferStatus, 0x97FB7C, (std::array<eReplayBufferStatus, NUM_REPLAY_BUFFERS>), {});

    static inline NOTSA_GLOBAL(Buffers, 0x97FB88, (std::array<tReplayBuffer, NUM_REPLAY_BUFFERS>), {});

    static inline NOTSA_GLOBAL(Playback, 0x97FB64, (CAddressInReplayBuffer), {});
    static inline NOTSA_GLOBAL(Record, 0x97FB70, (CAddressInReplayBuffer), {});

public:
    static void InjectHooks();

    static void Init();
    static void Update();
    static void DisableReplays();
    static void EnableReplays();
    static void StorePedAnimation(CPed* ped, CStoredAnimationState& state);
    static void StorePedUpdate(CPed* ped, uint8 index);
    static void RetrievePedAnimation(CPed* ped, const CStoredAnimationState& state);
    static void Display();
    static void MarkEverythingAsNew();
    static void EmptyReplayBuffer();
    static void EmptyPedsAndVehiclePools_NoDestructors();
    static void GoToNextBlock();
    static void RecordVehicleDeleted(CVehicle* vehicle);
    static void RecordPedDeleted(CPed* ped);
    static void InitialiseVehiclePoolConversionTable();
    static void InitialisePedPoolConversionTable();
    static void InitialisePoolConversionTables();
    static void SaveReplayToHD();
    static void PlayReplayFromHD();
    static bool ShouldStandardCameraBeProcessed();
    static void ProcessPedUpdate(CPed* ped, float a2, CAddressInReplayBuffer& address);
    static void ProcessReplayCamera();
    static void ProcessLookAroundCam();
    static int32 FindPoolIndexForPed(int32 index);
    static int32 FindPoolIndexForVehicle(int32 index);
    static bool CanWeFindPoolIndexForPed(int32 index);
    static bool CanWeFindPoolIndexForVehicle(int32 index);
    static void StorePlayerInfoVariables();
    static void StoreStuffInMem();
    static void RestorePlayerInfoVariables();
    static void RestoreStuffFromMem();
    static void FinishPlayback();
    static void StoreClothesDesc(const CPedClothesDesc& desc, tReplayClothesBlock& packet);
    static void RecordThisFrame();
    static void RestoreClothesDesc(CPedClothesDesc& desc, tReplayClothesBlock& packet);
    static CPed* DealWithNewPedPacket(const tReplayPedHeaderBlock& pedPacket, bool loadModel, tReplayClothesBlock* clothesPacket);
    static bool PlayBackThisFrameInterpolation(CAddressInReplayBuffer& buffer, float interpolation, uint32* outTimer);
    static bool FastForwardToTime(uint32 start);
    static void PlayBackThisFrame();

    // Returns size of the specified packed id
    // NOTSA: Marked as constexpr because we're using this to validate tReplayBlockData block sizes.
    static constexpr uint32 FindSizeOfPacket(eReplayPacket type);
    static bool IsThisVehicleUsedInRecording(int32 a1);
    static bool IsThisPedUsedInRecording(int32 a1);
    static void FindFirstFocusCoordinate(CVector& outPos);
    static uint32 NumberFramesAvailableToPlay();
    static void StreamAllNecessaryCarsAndPeds();
    static CPlayerPed* CreatePlayerPed();
    static void TriggerPlayback(eReplayCamMode mode, CVector fixedCamPos, bool loadScene);

    // @notsa
    // @brief Returns all available buffers
    static auto GetAllActiveBuffers() { return Buffers | std::views::filter([](auto&& buffer) { return buffer.IsAvailable(); }); }
};

