#!/usr/bin/env python3
"""gen_x86_golden.py - independent generator of x86 (exe) save-game / stream record bytes for the P-2 host tests.   SKELETON (P-2a).

The golden vectors must NOT come from the code under test: each record below is a table of (field, x86 byte offset, struct format, count) taken from the
original layout (VALIDATE_OFFSET / VALIDATE_SIZE numbers of the x86 build, cross-checked with the i686-pc-windows-msvc layout dump by `selfcheck`),
and encoded with Python's `struct` (little endian, no implicit padding). Slices c..h add their records to RECORDS and their tests compare
`FileRecord<X>::ToFile(x)` byte-for-byte with `golden('X_File', **values)` (padding bytes zero).

  gen_x86_golden.py list                              names, sizes
  gen_x86_golden.py hex  CPickup_File rev=1.5 ...     print the vector as hex (field=value; arrays as a,b,c)
  gen_x86_golden.py cpp  [names...]                   emit a C++ header body: `constexpr uint8_t kGolden_<name>_<case>[N] = {...}` for the built-in cases
  gen_x86_golden.py selfcheck [--work DIR]            compare every record's offsets with the i686 layout dump of the in-memory struct (needs a layout_diff.py run)
"""
import argparse, glob, os, struct, sys

# record -> (total size, [(field, offset, fmt, count)], {in-memory member path -> field}) ; fmt is a struct char ('f' float, 'I' u32, 'H' u16, 'h' i16, 'B' u8, ...)
# The third element is only used by `selfcheck` (x86 in-memory struct == file layout there); {} when the file record has no x86 in-memory twin.
RECORDS = {
    # P-2e: save Pickups.cpp, CPickup is dumped raw; m_pObject (@4) is a pointer -> u32 in the record
    'CPickup_File': (0x20, [('rev', 0x00, 'f', 1), ('obj', 0x04, 'I', 1), ('ammo', 0x08, 'I', 1), ('regen', 0x0C, 'I', 1), ('pos', 0x10, 'h', 3),
                            ('moneyPerDay', 0x16, 'H', 1), ('model', 0x18, 'h', 1), ('ref', 0x1A, 'h', 1), ('type', 0x1C, 'B', 1), ('flags', 0x1D, 'B', 1)],
                     {'CPickup': {'m_fRevenueValue': 'rev', 'm_pObject': 'obj', 'm_nAmmo': 'ammo', 'm_nRegenerationTime': 'regen', 'm_vecPos': 'pos',
                                  'm_nMoneyPerDay': 'moneyPerDay', 'm_nModelIndex': 'model', 'm_nReferenceIndex': 'ref', 'm_nPickupType': 'type', 'm_nFlags': 'flags'}}),
    # P-2f: tRadarTrace 0x28 (Radar.cpp CRadar::Save/Load, array of 175): colour u32, handle, pos[3], counter u16 @0x14, radius @0x18, blipSize u16 @0x1C, enex pool index+1 (the union slot) @0x20,
    # sprite i8 @0x24, bit-field bytes flags0 @0x25 (bright b0, tracking b1, shortRange b2, friendly b3, remain b4, fade b5, coordAppearance b6-7), flags1 @0x26 (display b0-1, type b2-5, appearance b6-7)
    'tRadarTrace_File': (0x28, [('colour', 0x00, 'I', 1), ('handle', 0x04, 'I', 1), ('pos', 0x08, 'f', 3), ('counter', 0x14, 'H', 1), ('radius', 0x18, 'f', 1), ('blipSize', 0x1C, 'H', 1),
                                ('enex', 0x20, 'I', 1), ('sprite', 0x24, 'b', 1), ('flags0', 0x25, 'B', 1), ('flags1', 0x26, 'B', 1)],
                         {'tRadarTrace': {'m_nColour': 'colour', 'm_nEntityHandle': 'handle', 'm_vPosition': 'pos', 'm_nCounter': 'counter', 'm_fSphereRadius': 'radius', 'm_nBlipSize': 'blipSize',
                                          'm_nBlipSprite': 'sprite', 'm_bBright': 'flags0', 'm_bTrackingBlip': 'flags0', 'm_bShortRange': 'flags0', 'm_bFriendly': 'flags0', 'm_bBlipRemain': 'flags0',
                                          'm_bBlipFade': 'flags0', 'm_nCoordBlipAppearance': 'flags0', 'm_nBlipDisplayFlag': 'flags1', 'm_nBlipType': 'flags1', 'm_nAppearance': 'flags1'}}),
    # P-2i: BreakInfo_t 0x34 (DFF geometry extension chunk 0xFD, BreakablePlugin.cpp): 9 pointer slots (@8,0xC,0x10,0x18,0x1C,0x24,0x28,0x2C,0x30; all garbage in the file, re-pointed by the reader)
    'BreakInfo_File': (0x34, [('posRule', 0x00, 'I', 1), ('numVerts', 0x04, 'H', 1), ('pVertexPos', 0x08, 'I', 1), ('pTexCoords', 0x0C, 'I', 1), ('pVertexColors', 0x10, 'I', 1),
                              ('numTris', 0x14, 'H', 1), ('pTriangles', 0x18, 'I', 1), ('pTriMaterials', 0x1C, 'I', 1), ('numMats', 0x20, 'H', 1), ('pTextures', 0x24, 'I', 1),
                              ('pTextureNames', 0x28, 'I', 1), ('pMaskNames', 0x2C, 'I', 1), ('pMaterialProps', 0x30, 'I', 1)],
                       {'BreakInfo_t': {'m_uiPosRule': 'posRule', 'm_usNumVertices': 'numVerts', 'm_pVertexPos': 'pVertexPos', 'm_pTexCoors': 'pTexCoords', 'm_pVertexColors': 'pVertexColors',
                                        'm_usNumTriangles': 'numTris', 'm_pTriangles': 'pTriangles', 'm_pTrianglesMaterialIndices': 'pTriMaterials', 'm_usNumMaterials': 'numMats',
                                        'm_pTextures': 'pTextures', 'm_pTextureNames': 'pTextureNames', 'm_pMaskNames': 'pMaskNames', 'm_pMaterialProperties': 'pMaterialProps'}}),
    # P-2d: member of CPedSaveStructure[13]; m_FxSystem (@0x18) pointer -> u32 (never read back, the exe writes the live pointer)
    'CWeapon_File': (0x1C, [('type', 0x00, 'I', 1), ('state', 0x04, 'I', 1), ('ammoInClip', 0x08, 'I', 1), ('totalAmmo', 0x0C, 'I', 1),
                            ('timeForNextShot', 0x10, 'I', 1), ('firstPerson', 0x14, 'B', 1), ('dontPlaceInHand', 0x15, 'B', 1), ('fx', 0x18, 'I', 1)],
                     {'CWeapon': {'m_Type': 'type', 'm_State': 'state', 'm_AmmoInClip': 'ammoInClip', 'm_TotalAmmo': 'totalAmmo', 'm_TimeForNextShotMs': 'timeForNextShot',
                                  'm_IsFirstPersonWeaponModeSelected': 'firstPerson', 'm_DontPlaceInHand': 'dontPlaceInHand', 'm_FxSystem': 'fx'}}),
    # P-2d: CPedSaveStructure 0x18C (Ped.cpp CPed::Save/Load, behind a uint32 size prefix): pos, health, armor, CWeapon[13] @0x14 (13 * 0x1C), then the scalars; pads (0x183, 0x18A..B) are 0 here
    'CPedSaveStructure_File': (0x18C, [('pos', 0x00, 'f', 3), ('health', 0x0C, 'f', 1), ('armor', 0x10, 'f', 1), ('weapons', 0x14, 'B', 13 * 0x1C), ('createdBy', 0x180, 'B', 1),
                                       ('activeSlot', 0x181, 'b', 1), ('areaCode', 0x182, 'b', 1), ('exitIndex', 0x184, 'i', 1), ('fightingStyle', 0x188, 'B', 1), ('allowedMoves', 0x189, 'B', 1)],
                               {'CPedSaveStructure': {'m_pos': 'pos', 'm_health': 'health', 'm_armor': 'armor', 'm_weapons': 'weapons', 'm_createdBy': 'createdBy', 'm_activeWeaponSlot': 'activeSlot',
                                                      'm_areaCode': 'areaCode', 'm_nExitIndex': 'exitIndex', 'm_nFightingStyle': 'fightingStyle', 'm_nAllowedAttackMoves': 'allowedMoves'}}),
    # P-2c: CMatrix 0x48 : right/top/at (3 floats + u32 each), pos (3 floats + u32), RwMatrix* attach @0x40, owns-flag @0x44 (+3 pad). The u32 after each vector: flags @0xC (RwMatrix::flags), pad1..3.
    'CMatrix_File': (0x48, [('right', 0x00, 'f', 3), ('flags', 0x0C, 'I', 1), ('top', 0x10, 'f', 3), ('pad1', 0x1C, 'I', 1), ('at', 0x20, 'f', 3), ('pad2', 0x2C, 'I', 1),
                            ('pos', 0x30, 'f', 3), ('pad3', 0x3C, 'I', 1), ('attach', 0x40, 'I', 1), ('owns', 0x44, 'B', 1)],
                     {'CMatrix': {'m_right': 'right', 'flags': 'flags', 'm_forward': 'top', 'pad1': 'pad1', 'm_up': 'at', 'pad2': 'pad2', 'm_pos': 'pos', 'pad3': 'pad3',
                                  'm_pAttachMatrix': 'attach', 'm_bOwnsAttachedMatrix': 'owns'}}),
    # P-2c: CVehicleSaveStructure 0x68 (Vehicle.cpp Save/Load): CMatrix @0, then the scalars; field_48 and the pad bytes are never written by Construct (0 here)
    'CVehicleSaveStructure_File': (0x68, [('matrix', 0x00, 'B', 0x48), ('field_48', 0x48, 'B', 1), ('primary', 0x49, 'B', 1), ('secondary', 0x4A, 'B', 1), ('tertiary', 0x4B, 'B', 1),
                                          ('quaternary', 0x4C, 'B', 1), ('alarm', 0x4E, 'H', 1), ('maxPass', 0x50, 'B', 1), ('steer', 0x54, 'f', 1), ('gas', 0x58, 'f', 1),
                                          ('brake', 0x5C, 'f', 1), ('upper', 0x60, 'I', 1), ('lower', 0x64, 'I', 1)],
                                   {'CVehicleSaveStructure': {'m_matrix': 'matrix', 'field_48': 'field_48', 'm_nPrimaryColor': 'primary', 'm_nSecondaryColor': 'secondary',
                                                              'm_nTertiaryColor': 'tertiary', 'm_nQuaternaryColor': 'quaternary', 'm_nAlarmState': 'alarm', 'm_nMaxPassengers': 'maxPass',
                                                              'm_fSteerAngle': 'steer', 'm_fGasPedal': 'gas', 'm_fBrakePedal': 'brake', 'm_nVehicleUpperFlags': 'upper',
                                                              'm_nVehicleLowerFlags': 'lower'}}),
    # P-2c: tReplayCameraBlock 88 bytes (pack 1; = CReplay::FindSizeOfPacket(REPLAY_PACKET_GENERAL)): type, isUsingRemoteVehicle, pad[2], CMatrix @4 (0x48 bytes), firstFocusPosn @0x4C
    'tReplayCameraBlock': (88, [('type', 0x00, 'B', 1), ('usingRemote', 0x01, 'B', 1), ('matrix', 0x04, 'B', 0x48), ('focus', 0x4C, 'f', 3)],
                           {'tReplayCameraBlock': {'isUsingRemoteVehicle': 'usingRemote', 'matrix': 'matrix', 'firstFocusPosn': 'focus'}}),
}
MAT_ROT = dict(right=(0, 1, 0), flags=3, top=(-1, 0, 0), at=(0, 0, 1), pos=(-2500.5, 1000.25, 12.125), attach=0, owns=0)
# built-in cases (name, record, values) used by `cpp` and by the self-test of this generator
CASES = [
    # P-2e: flags 0x5D = bDisabled | bHelpMessageDisplayed | bVisible | nPropertyTextIndex=5 (<<4); live object present (obj=1), type ON_STREET
    ('basic', 'CPickup_File', dict(rev=1.5, obj=1, ammo=77, regen=1000, pos=(100, -200, 300), moneyPerDay=5, model=1212, ref=3, type=2, flags=0x5D)),
    ('noobj', 'CPickup_File', dict(rev=0.0, obj=0, ammo=5, regen=0, pos=(-32768, 32767, 0), moneyPerDay=65535, model=-1, ref=-2, type=3, flags=0x0A)),   # type ONCE, no object: bVisible stays as on file
    ('none_obj', 'CPickup_File', dict(rev=0.0, obj=1, ammo=0, regen=0, pos=(0, 0, 0), moneyPerDay=0, model=0, ref=0, type=0, flags=0x08)),   # PICKUP_NONE with a stale object: loader leaves it alone
    # P-2f: 'basic' = a mission blip bound to enex slot 6 (pool index+1 = 7): colour 8, sprite 27, flags0 0x0D (bright|shortRange|friendly), flags1 0x4A (display 2, type 2 -> bits 2-5 = 0x08, appearance 1 -> 0x40)
    ('basic', 'tRadarTrace_File', dict(colour=8, handle=0x00A1B2C3, pos=(-2500.5, 1000.25, 12.125), counter=300, radius=2.5, blipSize=3, enex=7, sprite=27, flags0=0x0D, flags1=0x4A)),
    # every bit-field set once (flags bytes only; the rest zero) : the test sets exactly this member in a zeroed trace and compares
    ('b_bright', 'tRadarTrace_File', dict(flags0=0x01)), ('b_tracking', 'tRadarTrace_File', dict(flags0=0x02)), ('b_short', 'tRadarTrace_File', dict(flags0=0x04)),
    ('b_friendly', 'tRadarTrace_File', dict(flags0=0x08)), ('b_remain', 'tRadarTrace_File', dict(flags0=0x10)), ('b_fade', 'tRadarTrace_File', dict(flags0=0x20)),
    ('b_coord1', 'tRadarTrace_File', dict(flags0=0x40)), ('b_coord2', 'tRadarTrace_File', dict(flags0=0x80)), ('b_coord3', 'tRadarTrace_File', dict(flags0=0xC0)),
    ('b_disp1', 'tRadarTrace_File', dict(flags1=0x01)), ('b_disp2', 'tRadarTrace_File', dict(flags1=0x02)), ('b_disp3', 'tRadarTrace_File', dict(flags1=0x03)),
    ('b_type1', 'tRadarTrace_File', dict(flags1=0x04)), ('b_type2', 'tRadarTrace_File', dict(flags1=0x08)), ('b_type4', 'tRadarTrace_File', dict(flags1=0x10)),
    ('b_type8', 'tRadarTrace_File', dict(flags1=0x20)), ('b_type15', 'tRadarTrace_File', dict(flags1=0x3C)),
    ('b_app1', 'tRadarTrace_File', dict(flags1=0x40)), ('b_app2', 'tRadarTrace_File', dict(flags1=0x80)), ('b_app3', 'tRadarTrace_File', dict(flags1=0xC0)),
    ('extreme', 'tRadarTrace_File', dict(colour=0xFFFFFFFF, handle=0xFFFFFFFF, pos=(3.4e38, -3.4e38, 0.0), counter=65535, radius=-0.5, blipSize=65535, enex=0xFFFFFFFF, sprite=-5, flags0=0xFF, flags1=0xFF)),
    # P-2i: 'exe' = what the exe wrote (pointer slots hold live addresses, here recognisable garbage); 'port' = what the port writes (pointer slots 0); counts 7 / 5 / 3, rule COLLISION_ORIGIN
    ('exe', 'BreakInfo_File', dict(posRule=1, numVerts=7, pVertexPos=0x0B1E0034, pTexCoords=0x0B1E0088, pVertexColors=0x0B1E00E8, numTris=5, pTriangles=0x0B1E0104, pTriMaterials=0x0B1E0122,
                                   numMats=3, pTextures=0x0B1E01B4, pTextureNames=0x0B1E0136, pMaskNames=0x0B1E0196, pMaterialProps=0x0B1E01F6)),
    ('port', 'BreakInfo_File', dict(posRule=1, numVerts=7, numTris=5, numMats=3)),
    ('max', 'BreakInfo_File', dict(posRule=0, numVerts=65535, numTris=65535, numMats=65535)),
    ('basic', 'CWeapon_File', dict(type=30, state=0, ammoInClip=30, totalAmmo=500, timeForNextShot=0, firstPerson=0, dontPlaceInHand=1, fx=0)),
    # P-2d: 13 slots, slot k = type 10+k (k=0 is WEAPON_UNARMED-like 0), ammo 100*k, inClip k; the pointer slot (fx) is 0 in ToFile output
    ('basic', 'CPedSaveStructure_File', dict(pos=(-2500.5, 1000.25, 12.125), health=100.0, armor=50.5, weapons=lambda: [b for k in range(13) for b in golden('CWeapon_File', type=0 if k == 0 else 10 + k, state=k % 5, ammoInClip=k, totalAmmo=100 * k, timeForNextShot=1000 + k, firstPerson=k & 1, dontPlaceInHand=1 if k == 12 else 0)],
                                             createdBy=2, activeSlot=5, areaCode=1, exitIndex=-1, fightingStyle=6, allowedMoves=0x1F)),
    ('identity', 'CMatrix_File', dict(right=(1, 0, 0), top=(0, 1, 0), at=(0, 0, 1), pos=(10.5, -3.25, 7), attach=0, owns=0)),
    ('rotated', 'CMatrix_File', MAT_ROT),
    # values given as a callable are resolved by golden() (embedding one record into another)
    ('basic', 'CVehicleSaveStructure_File', dict(matrix=lambda: list(golden('CMatrix_File', **MAT_ROT)), primary=1, secondary=126, tertiary=3, quaternary=255, alarm=0x1234, maxPass=3,
                                                 steer=0.5, gas=1.0, brake=-0.25, upper=0x80000001, lower=0x00402010)),
    ('basic', 'tReplayCameraBlock', dict(type=5, usingRemote=1, matrix=lambda: list(golden('CMatrix_File', **MAT_ROT)), focus=(-2500.5, 1000.25, 12.125))),
]


def check_table(name):
    size, fields, _ = RECORDS[name]
    end = 0
    for f, off, fmt, cnt in sorted(fields, key=lambda x: x[1]):
        assert off >= end, '%s.%s overlaps (offset 0x%X < end 0x%X)' % (name, f, off, end)
        end = off + struct.calcsize('<' + fmt) * cnt
    assert end <= size, '%s: fields end 0x%X > size 0x%X' % (name, end, size)


def golden(name, **vals):
    check_table(name)
    size, fields, _ = RECORDS[name]
    buf = bytearray(size)
    for f, off, fmt, cnt in fields:
        v = vals.get(f, 0)
        if callable(v):
            v = v()
        v = list(v) if isinstance(v, (list, tuple)) else [v]
        if len(v) == 1 and cnt > 1:
            v = v * cnt
        assert len(v) == cnt, '%s.%s needs %d values' % (name, f, cnt)
        struct.pack_into('<%d%s' % (cnt, fmt), buf, off, *v)
    return bytes(buf)


def cmd_selfcheck(a):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import layout_diff as ld
    files = glob.glob(os.path.join(a.work, '*_msvc_*.dump'))
    if not files:
        print('no msvc dumps in %s: run layout_diff.py run first' % a.work); return 1
    want = {t for _, _, tw in RECORDS.values() for t in tw}
    recs = {}
    for f in files:
        for n, r in ld.parse_dump(open(f, errors='replace').read(), lambda n: n in want).items():
            recs.setdefault(n, r)
    bad = 0
    for name, (size, fields, tw) in RECORDS.items():
        check_table(name)
        for twin, mapping in tw.items():
            r = recs.get(twin)
            if not r:
                print('%s: in-memory twin %s not found in dumps' % (name, twin)); bad += 1; continue
            offs = {f: off for f, off, _, _ in fields}
            for member, field in mapping.items():
                got = r['fields'].get(member)
                if got is None:
                    print('%s: member %s not in the %s dump' % (name, member, twin)); bad += 1
                    continue
                got_b = int(got.split(':')[0])
                if got_b != offs[field]:
                    print('%s.%s: golden 0x%X but x86 %s.%s is at 0x%X' % (name, field, offs[field], twin, member, got_b)); bad += 1
            if r['size'] != size:
                print('%s: golden size 0x%X vs x86 sizeof(%s)=0x%X' % (name, size, twin, r['size'])); bad += 1
    print('selfcheck: %s' % ('OK' if not bad else '%d mismatches' % bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest='cmd', required=True)
    sp.add_parser('list')
    h = sp.add_parser('hex'); h.add_argument('name'); h.add_argument('vals', nargs='*')
    c = sp.add_parser('cpp'); c.add_argument('names', nargs='*')
    s = sp.add_parser('selfcheck'); s.add_argument('--work', default='/private/tmp/layout_diff_work')
    a = ap.parse_args()
    if a.cmd == 'list':
        for n, (sz, f, _) in RECORDS.items():
            check_table(n); print('%-16s 0x%X  %d fields' % (n, sz, len(f)))
    elif a.cmd == 'hex':
        vals = {}
        for kv in a.vals:
            k, v = kv.split('=', 1)
            vs = [float(x) if '.' in x else int(x, 0) for x in v.split(',')]
            vals[k] = vs if len(vs) > 1 else vs[0]
        print(golden(a.name, **vals).hex())
    elif a.cmd == 'cpp':
        for case, rec, vals in CASES:
            if a.names and rec not in a.names:
                continue
            b = golden(rec, **vals)
            print('constexpr uint8_t kGolden_%s_%s[%d] = {%s};' % (rec, case, len(b), ', '.join('0x%02X' % x for x in b)))
    else:
        sys.exit(cmd_selfcheck(a))


if __name__ == '__main__':
    main()
