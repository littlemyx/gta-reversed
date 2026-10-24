# MAC-P2-DESIGN report (read-only design of slice P-2)
Plan: `.notes/P2_PLAN.md` (per-struct table, 11 slices P-2a..P-2k, order, tests). Branch portable/main fbc7c5cc, no source touched.
* Pattern: dedicated `X_File` POD record + `notsa::FileRecord<X>` trait used by the typed Save/Load templates only under NOTSA_PORTABLE (MSVC raw path and code bytes unchanged; both-build static_asserts prove x86 equivalence). Exceptions: tScriptParam (drop unused pointer members), ExtraVertColour (not a file struct, only its writer), HAnimKeyFrame (stream stride 0x24 vs sizeof 40).
* CSimpleVariablesSaveStructure mystery: `<minwinbase.h>` (SYSTEMTIME) missing on the Mac, class body dropped; with a 16-byte SYSTEMTIME arm64 sizeof == 0x138 (probed).
* size_t prefixes: 5 sites (+ 4 hidden 8-byte `GetIndex()+1` writes in TheScripts.cpp:1299/1323/1327/1331, found by reading, invisible to the census); CPed/CObject::Load asserts compare against the in-memory sizeof.
* HeapBlockDesc: CMemoryHeap is dead (MEMORY_MGR_USE_MEMORY_HEAP defined nowhere); keep assert compiled out + guard.
* CRunningScript: next/prev/IP/IPStack are overwritten after load, BaseIP is only read by a debug log; so zeros (or VA+offset) in the file record are enough.
* No save game or replay.rep exists in the Steam copy (Aspyr .app bundle; data root recorded in plan s.1): golden x86 vectors from an independent Python generator + clang i686-msvc record layouts; real data used for NODES*.DAT, gta3.img DFF 0xFD/0xF9 chunks, main.scm; a real .b is requested from the Windows host (not blocking).
* New gate `layout_diff` (i686-msvc vs arm64 record layouts for all 131 FILE structs) because equal sizeof does not prove equal bit-field layout; may add rows.
* Windows hot files: the 9 latest fork/reverse/interior_c commits touch none of the P-2 files; watch Scripts/**, Radar.cpp, PathFind.cpp, standalone/rw/hanim.cpp.
* Estimate ~770 agent-min total; waves: a,i,j | b,c,d,e,f | g,h | k.
