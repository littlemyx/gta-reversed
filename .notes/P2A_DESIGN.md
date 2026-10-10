# P2a: data image + hooks-as-fixups (prototype, 2026-10-09)

Implements D1/D1'/D2 of `PHASE2_PLAN.md`. Code: `tools/standalone/`, `source/standalone/{DataImage,Fixups}.{h,cpp}`, small hooks in
`Base.h`, `PluginBase.h`, `reversiblehooks/{RHManager.h,RHManager.cpp,VMTInfo.cpp}`, `WinMain.cpp` (standalone entry), `source/CMakeLists.txt`.
New define `NOTSA_STANDALONE_RUN` = `GTASA_STANDALONE=ON` and not `..._DUMP_HOOKS_ONLY` (dump mode is unchanged).

## 1. Extractor (`tools/standalone/extract_exe_data.py <exe> <out_dir>`)
* Parses the PE section table (nothing hardcoded). Code = sections with CNT_CODE/EXECUTE (`.text`, `_rwcseg`) -> code range
  [0x401000, 0x858000). Data image = all other sections except `.rsrc`: `.rdata .data _TEXT_HA _rwdseg` -> [0x858000, 0xCB0000), 4,554,752 B.
* `original_data.bin` (4,554,752 B) = initial bytes laid out by VA (gaps 0) **plus the `_initterm` delta (D1')**:
  `tools/standalone/initterm_delta.py` (Unicorn replay of the 1667 static initialisers, 18,250 ranges / 252,095 B changed, 19 `rand()`
  calls, 140 atexit entries ignored) is applied BEFORE the pointer scan. The delta writes into BSS, so the bin now covers the whole
  committed range (BSS = zeros + delta). Needs `pip install unicorn` in the python used by the build (`-DGTASA_PYTHON=<python>`);
  `--no-initterm` skips it. The delta is tied to 1.0 US compact (hardcoded CRT addresses inside the script).
* `original_data.json` {image_base, code_lo/hi, data_base/end, committed_size, bin_size, sections, stats}; `data_pointers.txt`
  "addr value class run" (V = member of a run >= 2 of code pointers = vtable/callback table, C = isolated, S = isolated text-like, ignored).
* Numbers (post-delta): 14,778 code-pointing dwords (.rdata 11,048, .data 3,507, _TEXT_HA 223; V 10,572, C 4,206; 11,326 distinct
  targets), 1,907 text-like singles ignored (ASCII "abc\0" / UTF-16 look like 0x4xxxxx), 22,549 dwords pointing into the data range
  (pre-delta 7,511). The delta adds only 13 new code-range dwords (all text-like).
* CMake: custom command at build time into `<build>/standalone_data/`, POST_BUILD copy of bin+json next to the exe
  (`<build>/bin`, so it never touches `bin/<config>` of the DLL build). Outputs are .gitignored.

## 2. Loader (`DataImage.cpp`)
* Reserves the code range PAGE_NOACCESS (best effort), reserves the 64K-granule-aligned data range, commits it RW at the ORIGINAL VA,
  reads the bin into it. Fails loudly (MessageBox + `standalone.log`, with VirtualQuery of the occupant) if the range is taken.
* Order guarantee: the loader is a `.CRT$XIB` C initializer. The CRT runs all C initializers (`_initterm_e(__xi_a,__xi_z)`)
  strictly before any C++ dynamic initializer (`.CRT$XC*`), so it precedes every static constructor / inline-variable initializer
  that could call `StaticRef`. The early path is CRT-free (Win32 + wsprintf only) because parts of the CRT (locks) are not up yet
  (a first attempt that used `_vsnprintf_s` there deadlocked on a CRT critical section). `StaticRef` asserts `g_DataImageLoaded`
  in debug builds; `WinMain` calls `DataImage::Load()` again (no-op safety net).
* `StaticRef<T>(addr)` in RUN mode is the plain dereference (no per-type zero buffer; that stays DUMP-only).

## 3. Hooks as fixups (`Fixups.cpp`)
* `RHManager::InstallStatic` (all `RH_Scoped*Install`, constructors, destructors) in RUN registers `(exeAddr -> ours)`; no code is patched,
  `NullHook` is kept for the UI/docs. `InstallVirtual`/`InstallVirtualDestructor` register `(exe vtable + 4*slot -> slot of OUR vtable)`:
  our vtable is found as before through the exported `??_7cls@@6B@` symbol (`GetModuleHandle(nullptr)` instead of the DLL; the exe
  must export vtables = `NOTSA_EXPORT_VTABLE` works in an exe too) and the slot index via `FindIndexOf(fnAddressGTA)` in the exe vtable
  (still unmodified at that time). The exe function is also registered as a fallback for inherited slots in other vtables.
* `ApplyToDataImage()` (after `InjectHooksMain()`): classify, then for every V/C dword: slot map -> function map -> trap stub.
  Unknown ones are written to `standalone_unknown_pointers.txt` (addr, value, class) next to the exe.
* Trap stub = `push exeAddr; call NotsaStandaloneTrapHandler` in an RWX page (16 B each, one per distinct address); the handler logs the
  exe address + the caller's return address (and `module+offset`, resolvable with the `/MAP` file) and terminates (exit code 3).
* `plugin::Call*<addr>` -> `Fixups::Resolve(addr)` (ours or trap stub). Open-coded `((T(*)(..))0xADDR)(..)` casts (949 sites) are caught by
  the NOACCESS code range + a vectored exception handler: known address -> `Eip = ours` (same calling convention assumed, args are already
  on the stack/ecx), unknown -> logs and terminates. Slow but only a net; real fix is replacing the casts.

## 4. The original VA range cannot be obtained with VirtualAlloc at startup (found by experiment) -> in-image placeholder
A test exe linked `/BASE:0x10000000 /FIXED /DYNAMICBASE:NO` fails the reservation under Wine 11.18 x86: a private heap (0x3D0000-0x420000)
and mapped sections (0x7C0000-0x8B8000) already sit in the original range before any user code. Real Windows puts heap/NLS there too. Only
the MAIN image is mapped before them. Solution (implemented, verified under Wine): link the exe at `/BASE:0x400000 /FIXED /DYNAMICBASE:NO
/MERGE:_TEXT=.text` and add `tools/standalone/orig_image_pad.asm` (assembled with `ml`, 0x8B0000 zero bytes, first object) which the linker
places at the start of `.text` (observed start 0x5746d0, so it covers 0x858000..0xCB0000). `DataImage` detects it (`_notsa_orig_pad..._end`
covers the data range), makes the pages RW, PAGE_NOACCESS for the placeholder pages inside the original code range, and copies the bin in.
The VirtualAlloc path stays as a fallback (fails loudly). Costs 9 MB in the exe. (link.exe orders output sections by its own rule - `.text`
first, custom-named sections such as `.orig` last - so `#pragma bss_seg`/`allocate` placeholders do NOT work; 5 variants tried.)

## 5. Status (build dir = clean worktree of HEAD, Wine, 2026-10-09)
Standalone exe (55 MB) compiles AND LINKS with zero unresolved symbols (the exe contains the whole game code; RW/CRT are still reached
by address). Run under Wine: image mapped, 8,575 functions + 1,466 vtable slots registered, 0 conflicts; of the 14,776 code pointers
(V 10,572 / C 4,204) 1,466 fixed by slot + 1,513 by function; 11,797 trapped (V 7,600, C 4,197; list in `standalone_unknown_pointers.txt`).
Then `NOTSA_WinMain` runs and the first trap fires: 0x72F4C0 (CMemoryMgr aligned alloc, D3 CRT redirect). `CEntityScanner` has a vtable
but no `NOTSA_EXPORT_VTABLE` (its virtual hooks are skipped; the DLL build would throw for it as well).
