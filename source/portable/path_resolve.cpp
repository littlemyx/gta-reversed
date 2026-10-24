// MAC-BOOT1 / W-7: see path_resolve.h. Standalone on purpose (no StdInc): the host test compiles this file directly.
#ifdef NOTSA_PORTABLE
#include "path_resolve.h"

#include <cerrno>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace notsa::portable {
namespace {
struct DirListing {
    bool        valid{};
    timespec    mtime{};
    std::unordered_multimap<std::string, std::string> byLower; // lower-case name -> real name
};

std::mutex                                  s_Mutex;
std::unordered_map<std::string, DirListing> s_Dirs; // key: the directory path as built here (the cwd-relative or absolute text)

std::string Lower(std::string_view s) {
    std::string r{s};
    for (auto& c : r) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        // non-ASCII bytes (UTF-8 sequences) are compared as they are: HFS+/APFS fold full Unicode, the game's file names are ASCII
    }
    return r;
}

bool StatMtime(const std::string& dir, timespec& out) {
    struct stat st{};
    if (::stat(dir.empty() ? "." : dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return false;
#ifdef __APPLE__
    out = st.st_mtimespec;
#else
    out = st.st_mtim;
#endif
    return true;
}

bool SameTime(const timespec& a, const timespec& b) { return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec; }

bool Rescan(const std::string& dir, DirListing& l) {
    timespec mt{};
    if (!StatMtime(dir, mt)) { l.valid = false; l.byLower.clear(); return false; }
    DIR* d = ::opendir(dir.empty() ? "." : dir.c_str());
    if (!d) { l.valid = false; l.byLower.clear(); return false; }
    l.byLower.clear();
    while (const dirent* e = ::readdir(d)) {
        l.byLower.emplace(Lower(e->d_name), e->d_name);
    }
    ::closedir(d);
    l.mtime = mt;
    l.valid = true;
    return true;
}

bool Exists(const std::string& p) {
    struct stat st{};
    return ::stat(p.c_str(), &st) == 0;
}

// Finds the real spelling of `name` in `dir`; false = no such entry (even after a refresh).
bool FindInDir(const std::string& dir, const std::string& name, std::string& real) {
    auto& l = s_Dirs[dir];
    timespec mt{};
    const bool dirOk = StatMtime(dir, mt);
    if (!dirOk) { l.valid = false; return false; }
    if (!l.valid || !SameTime(l.mtime, mt)) {
        if (!Rescan(dir, l)) return false;
    }
    const auto lname = Lower(name);
    const auto range = l.byLower.equal_range(lname);
    std::string best;
    for (auto it = range.first; it != range.second; ++it) { // several spellings (case-sensitive volume): the exact one, else the lexicographically first
        if (it->second == name) { best = it->second; break; }
        if (best.empty() || it->second < best) best = it->second;
    }
    if (best.empty()) return false;
    real = std::move(best);
    return true;
}
} // namespace

std::string ResolvePathFrom(std::string_view baseDir, std::string_view path) {
    std::string norm{path};
    for (auto& c : norm) {
        if (c == '\\') c = '/';
    }
    if (norm.empty()) return {};
    const auto join = [](const std::string& dir, const std::string& comp) {
        if (dir.empty()) return comp;
        return dir.back() == '/' ? dir + comp : dir + "/" + comp;
    };

    std::string cur{baseDir};
    size_t      pos = 0;
    if (norm[0] == '/') {
        cur = "/";
        pos = 1;
    }
    bool missing = false; // after the first non-existing component everything is copied verbatim
    std::lock_guard lk{s_Mutex};
    while (pos < norm.size()) {
        size_t end = norm.find('/', pos);
        if (end == std::string::npos) end = norm.size();
        std::string comp = norm.substr(pos, end - pos);
        pos = end + 1;
        if (comp.empty() || comp == ".") continue;
        if (comp != "..") { // Win32 path normalisation drops trailing spaces and dots of a component ("DATA\\DEFAULT.IDE " is DEFAULT.IDE)
            const auto n = comp.find_last_not_of(" .");
            if (n == std::string::npos) continue; // only spaces / dots
            comp.resize(n + 1);
        }
        if (comp == ".." || missing) {
            cur = join(cur, comp);
            continue;
        }
        std::string real;
        if (!FindInDir(cur, comp, real)) {
            real = comp;
            if (Exists(join(cur, comp))) { /* unreadable directory (search permission only): trust the spelling */ }
            else missing = true;
        }
        cur = join(cur, real);
    }
    if (cur.empty()) cur = "."; // only "." / "" components
    if (norm.back() == '/' && cur.back() != '/') cur += '/';
    return cur;
}

std::FILE* OpenResolved(const char* path, const char* mode) {
    if (!path) { errno = EINVAL; return nullptr; }
    return std::fopen(ResolvePath(path).c_str(), mode);
}

namespace {
struct TextFile {
    std::vector<char> data;
    size_t            pos{};
};
int TextRead(void* c, char* buf, int n) {
    auto* t = (TextFile*)c;
    const size_t left = t->data.size() - t->pos;
    const size_t k    = left < (size_t)n ? left : (size_t)n;
    std::memcpy(buf, t->data.data() + t->pos, k);
    t->pos += k;
    return (int)k;
}
fpos_t TextSeek(void* c, fpos_t off, int whence) {
    auto* t = (TextFile*)c;
    long long base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (long long)t->pos : (long long)t->data.size();
    const long long np = base + (long long)off;
    if (np < 0) { errno = EINVAL; return -1; }
    t->pos = (size_t)np; // like fseek on a file: seeking past the end is allowed, reads then return 0
    return (fpos_t)np;
}
int TextClose(void* c) { delete (TextFile*)c; return 0; }
} // namespace

std::FILE* OpenResolvedText(const char* path, const char* mode) {
    if (!path || !mode) { errno = EINVAL; return nullptr; }
    const bool textRead = std::strchr(mode, 'r') && !std::strchr(mode, 'b') && !std::strchr(mode, '+');
    if (!textRead) return OpenResolved(path, mode);
    std::FILE* f = OpenResolved(path, "rb");
    if (!f) return nullptr;
    auto* t = new TextFile;
    char  buf[65536];
    bool  cr = false, eof = false;
    for (size_t n; !eof && (n = std::fread(buf, 1, sizeof(buf), f)) > 0;) {
        for (size_t i = 0; i < n; i++) {
            char c = buf[i];
            if (c == 0x1A) { eof = true; break; } // Ctrl-Z ends a text-mode file
            if (cr) { // previous byte was '\r'
                cr = false;
                if (c == '\n') { t->data.push_back('\n'); continue; }
                t->data.push_back('\r');
            }
            if (c == '\r') { cr = true; continue; }
            t->data.push_back(c);
        }
    }
    if (cr) t->data.push_back('\r');
    std::fclose(f);
    std::FILE* r = ::funopen(t, TextRead, nullptr, TextSeek, TextClose);
    if (!r) delete t;
    return r;
}

void* RwFopenResolved(const char* path, const char* mode) {
    return OpenResolved(path, mode);
}

int ChdirResolved(const char* path) {
    if (!path) { errno = EINVAL; return -1; }
    return ::chdir(ResolvePath(path).c_str());
}

bool ExistsResolved(const char* path) {
    return path && Exists(ResolvePath(path));
}

void ClearPathCache() {
    std::lock_guard lk{s_Mutex};
    s_Dirs.clear();
}
} // namespace notsa::portable
#endif
