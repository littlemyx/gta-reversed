#pragma once
/*
 * MAC-BOOT1 / W-7: portable path layer. The game builds Windows paths ("MODELS\\COLL\\peds.col", "data\\Paths\\NODES0.DAT") that are
 * relative to the process cwd and case-insensitive. On a case-sensitive POSIX file system they are resolved here:
 *  - '\\' becomes '/',
 *  - every component is matched case-insensitively against the real directory (exact match first; directory listings are cached,
 *    a cached listing is re-read when the directory's mtime changed or a lookup misses and the directory mtime moved on),
 *  - trailing spaces and dots of a component are dropped (Win32 path normalisation; the game's data files list "DATA\\DEFAULT.IDE " with a trailing blank),
 *  - "", "." and ".." components are kept as they are (".." goes to the real parent, as the OS does),
 *  - a component that does not exist stays verbatim (so "wb"/"a" modes create new files with the game's own spelling), and so does everything after it.
 * Relative paths stay relative to the cwd (the exe's CFileMgr::ChangeDir/SetDir chdir); absolute paths are resolved from "/".
 * Implementation: path_resolve.cpp (std only: used by the host test tests/portable/path_resolve_test.cpp as is).
 */
#ifdef NOTSA_PORTABLE
#include <cstdio>
#include <string>
#include <string_view>

namespace notsa::portable {
//! Resolves `path` against the real file system (relative to `baseDir`, "" = the cwd; baseDir itself is used verbatim).
std::string ResolvePathFrom(std::string_view baseDir, std::string_view path);
//! ResolvePathFrom("", path)
inline std::string ResolvePath(std::string_view path) { return ResolvePathFrom({}, path); }
//! std::fopen(ResolvePath(path), mode)
std::FILE* OpenResolved(const char* path, const char* mode);
//! CFileMgr::OpenFile semantics of the exe (MSVC CRT): a read-only mode without 'b' / '+' ("r", "rt") is TEXT mode, i.e. CRLF reads as LF and Ctrl-Z (0x1A) ends the
//! file; every other mode is as std::fopen. Text-mode reads go through an in-memory copy of the (translated) file (funopen), so fgets/fscanf/fseek/ftell/feof work as usual.
std::FILE* OpenResolvedText(const char* path, const char* mode);
//! C-callable (void* file) fopen replacement for librw's engine->filefuncs.rwfopen
void* RwFopenResolved(const char* path, const char* mode);
//! chdir(ResolvePath(path))
int ChdirResolved(const char* path);
//! true if ResolvePath(path) names an existing file or directory
bool ExistsResolved(const char* path);
//! Drops all cached directory listings (tests)
void ClearPathCache();
}
#endif
