# FID-MAC-2 (2026-10-27) - Win32 fidelity items from MAC-STAB1..4 / MAC-FPTRAP

Branch fid-mac-2 on origin/reverse/interior_c 81636be3. asm = gta_sa_compact.exe (tools/standalone/exedis.py).

| # | Item | Exe | Verdict | Commit |
|---|------|-----|---------|--------|
| 1 | ClearSpaceForMissionEntity 0x486B00 | pre-pass 0x486B6C..0x486B90 nulls peds with pedFlags&0x100; vehicle branch ends at 0x486D4E | port bug, fixed | 6232b0da |
| 2 | PlayGunSounds t | 0x503FE9..0x503FFC fsubr/fsub/fdivrp | port bug, fixed | 1bb8daee |
| 3 | CreateConvoy / CreatePoliceChase matrix copy | CMatrix::operator= 0x59BBC0 | port bug, fixed | 841378ff |
| 4 | CVehiclePool slot = CHeli | VALIDATE_SIZE: CHeli 0xA18 >= every vehicle (CPlane 0xA04) | fine on x86, no change | - |
| 5 | FindCoverPointsBehindBox `(i-2)%4` | 0x69A1C8 `and eax,3` | port bug, fixed (`& 3`) | 3d53912d |
| 6a | MatrixLinkList::Init m_pOwner = nullptr | exe leaves it uninit | applied unguarded, not exe-visible | 4114c717 |
| 6b | DeleteRwObject CopyParentMatrix | exe reads freed RwFrame matrix (latent UAF) | applied unguarded, gameplay-neutral | e57528f4 |
| 6c | CreateHitColModelSkinned bone-local centre | 0x4C6D90 uses unanimated hierarchy matrices | exe-too, not applied | - |

Gate: Release 1185 objs 0 errors; oracle targets game/logic/physics/wave3_{ai,audio,coll,entity,ped,scripts,vehicle} all ok (0 failures, 0 mismatches);
wave2_oracle_test NOBUILD from an unrelated test-source error (wave2_oracle_gen_func.inc:852, eBoneTag32 vs eBoneTag16); soak 3 min NOVSYNC RESULT OK.
Not run: full run_all_tests, gameplay_regress, PortableWin64 (no gate step yet). Consumer audit: no reader/writer of changed layout (items are local logic fixes).
