# D-A3 report: image poison + page census (slice A3 of DETACH_DATA_PLAN)

## What exists
| piece | where | notes |
|---|---|---|
| `NOTSA_POISON_CONVERTED=<file>` | `source/standalone/ImageDiag.cpp` (`DataImage::ArmDiagnostics`, declared in `DataImage.h`, one call in `WinMain.cpp` right after `NOTSA_VERIFY_GLOBALS_BOOT()`, i.e. after `ApplyToDataImage`) | fills `[addr, addr+size)` of every `0xADDR<ws>SIZE` line (`#` skipped; the `NOTSA_VERIFY_GLOBALS` boot dump is exactly this format) with 0xDD, `.rdata` made RW and restored. Only meaningful in a detached build (a converted global no longer lives in the image). Log line `NOTSA_POISON_CONVERTED: N ranges / M bytes ...` |
| `NOTSA_IMAGE_CENSUS=1` | same file | all 1,112 pages of 0x858000..0xCB0000 -> PAGE_NOACCESS (original protection remembered per page); vectored handler (first in chain, only claims AVs on armed pages): logs `page, addr, R/W, tick, tid, IP, 2 return-address candidates` (stack scan for return addresses preceded by a call opcode) to `NOTSA_IMAGE_CENSUS_FILE` (default `image_census.tsv` next to the exe), restores the protection, continues. `NOTSA_IMAGE_CENSUS_REARM_MS=<ms>` re-protects every page each period (one more first-touch per page per round; (addr, IP) pairs are de-duplicated), so one soak yields ~3,900 distinct touches instead of 1,112 |
| census tool | `tools/standalone/census.py` | `run`: one `soak.sh` run of `build/StandaloneDetached` (private snapshot of soak.sh; data dir `/tmp/d3c` + prefix `/tmp/d3c-prefix` deleted afterwards, raw files + soak report kept in `/tmp/d3c-out/census`) with VERIFY dump + census; `report`: classifies and symbolises (linker map) -> `.notes/DETACH_census.tsv` |
| output | `.notes/DETACH_census.tsv` | classes: STRAY_CONVERTED (touch inside an already converted global = old-address reader), UNCLAIMED (no DETACH_GLOBALS row, no converted global, no vtable run of data_pointers.bin), PENDING (inside a not yet converted row), VTABLE (counted only). One row per (class, owner or page, touching function, access) with first address, touches, first tick, callers |

## Acceptance
| item | result |
|---|---|
| build | `build/StandaloneDetached` (preset, DETACHED + VERIFY on, current working tree incl. the other agents' uncommitted edits): ImageDiag.cpp + WinMain.cpp compile, link 0 errors |
| poisoned run | `NOTSA_POISON_CONVERTED` = the boot dump of that build: **262 ranges / 5,376 bytes** (98 pilot globals + 164 ModelIndices.cpp globals of D-B11, uncommitted at that time) filled with 0xDD; `soak.sh build/StandaloneDetached 3` (min route = 412 s): RESULT OK, 21 route markers, clean atexit, 0 errors, 0 game [error]; `verify_globals.py` on a boot dump of the same poisoned run: 262 globals, 0 mismatches |
| census run (10 min) | `census.py run --minutes 10`: soak RESULT OK (22 markers, 19,300 frames, 0 errors, exit 0). 3,857 distinct touches, 896 of 1,112 pages touched; 992 report rows |
| census result | STRAY_CONVERTED 0 pages (no reader of the 262 converted addresses), VTABLE 4 pages, PENDING 877 pages (every unconverted global, expected), **UNCLAIMED 17 pages**: CAnimBlendAssocGroup::CreateAssociations / CAnimManager::CreateAnimAssocGroups (0x858B54, 0x85D374.., 0x8A8408..0x8A9F68, anim-association tables), CAESound::UpdateParameters (0x872A74, 0x85F438), CCutsceneObject::SetupCarPipeAtomicsForClump (0x868AFC), `_stricmp` from CShopping::LoadStats (0x85A4C8), `_strcpy_s` into 0x8ABBC8 (CAnimManager::AddAnimAssocDefinition), InteriorManager_c::Init/Update (0xBB3DC0, d-object not in the table), CDirectory::ReadDirFile (BSS arena 0xBBD000..0xBC0000) |

## Notes / caveats
* The census does not run in the oracle/address build for converted globals (nothing converted there); use the detached preset.
* A kernel-side access to a protected page (ReadFile/GetKeyboardState into an image buffer) returns an error instead of faulting; none seen in the soak (route completes, 0 errors).
* Coverage is limited by the route: 216 pages never touched in 10 minutes (not "unused", only not used by the route); more rounds / other routes add rows (`census.py report` merges nothing, it reads one raw file).
* The first 3-minute census attempt hung in `CCarCtrl::FindAngleToWeaveThroughTraffic` (+0x161..0x209, game-thread spin, sampler profile, ~5 faults per 15 s = not the handler): an infinite loop in the in-flight CarCtrl.cpp edit of another agent, not census related (the identical exe finished the poisoned and the 10-minute run). Worth a look by whoever owns CarCtrl.
* Another agent committed soak.sh while a 10-minute run was in progress and broke the running bash; `census.py run` therefore runs a private copy of the script.
* Build mutex: other agents hold it for long stretches; two of my builds failed on foreign WIP (ModelIndices / Physical.cpp / `.rsp` races) until it settled.
