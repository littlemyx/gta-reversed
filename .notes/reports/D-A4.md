# D-A4 report: alias / conflict / overlap / extent decisions + independent review of A1

Commit: `tools/standalone/gen_globals.py` only (review fix + `--check`). Outputs (untracked): `.notes/aliases.json`, `.notes/DETACH_ctor_nonzero.tsv`; `DETACH_GLOBALS.tsv` regenerated (1 row changed, see 1.3).
Check: `python3 -I tools/standalone/gen_globals.py --check` -> `0 unresolved` (2,711 live sites, 70 decided addresses). Mutation test: deleting 2 owners, 1 member, 1 extents entry, 1 tail verdict, 1 ctor verdict gives 9 UNRESOLVED lines, exit 1.

## aliases.json schema (for the codemod, agent D-A2)
(also the `_schema` key at the top of the file)
```
    aliases.json schema v1 (slice D-A4). Consumer: tools/standalone/codemod_globals.py (slice A2). Verifier: `python3 tools/standalone/gen_globals.py --check` (0 unresolved).
    Addresses are strings "0x%X". SITE record = {addr,file,line,kind,name,func,type}. A site is identified by (file, addr, kind, name, func), NOT by line (lines drift).
      kind: "decl" = `[static] [inline] auto& NAME = StaticRef<T>(A);` | "expr" = StaticRef<T>(A) inside an expression | "return" = `return StaticRef<T>(A);` in a function.
      name = qualified C++ name of the declared variable ("" for expr/return); func = enclosing function ("" at namespace/class scope).
    Every address that has exactly ONE site and is not mentioned below is plain: one site -> one global (type/size from DETACH_GLOBALS.tsv). Everything below is the exception list.
    globals[A] = {type, size, owner, aliases[], note}   (A = address of the storage; duplicates, conflicts)
      type, size: the single type (C++ spelling) and true byte size the storage gets. A site whose own `type` differs carries cast:true.
      owner: either an existing SITE (+ optional link/init_in_source/retype/grow) that becomes the real global, or {synthetic, home, scope}: no suitable site, create the variable
             `synthetic` in `home`: scope "tu" = file-local (anonymous namespace) in that .cpp; scope "shared" = `inline T notsa::shared::NAME` in the file named by the top-level key
             shared_header (generated, included by every alias file). owner.link="extern": owner keeps its .cpp, needs external linkage + extern declaration in the alias files.
             owner.init_in_source: the owner statement carries a runtime initialiser that must be kept (image holds zeros).
      aliases[]: every other site of A. decl -> `auto& NAME = <Owner>;` (cast:true -> `auto& NAME = reinterpret_cast<TYPE&>(<Owner>);`; same static/inline/qualification as before);
             expr/return -> replace the text `StaticRef<T>(A)` by <Owner> (or by `replace` when present). access:"private" -> the owner is a private class static: needs a friend/accessor.
             unused:true -> nothing uses it, delete the declaration.
    members[] = {addr, owner, offset, type, sites[], note}: the site's storage lies INSIDE the global `owner` (address of another entry) at byte `offset`; emit
             `auto& NAME = *reinterpret_cast<TYPE*>(reinterpret_cast<uint8*>(&<Owner>) + offset);`. Never a global of its own (no definition, no verifier extent).
    views[] = {addr, site, type, verify_addr, verify_offset, verify_size}: declaration whose base address is NOT the start of the real object (repo-shifted index). Keep its own zeroed
             storage of `type`; the verifier compares elements/bytes [verify_offset, verify_offset+verify_size) with the image at verify_addr.
    virtual[] = {addr, sites[], real_array, real_name, index_bias}: not a storage address (base of an indexed access); rewrite the accessor to index `real_array`; no global is created.
    retype[] = {addr, file, line, frm, to, why}: the repo declaration has the wrong width/shape against the exe asm; fix the declaration BEFORE converting (do not guess in the codemod).
    readdress[] = {site, frm, to, type, size, why}: the site is declared at the wrong address; change the literal to `to` (a new global, see new_globals).
    new_globals[] = {addr, type, size, cat, value?, file, line, why}: addresses that are not rows of DETACH_GLOBALS.tsv (cat a = zero, b = constant `value`).
    extents{A} = {kind: shrink|grow|extend|keep, declared, true_size, element_type?, why, source}: byte size to use (verifier extent / array bound) instead of the size of the declared type.
    width_notes[] = informational: scalar rows whose exe accesses are wider/narrower than the declared type.
    tail_audit{A} = "SEPARATE" | "EXTEND" | "UNKNOWN" for every row with non-zero bytes between its extent and the next declared global (columns tail_nz in DETACH_GLOBALS.tsv); EXTEND rows have an `extents` entry.
    ctor_audit{A} = "ZERO" | "NONZERO" | "VPTR" for the 120 all-zero rows whose repo type has a non-trivial default ctor (detail: .notes/DETACH_ctor_nonzero.tsv). NONZERO = the ctor writes non-zero bytes (e.g. -1 fills): the global must be zero-initialised (constinit/aggregate zero) instead.
```
Counts: globals 40 (owner site 26, synthetic 14 of which shared 10), alias sites 61, members 30, views 1, virtual 1, retype 20, readdress 1, new_globals 5, extents 22, tail_audit 363 ({'SEPARATE': 349, 'EXTEND': 14}), ctor_audit 120 ({'VPTR': 1, 'NONZERO': 39, 'ZERO': 80}), width_notes 38.

## 1. Review of A1 (independent)
Independent re-derivation from the exe file (own PE parser, no extractor code) and the live source:
| check | scope | result |
|---|---|---|
| address literal + declared name found at the owner file:line | all 2,755 rows | 0 failures |
| row `type` == the `StaticRef<...>` argument at the owner line | all 2,755 | 2,754 equal; 1 differs only by a comment inside the type text (0xC0BC78) |
| section label (.rdata/.data/.bss) vs my PE section table | all | 0 mismatches |
| `nz_bytes`, `delta_bytes` recomputed from the post/pre image; pre image == exe file bytes | all rows + all image sections | 0 mismatches |
| size: compiler `size` vs my table (primitives, enum underlying types, arrays, std::array, VALIDATE_SIZE) | 2,602 of 2,755 | 2,599 equal; 3 differ and are exactly conflicts (0xB61D4C scanner State enum (my enum-name lookup), 0xC278F8, 0xC8CF68); 153 rows (opaque structs/classes without VALIDATE_SIZE) not independently derivable, eyeballed 40 (CRGBA 4, CVector2D 8, RGBAf 16, D3DMATRIX 64, D3DCAPS9 304, D3DPRESENT_PARAMETERS 56, Im3DVertex 36 x N ...) |
| manual sample (stratified, 65 rows: 18 a, 22 b, all 11 c, 14 d): address, type, size, first 16 exe bytes, category | 65 | all consistent |
| all 12 conflicts | 12 | all re-decided from asm (section 2.2); 5 of them turned out to be repo bugs, not spelling issues |
| d rule (own ctor ran in `_initterm`) vs INITTERM_AUDIT | whole image delta | all 128,021 changed bytes: 122,041 inside d rows, 15 inside the 7 documented initterm scalars (cat b), 5,965 in 211 ranges that no row covers (unreferenced-by-address objects, Phase D/E). Every audit b/n row with `this=` maps to a d row (2 exceptions are objects no `StaticRef` names: 0x977BE8, 0x3189 junk). No d row lacks an audit/fill/float cause; 44 d rows have delta 0 (audit class n). The rule is sound. |
| c rule | 11 rows | 1 BUG (fixed, commit): `gBeatTrackLookup` (std::array<short,14>, 0x8AE538) was counted as 7 data pointers because the 4-byte-aligned dword scan runs over 16-bit elements. `ptr_scan` now skips (arrays of) char/short/bool. After the fix: cat c = 10 (not 11), b = 716, `cat_plan` c = 19 (the plan's 20 contained this false hit). Everything else in c is real: 65+26 code pointers, 11 + 12 + 6 string pointers, 236 + 12 + 7 pointers to unreferenced data tables (AnimAssocDefinition AnimNames/AnimDescr, ms_vehicleDescs, waterCreatureInfos: their TARGETS are not rows, the B batch that emits these literals must emit the targets too) |
| cat_plan 1,100 B tail-promotion threshold | 9 rows | arbitrary but harmless (only feeds `cat_plan`, which A1 already says not to use). Not changed. |
Not fixed (documented limits): `ptr_data` counts any dword in 0x858000..0xCB1000 (false positives possible for int tables; no such c row remains); the `small_elem` rule does not look inside structs of small fields.

## 2. A4 decisions
### 2.1 Duplicates (42 addresses / 52 extra sites)
Rule: (R1) header-declared class static / inline namespace variable present -> it is the owner (22 addresses); (R2) all sites in one TU -> first decl (4 .cpp owners), or a file-local synthetic for expr-only sites (4); (R3) several .cpp files, no header -> synthetic `notsa::shared::NAME` in a new generated header (`DetachedShared.h`) and EVERY site (also the former owner) becomes an alias (10). `owner.link="extern"` only for SIN_LUT (owner statement holds the runtime initialiser lambda: the image is zero). Aliases of a different integer type (`int32` vs `uint32`, `int32&` vs `eWantedLevel`) carry `cast:true` (reinterpret_cast, signed/unsigned of one type is legal aliasing; MSVC does not use TBAA). Private class statics accessed from Group18/19 (CShopping x3) and the Furniture globals are flagged `access:"private"` (friend/accessor needed in the B slice).
Two addresses are not duplicates but special: 0xB78E20 (`virtual`, see 2.3) and 0xBA18D9 (member of OnscnTimer).
### 2.2 The 12 conflicts
| address | repo says | decision (evidence) |
|---|---|---|
| 0x8A5B24 | int32 / uint32 | uint32 (header owner), int32 alias via cast |
| 0x8CDEE4 | eWantedLevel / int32 | eWantedLevel, int32& return via cast |
| 0x8D6220 | RwInt32 / int32 | same type, one header |
| 0xA9A318 | array<uint32,300> / uint32 | array (1200 B ends exactly at the next row 0xA9A7C8); the uint32 site is `(&X)[index]`, replace by element 0 |
| **0xB6B98C** | CPed* / tVehicleAudioSettings* | **repo bug**: 0x4F5B20 stores the driver (`[eax+0x460]`) at 0xB6B98C and `this+0x80` (&m_AuSettings) at **0xB6B99C**. The repo put both names on 0xB6B98C, so `s_pVehicleAudioSettingsForRadio = &m_AuSettings` overwrote the driver pointer. `readdress` -> 0xB6B99C (new zero global). |
| **0xB6EC5C** | CEntity* / array<bool,29> | gArrestCamOneCop (CEntity*) is the real global; `gbExitCam` is a repo-shifted base: the exe uses 9 bytes at 0xB6EC70 (cam ids 20..28), elements [0,20) overlay five unrelated globals and are unused. Kept as `view` (own zeroed std::array<bool,29>, verifier compares [20,29) with 0xB6EC70). Resolves 5 overlap pairs. |
| **0xBAB378** | uint16 / uint32 | uint16: all 12 exe accesses are 16 bit. Same for 0xBAB37C (declared uint32 only, 8 accesses, 16 bit). `retype`. |
| 0xBB3E00 | SIN_LUT_SIZE spelling | same type (256 floats); owner Maths.cpp (extern + initialiser) |
| **0xC17824** | CColModel / array<CColModel,2> | one CColModel (audit #1154, ctor 0x40FB60); the second half is the CCollisionData row 0xC17854 (ctor 0x40F030). `retype` col1 -> CColModel; Game.cpp:271-272 `col1[1]` must use s_FightColData. |
| 0xC278F8 | Im3DVertex* / Im3DVertex[4] | the 4-vertex array (144 B) is the object; 3dMarkers.cpp's pointer declaration is unused (delete). |
| **0xC4D958** | TempVertexBuffer(102400) / array<vertex,4> | **one scratch arena 0xC4D958..0xC6A158 = 116,736 B**: TempBufferVertices.m_3d (2048 x 36 B) + m_2d, with ShatteredPolyVertexBuffer (+0x9000), ReflectionPolyVertexBuffer (+0xD800, ends beyond TempVertexBuffer's declared end) and aRadiosityVertexBuffer (+0x12000 = m_2d) deliberately overlaid (Glass indexes TempBufferVertices.m_3d[ShatteredVerticesBaseIdx+i]). Owner = arena (`grow`), the rest are members; Fx.cpp's array is a prefix view. Must be ONE object (union) in the detached build or the Glass code loses its aliasing. |
| 0xC8CF68 | WinVer(4) / OSStatus(32) | OSStatus (32 B, next row at +0x20), WinVer = first member |
### 2.3 Overlaps (43 pairs)
After the decisions above all 43 pairs are gone: 30 `members` incl. 3 own findings (inner storage inside a larger global: cop model arrays, bike cheat, CCamera/CIdleCam/CFireManager/COnscreenTimer/FurnitureManager/InteriorManager fields incl. 8 InteriorManager statics, colPoints[0].m_vecNormal = `fxDirection`, CMeleeInfo[12] overlay of the pistol-whip tables (the exe's LoadMeleeData loop end 0xC177BC proves 13 elements), arena members, OSStatus::OSVer, 2 raw views into s_Conversation, WaterLayerAlpha[2], surface table field); `retype` list of 20 declaration errors (VehicleGunOffset 14 -> 13 elements, GameLogic GameState/ActivePlayers are bytes in the exe (repo int32 ActivePlayers swallows both flag bytes: bug), col1, arena); 1 `virtual` (0xB78E20 = &StatTypesInt[0] - 120*4, the base of `[stat*4+0xB78E20]`; lies inside TimesMissionAttempted, must not become a global; IntStatCell(stat) -> StatTypesInt[stat-120]); 1 view (gbExitCam).
### 2.4 Under-declared extents (tail_nz > 0: 363 rows incl. the pilot-converted)
Method: capstone sweep of .text (1.53 M instructions, 98 k references into the data image, with R/W/width), plus every hex literal in the source (comments included) = "claims". A tail is SEPARATE when the first claim comes immediately or the unclaimed prefix is zero (306 rows); the 56 rows with non-zero bytes before the first claim were triaged by asm (helper agent, loop bounds, sentinels, indexed access ranges) -> **13 EXTEND, 43 SEPARATE**:
- 8 `tStatLine` tables (Stats.cpp:870-877): `&StaticRef<tStatLine>` is the head of a -99-terminated table: 25/20/6/25/14/50/87/13 entries = 150/120/36/150/84/300/522/78 B (terminators verified in the image).
- 0x8AD914: field of a **195-entry 16-byte table at 0x8AD908** (3,120 B, ids from `id<<4`; ends at 0x8AE538); new global, 0x8AD914 becomes a member (repo comment 'table at 0x8AD910, percent at +4' is wrong).
- gFollowPedCamTuning 2 -> 3 entries (180 B), ms_WakePointAlphaMults 4 -> 5 floats, gCameraMode 2 -> 4 B (exe uses int32, value -1), m_grainStrength array<char,2> -> int32, WaterLayerAlpha 2 -> 3 uint32 (own finding: 0x8D3810 is [2], read at 0x6E8BE1).
- **CAEAmbienceTrackManager 0x8AC15C stays 44 B**: all members referenced by the exe end at +0x28; the 4,874 non-zero bytes behind it are other tables (gSoundDistAttenuationTable @0x8AC270 ported as constexpr, CAEAudioEnvironment constants, reverb tables), not part of the object. The plan's 5,396 B was an est-capped type extent.
Limits: claims are generous and the exe never indexes a 'never used' element variably (WaterLayerAlpha[2] was found only because the helper noticed the neighbour); 289 rows have an exe-referenced undeclared address exactly at their end - all scalar-after-scalar or separate neighbour tables in the 40 I looked at, not individually audited. 66 `width_notes` rows (scalars whose exe access is wider/narrower than the declared type; e.g. RwBool declared `bool`, CTheZones::m_CurrLevel enum eLevelName 1 B accessed as dword): informational, harmless in the detached build (separate storage).
### 2.5 The 120 zero rows with non-trivial repo default ctors (`.notes/DETACH_ctor_nonzero.tsv`, helper agent from source, I re-checked CStreamingInfo, CLoadedCarGroup, CZoneInfo)
ZERO 80, **NONZERO 39**, VPTR 1. NONZERO (must be zero-initialised / constinit zero when they become globals; the exe image is zero because the exe constructs them later at runtime): CLoadedCarGroup (Clear() -> -1 fill, 5 rows), CStreamingInfo[26316] (three int16 = -1), tStreamingFileDesc[8], CNodeAddress x4 + CPathFind (-1), tScript{Sequence,Checkpoint,EffectSystem,Sphere} (id = 1), tScriptConnectLodsObject, tScriptAttachedAnimGroup, tBuildingSwap, tScriptBrainWaitEntity, CStreamedScripts, CUpsideDownCarCheck, CScriptResourceManager (also `= delete` default ctor), tPhraseMemory[150], tScannerSlot[5] x2, tRadioIndexHistory x4, tDummyEngineSlot, CZoneInfo[380] (PopType 5, PopRaces 0xF), g_RadarGangOverlay CRect (+-1e6), CPool<COctTree>, CustomEnvMapPipeMaterialData, CPlantSurfProp[57], CCheckpoint[32], CExplosion[16], CShotInfo[100]. VPTR: gRwStream (abstract rw::Stream: needs a byte wrapper). ZERO but not declarable as plain globals: cHandlingDataMgr (ctor declared, never defined), CReplay::CVector_Reversed x3 and CWaterPolygon<3>/<4> (no default ctor): add `= default`. Destructors that run at exit (CPtrList, CPool Flush, SArray, EntityRef, CMatrix) are not construction side effects. Remaining 80 ZERO verdicts are from a shallow read for plain-POD arrays (members grepped, not read line by line).

## 3. Findings the B slices must act on (source edits that are NOT in the codemod)
1. Repo bugs found: s_pVehicleAudioSettingsForRadio at the wrong address (0xB6B98C, should be 0xB6B99C); CGameLogic::ActivePlayers int32 over a byte (clobbers two flags); col1[1] typed CColModel but is CCollisionData; FurnitureManager ids uint32 over uint16; SetAmbientColoursToIndicateRoadGroup uses `idx & size` where the exe does `idx % 7` (unused function). `retype`/`readdress` lists in aliases.json have file:line.
2. 127 of the 133 namespace-scope header declarations are `static [inline] auto& X = StaticRef` (124 `static inline`, 3 `static`): internal linkage = one copy per TU. The codemod must emit plain `inline` (no `static`) for them or every TU gets its own variable.
3. `ScopedStaticRef` sites (AEGlobalWeaponAudioEntity.cpp:124, PostEffects.cpp:1370) are lazy-init guards: become `static T v = init;`; their flag words 0xB61368 / 0xC40330 are not needed. They and 0x8D2E78 (decimal literal, value 0x7D0) are in `new_globals`.
4. Pilot files are already converted by A2 (Cheat.h, Hud.h, Timer.h): the scanner no longer sees them (`stale` is only a warning); TSV rows keep covering them.
5. Maths.cpp `SIN_LUT = []{...}()` fills a zero image row at static-init time: keep the initialiser.
