# D-A1 report: gen_globals scanner + type probe (steps 1-3 of DETACH_DATA_PLAN 3.3)

Commit `8c9ef48a` (`tools/standalone/gen_globals.py`, only file committed; outputs live untracked in `.notes/DETACH_*`). Nothing in CMake calls it.
Reproduce (about 5 min on this Mac, takes the build mutex while `cl` runs):
`SCR=<scratchpad>; $SCR/capvenv/bin/python -I tools/standalone/gen_globals.py all --work <dir> --out .notes --lock $SCR/build.lock --jobs 4`
(needs: the exe at repo root, `build/StandaloneRelease/compile_commands.json` for the cl flags, unicorn for the post-`_initterm` image).
Stages: `scan` / `probe` / `merge` / `classify` / `all`; `scan` snapshots `source/` into `--work/snap` (other agents edit source concurrently; scan and probe must see
the same bytes, a hash is checked in `classify`).

## Outputs (`.notes/`)
| file | rows | content |
|---|---|---|
| `DETACH_GLOBALS.tsv` | 2,755 | one row per distinct address: section, `cat`, `cat_plan`, exact `size`/`align`, name, owner type, canonical type (compiler spelling), trait list, nz/delta bytes, pointer dwords (code/str/data/vtbl), owner file:line, other sites, scope, live-in-run flag, probe config, gap/tail columns, notes |
| `DETACH_objects.tsv` | 134 | category d: ctor VA (from INITTERM_AUDIT), source of the attribution, repo ctor (reversed name or trait), delta/nz bytes, vptr dwords, initterm ranges |
| `DETACH_duplicates.tsv` | 42 addresses / 52 extra sites | |
| `DETACH_conflicts.tsv` | 12 | class = size (7) / type (3) / spelling (2) |
| `DETACH_overlaps.tsv` | 43 | pairs whose exact extents intersect (contained / partial) |
| `DETACH_probe.json` | 696 canonical types | size, align, trait bits and the spellings that map to each |
| `DETACH_extra_sites.tsv` | 8 | sites NOT among the 2,755 rows: 2 ScopedStaticRef (var 0xB61364 / 0xC4032C; their flag words 0xB61368 / 0xC40330 are two more globals only visible in the 2nd argument), 1 decimal address, 5 base+offset (their base is a row) |
| `DETACH_raw_casts.tsv` | 873 | raw `(T*)0xADDR` casts; 22 are data (plan: 23), 851 are function-pointer casts (RW wrappers, `kind=code`) |

## How the probe works (step 2)
No hand-written type table. A COPY of `source/` is patched in `Base.h` (work dir only): `StaticRef` becomes a macro for `notsa_probe::Sr<__FILE__, __LINE__, __COUNTER__>::f`,
whose body takes the address of a class-template static member placed in a custom COFF section `probe$m` (record: file, line, sizeof, alignof, 14 trait bits, sizeof(elem),
`__FUNCSIG__` = canonical type). The real TUs are compiled with the run build's own cl flags (taken from compile_commands.json, /Od, /bigobj) under Wine against a PCH of the
patched StdInc: every `.cpp` that owns a site + one synthetic `#include "header"` TU per owner header (367 TUs, ~2 min with 4 jobs). The records are read from the .obj
(`read_probe_records`, regular and bigobj COFF). Because the compiler sees the call site in its real class / namespace / function / anonymous-namespace scope, local and
anonymous types, `constexpr` array dimensions, `NOTSA_EXPORT_VTABLE` classes etc. all resolve; result: 2,807/2,807 sites sized, est = 0.
Second pass (alt config = flags without NOTSA_RW_LIBRW / STANDALONE(_RUN) / SDL3, no fakerw) for the 40 sites in code the run build does not compile
(WindowedMode.cpp 27, `RenderWare/rw/{rwcore,rpmatfx}.h` 7, `RenderWare.h` 5 non-librw branch, `WinPs.cpp` 1 non-SDL branch): sizes come from the original RW/D3D headers.

## Acceptance
| item | plan section 0 | result | note |
|---|---|---|---|
| rows | 2,755 | 2,755 (2,807 sites, 1,798 in headers) | exact; hex-literal, non-Scoped sites. Deltas to the raw grep (2,812 `StaticRef<` matches): 2 `ScopedStaticRef` + 1 decimal literal (9252472 = 0x8D2078, TaskSimpleGunControl.h:28) + 2 matches inside Base.h; these 3 addresses are in `DETACH_extra_sites.tsv`, they would be rows 2,756-2,758 (A4 decides) |
| est sizes | 0 | 0 | |
| sections | .rdata 13 / .data 721 / BSS 2,021 | 13 / 721 / 2,021 | exact |
| duplicates | 52 | 52 extra sites on 42 addresses | exact |
| conflicts | 12 | 12 | |
| dead in run build | 34 | 34 (live_run=0) | exact (files WindowedMode.cpp + RenderWare/rw/*). 6 more rows are dead too but sit in preprocessor-inactive branches of live files (`RenderWare.h:33-37` non-librw, `WinPs.cpp:26` non-SDL3: RwInitialized, RwEngineInstance, geometryTKList, RpUVAnimDictSchema, AmbientSaturated, PsGlobal): `probe_cfg=alt`, `live_run=1`; effective dead set = 40 |
| objects.tsv | 134 | 134 | |
| overlaps | 93 candidates (plan: "many from enum=4 guess; real ~25") | 43 exact-extent intersections | |
| `cat_plan` a / b / c / d | 1,902 / 699 / 20 / 134 | 1,902 / 699 / 20 / 134, per-section split identical (a 1+13+1,888, b 12+687, c 20, d 133 BSS + 1 .data) | see "Categories" |
| `cat` (exact) a / b / c / d | | 1,895 / 715 / 11 / 134 | b +2.3 %, c -45 % vs plan, see below |

## Categories: two columns, why
`cat` is what the exact data says; `cat_plan` is the plan's census definition reproduced so section 0 can be checked 1:1.
* d (134, same in both): the plan's rule was "a constructor / initialiser of THIS object ran in `_initterm`", not "bytes changed". Reproduced exactly with: audit rows
  (INITTERM_AUDIT section 5, class b or n) whose `this=`/array start lies in the extent (includes 44 "n" objects whose ctor wrote nothing net, e.g. `CColModel`s, `CPedGroup`s),
  `_initterm` float stores into the extent (5), class-b fill loops that changed bytes in the extent (7), plus `AERadioTrackManager` (880 B class object written by
  CMenuManager's ctor, 2 B; the plan's single d row in .data). Seven small scalars written only by CMenuManager's ctor (`m_fMouseAccelHorzntl`,
  `m_bUseMouse3rdPerson`, `ProcessPed()::v`, 4 `s_unk_*`) are NOT objects: `cat` = b (value must be emitted, listed as `ctor_source=foreign-ctor-write` where they appear in objects.tsv),
  `cat_plan` = a (the exe FILE holds zeros there; that is how the plan counted them).
* d membership is by ctor attribution, so the plan's 33.5 KB "nz bytes of d" (est-capped extents) becomes 122,054 nz bytes with exact extents (= 95 % of the 127,968 B BSS delta).
* a / b / c: exact extents move 9 rows from c (plan) to b: 6 `tStatLine` (a 6-byte local struct that is really the first element of a table; `&StaticRef<tStatLine>` is returned),
  `gCrossHairRingGrowing` (std::array<bool,2> at the head of a table), and `SetAmbientColoursToIndicateRoadGroup()` (array of pointers to float constants, genuinely under-declared) plus `CClouds::RenderBottomFromHeight()`'s table (pointer-like hits there may be false). The plan's census scanned est-capped extents and saw string pointers
  after these heads. `cat_plan` reproduces the COUNT by HEURISTIC "tail promotion" (b row, class/array type, not .rdata, <= 1100 B gap to the next declared global, pointer-like dwords in the gap) -
  the 1,100 B threshold was tuned to get 9 rows, and the MEMBERSHIP differs from the plan's prose list in 2-4 rows (it picks `SetAmbientColoursToIndicateRoadGroup()` array
  and `CClouds::RenderBottomFromHeight()` offsets; it misses `gLightSurfProps` (RwSurfaceProperties) and `geometryTKList` (RwPluginRegistry), gaps 4.8 KB / 35 KB).
  Do not use `cat_plan` for work, use `cat` + the tail columns.
* Under-declared extents are a real finding, not an artefact: `tail_nz`/`tail_ptr`/`tail_delta` give non-zero bytes / pointer-like dwords / initterm bytes between the declared end and the next declared
  global (<= 8 KB). 362 rows have non-zero tail bytes (noisy: most tails are other, unreferenced globals); known genuine cases: `CAEAmbienceTrackManager` 0x8AC15C (declared 44 B, 4,874 non-zero bytes follow before the next declared global; plan: 5,396 B),
  the tStatLine / crosshair tables. A4 / the B batches must resolve those before emitting literals.

## HEURISTICS used (for the reviewer)
1. Scanner: comments/strings stripped; scope by brace stack (class / struct / union / namespace / enum / block); class header regex tolerates ALL_CAPS macro tokens between keyword
   and name (`class NOTSA_EXPORT_VTABLE CCamera`); function name = regex over the block header; a site is `decl` if the statement ends in `T& NAME =`, `return` if after `return`, else `expr`.
   Name qualification: scope chain + declarator qualifier; anonymous namespace dropped; function-local = `Func()::name` (`<fn>` for lambdas). Names of `expr`/`return` rows are `<File.cpp:line>`.
2. Several sites on one source line are paired with probe records by textual order = `__COUNTER__` order (one case: Group18.cpp:650).
3. Row size = max of the probe sizes of all its sites (7 `size` conflicts, listed); owner site = first decl (non-local, header before cpp, path, line).
4. Image: `extract_exe_data.py` output (post-`_initterm`, identical to the build's `original_data.bin`) vs `--no-initterm` (pre). `delta` = bytes that differ in the extent (exact, not the merged ranges).
5. Pointer dwords (`ptr_*`): code = accepted by the extractor's classifier (V/C in data_pointers.txt); vtbl = value is the start of a V/C run in .rdata; str = printable ASCII, NUL-terminated, >= 2 chars, in .rdata/.data;
   data = any other value inside 0x858000..0xCB1000 (so small ints that happen to fall into that range are counted: audio/colour tables can produce false data pointers; `c` is only assigned from the declared extent).
6. `cat`: d (see above), polymorphic with changed bytes -> d, class victim >= 64 B -> d, else a if all zero after `_initterm`, c if any pointer dword in the declared extent, else b.
7. objects.tsv ctor attribution order: audit row with object start inside the extent; float-init; ctor-less fill loop overlapping with real delta; foreign ctor net range. "repo ctor" = the audit's "repo has reversed ctor `X`", else trait
   (non-trivial default ctor / trivial). 120 category-a rows have a non-trivial repo default ctor on an all-zero exe image: when they become real globals their ctors must produce zeros (flag for A2/B batches).
8. Dead-in-run = file under `RenderWare/rw/` or `WindowedMode.cpp`; `probe_cfg=alt` rows were sized in the second configuration.

## Not done / limits
* Step 4 (literal emitter), codemod and verifier are other slices. `sizeof_probe.cpp` of the plan is replaced by the patched-Base.h approach above (a single generated probe TU cannot see local/anonymous types and class-scope names).
* The probe measures the types as the REPO declares them (run config). Where the repo type is smaller than the exe object (under-declared), the exe extent is only visible through the tail columns.
* Raw casts list is regex-based; the data-address casts (22) are not rows.
* `/Od` probe objects, PCH (550 MB each) and snapshots live in the work dir only; nothing is committed besides the script.
