#pragma once

#include "Matrix.h"
class CVehicle;

class CVehicleSaveStructure {
public:
    CMatrix m_matrix;
    uint8   field_48;
    uint8   m_nPrimaryColor;
    uint8   m_nSecondaryColor;
    uint8   m_nTertiaryColor;
    uint8   m_nQuaternaryColor;
    uint16  m_nAlarmState;
    uint8   m_nMaxPassengers;
    float   m_fSteerAngle;
    float   m_fGasPedal;
    float   m_fBrakePedal;
    uint32  m_nVehicleUpperFlags;
    uint32  m_nVehicleLowerFlags;

public:
    static void InjectHooks();

    void Construct(CVehicle* v);
    void Extract(CVehicle* v);
};
VALIDATE_SIZE(CVehicleSaveStructure, 0x68); // x86 in-memory (CMatrix grows on LP64); the on-disk 0x68 is VALIDATE_SIZE_FILE(CVehicleSaveStructure_File) in FileRecords/MatrixFile.hpp

// P-2c: file layout (0x68). The exe dumps the whole object raw (Vehicle.cpp Save/Load); under NOTSA_PORTABLE the typed CGenericGameStorage Save/Load go through this record
// (CMatrix::m_pAttachMatrix is a pointer: CVehicleSaveStructure is 0x70 on LP64). field_48 and the pad bytes are never initialised by Construct(); they are written as 0.
template<> struct notsa::FileRecord<CVehicleSaveStructure> {
    using Type = CVehicleSaveStructure_File;
    static Type ToFile(const CVehicleSaveStructure& s) {
        Type f{};
        f.matrix          = FileRecord<CMatrix>::ToFile(s.m_matrix);
        f.field_48        = 0;
        f.primaryColor    = s.m_nPrimaryColor;
        f.secondaryColor  = s.m_nSecondaryColor;
        f.tertiaryColor   = s.m_nTertiaryColor;
        f.quaternaryColor = s.m_nQuaternaryColor;
        f.alarmState      = s.m_nAlarmState;
        f.maxPassengers   = s.m_nMaxPassengers;
        f.steerAngle      = s.m_fSteerAngle;
        f.gasPedal        = s.m_fGasPedal;
        f.brakePedal      = s.m_fBrakePedal;
        f.upperFlags      = s.m_nVehicleUpperFlags;
        f.lowerFlags      = s.m_nVehicleLowerFlags;
        return f;
    }
    static void FromFile(CVehicleSaveStructure& s, const Type& f) {
        FileRecord<CMatrix>::FromFile(s.m_matrix, f.matrix);
        s.field_48              = f.field_48;
        s.m_nPrimaryColor       = f.primaryColor;
        s.m_nSecondaryColor     = f.secondaryColor;
        s.m_nTertiaryColor      = f.tertiaryColor;
        s.m_nQuaternaryColor    = f.quaternaryColor;
        s.m_nAlarmState         = f.alarmState;
        s.m_nMaxPassengers      = f.maxPassengers;
        s.m_fSteerAngle         = f.steerAngle;
        s.m_fGasPedal           = f.gasPedal;
        s.m_fBrakePedal         = f.brakePedal;
        s.m_nVehicleUpperFlags  = f.upperFlags;
        s.m_nVehicleLowerFlags  = f.lowerFlags;
    }
    // x86: the record IS the raw object (same size, same member offsets) - proves the MSVC raw copy == the record path
    static_assert(sizeof(void*) != 4 || (sizeof(CVehicleSaveStructure) == sizeof(Type)
        && offsetof(CVehicleSaveStructure, field_48) == offsetof(Type, field_48) && offsetof(CVehicleSaveStructure, m_nPrimaryColor) == offsetof(Type, primaryColor)
        && offsetof(CVehicleSaveStructure, m_nAlarmState) == offsetof(Type, alarmState) && offsetof(CVehicleSaveStructure, m_nMaxPassengers) == offsetof(Type, maxPassengers)
        && offsetof(CVehicleSaveStructure, m_fSteerAngle) == offsetof(Type, steerAngle) && offsetof(CVehicleSaveStructure, m_nVehicleUpperFlags) == offsetof(Type, upperFlags)
        && offsetof(CVehicleSaveStructure, m_nVehicleLowerFlags) == offsetof(Type, lowerFlags)));
};
