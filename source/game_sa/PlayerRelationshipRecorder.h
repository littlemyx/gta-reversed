#pragma once

class CPed;

struct PlayerRelationship {
    const CPed* Ped;
    uint8       Relationship; // asm: byte at +4

    PlayerRelationship() { Flush(); }

    void Flush() {
        Ped = nullptr;
        Relationship = 0;
    }
};

// useless class (unfinished)
class CPlayerRelationshipRecorder {
public:
    std::array<PlayerRelationship, 16> m_Relationships;

public:
    static void InjectHooks();

    CPlayerRelationshipRecorder();
    ~CPlayerRelationshipRecorder();

    void Flush();
    void ClearRelationshipWithPlayer(const CPed* ped);
    void AddRelationship(const CPed* ped, int32 value);
    uint8 GetRelationshipWithPlayer(const CPed* ped);
    void RecordRelationshipWithPlayer(const CPed* ped);

private: // NOTSA:
    friend void InjectHooksMain();

    CPlayerRelationshipRecorder* Constructor() {
        this->CPlayerRelationshipRecorder::CPlayerRelationshipRecorder();
        return this;
    }

    CPlayerRelationshipRecorder* Destructor() {
        this->CPlayerRelationshipRecorder::~CPlayerRelationshipRecorder();
        return this;
    }
};

CPlayerRelationshipRecorder& GetPlayerRelationshipRecorder();

VALIDATE_SIZE(CPlayerRelationshipRecorder, 0x80);
