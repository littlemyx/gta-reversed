# ASM-S75 report

Base 5f3ff606a (origin/reverse/interior_c). Result table: `.notes/asm_sweep/S75.result.tsv`. 37 rows: 17 MATCH, 3 FIXED, 17 NOT_REACHED (budget).

| row | verdict | commit |
|---|---|---|
| tHandlingData::InitFromData / cHandlingDataMgr::LoadHandlingData (0x5BD830-0x5BE47D) | FIXED | c06741525 |
| IKChainManager_c::CanAccept (0x618800-0x6188A4) | FIXED (NaN arms only) | ce73c4138 |
| CShotInfo::AddShot (0x739C30-0x739DDE) | FIXED (_ftol of destroy time) | 5da3d8c14 |
| CProjectileInfo::AddProjectile + the 5 small ProjectileInfo rows | MATCH | - |
| IKChainManager Update / CanAcceptLookAt / PointArm, CAutoPilot ModifySpeed / RemoveOnePathNode, CBulletInfo, CDamageManager::ProgressEngineDamage, ConvertBikeDataToWorldUnits, FindExactWord | MATCH | - |

## Notes

- Handling.cfg: the exe parses with strtok(" \t") + atof/atoi/sscanf("%x") and a jump table per line kind. All four tables (vehicle 36 tokens, bike 16, flying 22, boat 15) were decoded: store offsets and token order agree with the port's sscanf formats (vehicle: +0xc4 anti-dive, then token 28 -> +0xd4 seat offset, token 29 -> +0xc8 collision damage, 30 monetary, 31/32 hex model/handling flags with +0x78 copy, 33-35 lights/anim group). The real deviation: token 13 (engine acceleration) is `fld(atof) ; fmul qword 0.4 (0x86A950) ; fstp float`; the port scanned a float and multiplied by 0.4f, which is 1 ulp off for 6 of the 35 distinct accelerations in the shipped handling.cfg (1.2, 1.6, 13, 18, 21, 26). Now scanned as `%lf` and multiplied as a double (portable build: `notsa::fp::Round24Mul`). The `double` in the pre-commit grep is a genuine 53-bit atof operand.
- Left as is (invalid data only): FindExactWord returns entryCount (210) in the exe when the name is unknown, the port returns -1 and skips the line; a bike line with a car name indexes below the bike array in the exe.
- AddProjectile: every branch traced (jump tables 0x7387D4/0x7387F0 and 0x73881C/0x738830 decoded), including constants (0.22 + 0.15 speed, 0.4 z factor, molotov 0.2 floor, 0.5/0.03/0.75 elasticities, timers) and the object-info index 4 pointer (0xBB4BD0 = ms_aObjectInfo + 4*0x50). The exe also passes a 5th "CODEPRJ" argument to CRadar::SetEntityBlip that the port's 4-argument hook ignores (not in this slice).
- CShotInfo::AddShot: lifespan is 1000.0 for every weapon.dat entry, so the _ftol change is behaviour-neutral on shipped data.
- Not reached (budget): CBulletTraces (5), CSkidmarks (4), C_PcSave (4), CAccidentManager::ReportAccident, CNodeRoute new/delete, cTransmission::DisplayGearRatios.
