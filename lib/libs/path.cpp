//
// Created by Xomagat on 19.11.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <direct.h>
#include <sys/utime.h>
#else
#include <unistd.h>
#include <utime.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "../alloc.h"

static inline const char* S(const char* x) { return x ? x : ""; }

extern "C" void* fab_list_new_string();
extern "C" void  fab_list_addend_string(void* l, const char* v);
extern "C" int   fab_list_length_string(void* l);
extern "C" const char* fab_list_get_string(void* l, int i);
extern "C" void  fab_list_sort_string(void* l);
extern "C" void  fab_list_free_string(void* l);

struct FabFileInfo
{
    long long size, mtime;
    int  mode;
    bool exists, is_dir, is_file, is_link;
};

extern "C" void* fab_fi_stat(const char* path)
{
    const char* p = S(path);
    auto* f = (FabFileInfo*)calloc(1, sizeof(FabFileInfo));
    if (!f) return nullptr;
    f->size = -1; f->mtime = -1;

#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(p, GetFileExInfoStandard, &d)) return f;
    DWORD a = d.dwFileAttributes;
    unsigned long long t = ((unsigned long long)d.ftLastWriteTime.dwHighDateTime << 32)
                         | d.ftLastWriteTime.dwLowDateTime;
    f->exists  = true;
    f->size    = ((long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    f->mtime   = (long long)(t / 10000000ULL) - 11644473600LL;
    f->is_dir  = (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    f->is_file = !f->is_dir;
    f->is_link = (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    f->mode    = (a & FILE_ATTRIBUTE_READONLY) ? 0444 : 0666;
#else
    struct stat st;
    if (lstat(p, &st) != 0) return f;
    f->is_link = S_ISLNK(st.st_mode);
    if (f->is_link && stat(p, &st) != 0) lstat(p, &st);     // dangling link: describe the link itself
    f->exists  = true;
    f->size    = (long long)st.st_size;
    f->mtime   = (long long)st.st_mtime;
    f->mode    = (int)(st.st_mode & 07777);
    f->is_dir  = S_ISDIR(st.st_mode);
    f->is_file = S_ISREG(st.st_mode);
#endif
    return f;
}

extern "C" void fab_fi_free(void* h) { free(h); }

#define FI(h) ((FabFileInfo*)(h))
extern "C" bool      fab_fi_exists(void* h)     { return h && FI(h)->exists; }
extern "C" long long fab_fi_size(void* h)       { return h ? FI(h)->size : -1; }
extern "C" long long fab_fi_mtime(void* h)      { return h ? FI(h)->mtime : -1; }
extern "C" int       fab_fi_mode(void* h)       { return h ? FI(h)->mode : 0; }
extern "C" bool      fab_fi_is_dir(void* h)     { return h && FI(h)->is_dir; }
extern "C" bool      fab_fi_is_file(void* h)    { return h && FI(h)->is_file; }
extern "C" bool      fab_fi_is_symlink(void* h) { return h && FI(h)->is_link; }

// ---------- helpers ----------
static const size_t NPOS = (size_t)-1;

// Strings live in the bump allocator, like everywhere in the stdlib
static char* dup_n(const char* s, size_t n)
{
    char* r = (char*)bump_alloc(n + 1);
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static bool is_sep(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

// Length of the root prefix: "/" -> 1, "C:/" -> 3, "C:" -> 2 (Windows), relative -> 0
static size_t root_len(const char* p)
{
#ifdef _WIN32
    if (((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z')) && p[1] == ':')
        return is_sep(p[2]) ? 3 : 2;
#endif
    return is_sep(p[0]) ? 1 : 0;
}

// Last component [b, e): trailing separators are ignored, the root is never part of it
static void last_component(const char* p, size_t& b, size_t& e)
{
    size_t r = root_len(p);
    e = strlen(p);
    while (e > r && is_sep(p[e - 1])) --e;
    b = e;
    while (b > r && !is_sep(p[b - 1])) --b;
}

// Index of the extension dot inside [b, e), or NPOS. ".bashrc", ".", ".." have no extension.
static size_t ext_pos(const char* p, size_t b, size_t e)
{
    size_t l = e - b;
    if (l == 1 && p[b] == '.') return NPOS;
    if (l == 2 && p[b] == '.' && p[b + 1] == '.') return NPOS;
    for (size_t i = e; i > b + 1; --i)
        if (p[i - 1] == '.') return i - 1;
    return NPOS;
}

// ---------- path text ----------
extern "C" const char* fab_pth_sep()
{
#ifdef _WIN32
    return "\\";
#else
    return "/";
#endif
}

// Python-like: an absolute second part replaces the first one
extern "C" char* fab_pth_join(const char* a, const char* b)
{
    a = S(a); b = S(b);
    size_t la = strlen(a), lb = strlen(b);
    if (lb == 0) return (char*)a;
    if (la == 0 || root_len(b) > 0) return (char*)b;

    bool need = !is_sep(a[la - 1]);
    char* r = (char*)bump_alloc(la + lb + 2);
    memcpy(r, a, la);
    if (need) r[la++] = '/';
    memcpy(r + la, b, lb + 1);
    return r;
}

extern "C" char* fab_pth_basename(const char* path)
{
    const char* p = S(path);
    size_t b, e;
    last_component(p, b, e);
    return dup_n(p + b, e - b);
}

// "a/b" -> "a", "a" -> "", "/a" -> "/", "a/b/" -> "a"
extern "C" char* fab_pth_dirname(const char* path)
{
    const char* p = S(path);
    size_t b, e;
    last_component(p, b, e);
    size_t r = root_len(p), d = b;
    while (d > r && is_sep(p[d - 1])) --d;
    return dup_n(p, d);
}

extern "C" char* fab_pth_extension(const char* path)
{
    const char* p = S(path);
    size_t b, e;
    last_component(p, b, e);
    size_t d = ext_pos(p, b, e);
    return d == NPOS ? dup_n("", 0) : dup_n(p + d, e - d);
}

extern "C" char* fab_pth_stem(const char* path)
{
    const char* p = S(path);
    size_t b, e;
    last_component(p, b, e);
    size_t d = ext_pos(p, b, e);
    return dup_n(p + b, (d == NPOS ? e : d) - b);
}

// The dot in ext is optional: "txt" and ".txt" are the same. Empty ext removes the extension.
extern "C" char* fab_pth_with_extension(const char* path, const char* ext)
{
    const char* p = S(path);
    const char* x = S(ext);
    if (*x == '.') ++x;

    size_t b, e;
    last_component(p, b, e);
    if (b == e) return dup_n(p, strlen(p));            // root or empty: nothing to change

    size_t d = ext_pos(p, b, e);
    size_t cut = (d == NPOS) ? e : d;
    size_t xl = strlen(x);

    char* r = (char*)bump_alloc(cut + xl + 2);
    memcpy(r, p, cut);
    size_t k = cut;
    if (xl) { r[k++] = '.'; memcpy(r + k, x, xl); k += xl; }
    r[k] = '\0';
    return r;
}

extern "C" bool fab_pth_is_absolute(const char* path)
{
    size_t r = root_len(S(path));
#ifdef _WIN32
    return r == 1 || r == 3;
#else
    return r == 1;
#endif
}

// Collapses "//", resolves "." and "..", always uses "/", no trailing separator.
// "" and "a/.." give ".", "/.." gives "/", "../x" stays "../x".
extern "C" char* fab_pth_normalize(const char* path)
{
    const char* p = S(path);
    size_t n = strlen(p), r = root_len(p);
    char* o = (char*)bump_alloc(n + 3);
    size_t len = 0;

#ifdef _WIN32
    if (r >= 2) { o[len++] = p[0]; o[len++] = ':'; }
    bool is_abs = (r == 1 || r == 3);
#else
    bool is_abs = (r == 1);
#endif
    if (is_abs) o[len++] = '/';

    size_t base = len;
    size_t i = r;
    int depth = 0;                                      // kept components that are not ".."

    while (i < n)
    {
        while (i < n && is_sep(p[i])) ++i;
        size_t s = i;
        while (i < n && !is_sep(p[i])) ++i;
        size_t cl = i - s;

        if (cl == 0 || (cl == 1 && p[s] == '.')) continue;

        if (cl == 2 && p[s] == '.' && p[s + 1] == '.')
        {
            if (depth > 0)
            {
                size_t k = len;
                while (k > base && o[k - 1] != '/') --k;       // start of the last component
                len = (k > base) ? k - 1 : base;               // and the '/' before it
                --depth;
                continue;
            }
            if (is_abs) continue;                              // "/.." == "/"
            // relative: leading ".." stays
        }
        else ++depth;

        if (len > base) o[len++] = '/';
        memcpy(o + len, p + s, cl);
        len += cl;
    }

    if (len == 0) o[len++] = '.';
    o[len] = '\0';
    return o;
}

// Root first ("/" or "C:/"), then the components
extern "C" void* fab_pth_split(const char* path)
{
    void* list = fab_list_new_string();
    const char* p = S(path);
    size_t n = strlen(p), i = root_len(p);

    if (i > 0)
    {
        char* root = dup_n(p, i);
        for (size_t k = 0; k < i; ++k) if (is_sep(root[k])) root[k] = '/';
        fab_list_addend_string(list, root);
    }
    while (i < n)
    {
        while (i < n && is_sep(p[i])) ++i;
        size_t s = i;
        while (i < n && !is_sep(p[i])) ++i;
        if (i > s) fab_list_addend_string(list, dup_n(p + s, i - s));
    }
    return list;
}

extern "C" char* fab_pth_to_native(const char* path)
{
    const char* p = S(path);
    size_t n = strlen(p);
    char* r = dup_n(p, n);
#ifdef _WIN32
    for (size_t i = 0; i < n; ++i) if (r[i] == '/') r[i] = '\\';
#endif
    return r;
}

// ---------- disk: low level ----------
static bool is_dir_follow(const char* p)
{
#ifdef _WIN32
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

// A real directory (a symlink to a directory is not one): used by walk_dir / remove_all
static bool is_dir_nofollow(const char* p)
{
#ifdef _WIN32
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)
        && !(a & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    struct stat st;
    return lstat(p, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static bool mk1(const char* p)
{
#ifdef _WIN32
    return _mkdir(p) == 0;
#else
    return mkdir(p, 0777) == 0;
#endif
}

static bool rm_dir(const char* p)
{
#ifdef _WIN32
    return RemoveDirectoryA(p) != 0;
#else
    return rmdir(p) == 0;
#endif
}

// File or symlink (on Windows also a directory symlink / junction)
static bool rm_leaf(const char* p)
{
#ifdef _WIN32
    DWORD a = GetFileAttributesA(p);
    if (a == INVALID_FILE_ATTRIBUTES) return false;
    if (a & FILE_ATTRIBUTE_READONLY) SetFileAttributesA(p, a & ~FILE_ATTRIBUTE_READONLY);
    if (a & FILE_ATTRIBUTE_DIRECTORY) return RemoveDirectoryA(p) != 0;
    return DeleteFileA(p) != 0;
#else
    return unlink(p) == 0;
#endif
}

static char* get_cwd()
{
    const size_t cap = 4096;
    char* b = (char*)malloc(cap);
    if (!b) return dup_n("", 0);
#ifdef _WIN32
    char* r = _getcwd(b, (int)cap);
#else
    char* r = getcwd(b, cap);
#endif
    char* res = r ? dup_n(b, strlen(b)) : dup_n("", 0);
    free(b);
    return res;
}

// ---------- disk: exports ----------
extern "C" bool fab_fs_exists(const char* path)
{
    const char* p = S(path);
#ifdef _WIN32
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(p, &st) == 0;
#endif
}

extern "C" bool fab_fs_is_file(const char* path)
{
    const char* p = S(path);
#ifdef _WIN32
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

// Last modification time, unix seconds; -1 if the path does not exist
extern "C" long long fab_fs_mtime(const char* path)
{
    const char* p = S(path);
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(p, GetFileExInfoStandard, &d)) return -1;
    unsigned long long t = ((unsigned long long)d.ftLastWriteTime.dwHighDateTime << 32)
                         | d.ftLastWriteTime.dwLowDateTime;
    return (long long)(t / 10000000ULL) - 11644473600LL;
#else
    struct stat st;
    if (stat(p, &st) != 0) return -1;
    return (long long)st.st_mtime;
#endif
}

// Does not touch the disk (no symlink resolving), so it works for paths that do not exist yet
extern "C" char* fab_fs_abs_path(const char* path)
{
    const char* p = S(path);
    if (fab_pth_is_absolute(p)) return fab_pth_normalize(p);
    return fab_pth_normalize(fab_pth_join(get_cwd(), p));
}

// Names only (no path, no "." and ".."), sorted. Empty list if the directory cannot be read.
extern "C" void* fab_fs_list_dir(const char* path)
{
    void* list = fab_list_new_string();
    const char* p = S(path);
    if (!*p) p = ".";

#ifdef _WIN32
    size_t n = strlen(p);
    char* pat = (char*)malloc(n + 3);
    if (!pat) return list;
    memcpy(pat, p, n);
    if (!is_sep(pat[n - 1])) pat[n++] = '\\';
    pat[n++] = '*';
    pat[n] = '\0';

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return list;
    do
    {
        const char* nm = fd.cFileName;
        if (strcmp(nm, ".") == 0 || strcmp(nm, "..") == 0) continue;
        fab_list_addend_string(list, dup_n(nm, strlen(nm)));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(p);
    if (!d) return list;
    while (dirent* e = readdir(d))
    {
        const char* nm = e->d_name;
        if (strcmp(nm, ".") == 0 || strcmp(nm, "..") == 0) continue;
        fab_list_addend_string(list, dup_n(nm, strlen(nm)));
    }
    closedir(d);
#endif

    fab_list_sort_string(list);
    return list;
}

// rel = path of the current directory relative to root ("" for root itself)
static void walk(const char* root, const char* rel, void* out)
{
    const char* full = *rel ? fab_pth_join(root, rel) : root;
    void* names = fab_fs_list_dir(full);
    int n = fab_list_length_string(names);

    for (int i = 0; i < n; ++i)
    {
        const char* nm = fab_list_get_string(names, i);
        char* child = *rel ? fab_pth_join(rel, nm) : dup_n(nm, strlen(nm));
        if (is_dir_nofollow(fab_pth_join(root, child))) walk(root, child, out);
        else fab_list_addend_string(out, child);
    }
    fab_list_free_string(names);                  // only the list itself, the strings stay valid
}

// All files below the directory, paths relative to it, "/" separators, sorted.
// Directories themselves are not listed; symlinks to directories are not followed.
extern "C" void* fab_fs_walk_dir(const char* path)
{
    void* out = fab_list_new_string();
    const char* p = S(path);
    if (!*p) p = ".";
    walk(p, "", out);
    fab_list_sort_string(out);
    return out;
}

// Like mkdir -p: true if the directory exists at the end (also when it already existed)
extern "C" bool fab_fs_make_dirs(const char* path)
{
    const char* p = S(path);
    size_t n = strlen(p);
    if (n == 0) return false;

    char* b = (char*)malloc(n + 1);
    if (!b) return false;
    memcpy(b, p, n + 1);

    size_t r = root_len(b);
    while (n > r && is_sep(b[n - 1])) b[--n] = '\0';

    for (size_t i = r; i <= n; ++i)
    {
        if (b[i] != '\0' && !is_sep(b[i])) continue;
        char c = b[i];
        b[i] = '\0';
        if (i > 0 && !is_dir_follow(b)) mk1(b);
        b[i] = c;
    }

    bool ok = is_dir_follow(p);
    free(b);
    return ok;
}

static bool rm_rec(const char* p)
{
    if (!is_dir_nofollow(p)) return rm_leaf(p);

    void* names = fab_fs_list_dir(p);
    int n = fab_list_length_string(names);
    bool ok = true;
    for (int i = 0; i < n; ++i)
        ok = rm_rec(fab_pth_join(p, fab_list_get_string(names, i))) && ok;
    fab_list_free_string(names);
    return rm_dir(p) && ok;
}

// Deletes a file or a whole directory tree. False if the path does not exist.
// Refuses "", ".", roots, and paths that end with "..".
extern "C" bool fab_fs_remove_all(const char* path)
{
    const char* p = S(path);
    char* norm = fab_pth_normalize(p);
    size_t b, e;
    last_component(norm, b, e);

    if (strcmp(norm, ".") == 0 || root_len(norm) == strlen(norm)) return false;
    if (e - b == 2 && norm[b] == '.' && norm[b + 1] == '.') return false;
    return rm_rec(p);
}

// Overwrites the destination. False if from == to (by absolute path) or on any I/O error.
extern "C" bool fab_fs_same_file(const char* a, const char* b)
{
    a = S(a); b = S(b);
#ifdef _WIN32
    return fab_fs_exists(a) && fab_fs_exists(b)
        && _stricmp(fab_fs_abs_path(a), fab_fs_abs_path(b)) == 0;
#else
    struct stat x, y;
    return stat(a, &x) == 0 && stat(b, &y) == 0 && x.st_dev == y.st_dev && x.st_ino == y.st_ino;
#endif
}

extern "C" bool fab_fs_copy_file(const char* from, const char* to)
{
    from = S(from); to = S(to);
    if (fab_fs_same_file(from, to) || strcmp(fab_fs_abs_path(from), fab_fs_abs_path(to)) == 0)
        return false;

    FILE* in = fopen(from, "rb");
    if (!in) return false;
    FILE* out = fopen(to, "wb");
    if (!out) { fclose(in); return false; }

    const size_t CAP = 1u << 16;
    char* buf = (char*)malloc(CAP);
    bool ok = buf != nullptr;

    while (ok)
    {
        size_t n = fread(buf, 1, CAP, in);
        if (n > 0 && fwrite(buf, 1, n, out) != n) ok = false;
        if (n < CAP) { if (ferror(in)) ok = false; break; }
    }

    free(buf);
    fclose(in);
    if (fclose(out) != 0) ok = false;
    return ok;
}

extern "C" char* fab_fs_temp_dir()
{
#ifdef _WIN32
    char b[MAX_PATH + 2];
    DWORD n = GetTempPathA(sizeof b, b);
    if (n == 0 || n >= sizeof b) return dup_n(".", 1);
    return fab_pth_normalize(b);
#else
    const char* t = getenv("TMPDIR");
    return fab_pth_normalize((t && *t) ? t : "/tmp");
#endif
}

extern "C" char* fab_fs_home_dir()
{
#ifdef _WIN32
    const char* h = getenv("USERPROFILE");
#else
    const char* h = getenv("HOME");
#endif
    return (h && *h) ? fab_pth_normalize(h) : dup_n("", 0);
}

// "/a/b" -> "/a/c/d"  gives  "../c/d"
extern "C" char* fab_pth_relative(const char* from, const char* to)
{
    void* a = fab_pth_split(fab_fs_abs_path(from));
    void* b = fab_pth_split(fab_fs_abs_path(to));
    int na = fab_list_length_string(a), nb = fab_list_length_string(b);

    int i = 0;
    while (i < na && i < nb && strcmp(fab_list_get_string(a, i), fab_list_get_string(b, i)) == 0) ++i;

    if (i == 0)                                       // different roots (Windows drives)
    {
        fab_list_free_string(a); fab_list_free_string(b);
        return fab_fs_abs_path(to);
    }

    size_t cap = 3 * (size_t)(na - i) + 2;
    for (int j = i; j < nb; ++j) cap += strlen(fab_list_get_string(b, j)) + 1;

    char* r = (char*)bump_alloc(cap);
    size_t k = 0;
    for (int j = i; j < na; ++j) { if (k) r[k++] = '/'; r[k++] = '.'; r[k++] = '.'; }
    for (int j = i; j < nb; ++j)
    {
        const char* s = fab_list_get_string(b, j);
        size_t l = strlen(s);
        if (k) r[k++] = '/';
        memcpy(r + k, s, l);
        k += l;
    }
    if (k == 0) r[k++] = '.';
    r[k] = '\0';

    fab_list_free_string(a); fab_list_free_string(b);
    return r;
}

extern "C" char* fab_fs_home_dir();

extern "C" char* fab_pth_expand_user(const char* path)
{
    const char* p = S(path);
    if (p[0] != '~' || (p[1] != '\0' && !is_sep(p[1]))) return (char*)p;

    char* h = fab_fs_home_dir();
    if (!*h) return (char*)p;

    const char* rest = p + 1;
    while (is_sep(*rest)) ++rest;
    return *rest ? fab_pth_join(h, rest) : h;
}

// '*' (any run, also across '/') and '?' (one char), case sensitive
static bool glob_rec(const char* p, const char* s)
{
    const char* star = nullptr;
    const char* ss = nullptr;
    while (*s)
    {
        if (*p == '*')                    { star = p++; ss = s; }
        else if (*p == '?' || *p == *s)   { ++p; ++s; }
        else if (star)                    { p = star + 1; s = ++ss; }
        else return false;
    }
    while (*p == '*') ++p;
    return *p == '\0';
}

extern "C" bool fab_pth_glob_match(const char* pattern, const char* text)
{
    return glob_rec(S(pattern), S(text));
}

extern "C" void* fab_fs_find_files(const char* dir, const char* pattern)
{
    void* all = fab_fs_walk_dir(dir);
    void* out = fab_list_new_string();
    const char* pat = S(pattern);
    bool full = strchr(pat, '/') != nullptr;          // pattern with '/': match the whole relative path

    int n = fab_list_length_string(all);
    for (int i = 0; i < n; ++i)
    {
        const char* rel = fab_list_get_string(all, i);
        const char* subject = rel;
        if (!full) { const char* sl = strrchr(rel, '/'); if (sl) subject = sl + 1; }
        if (glob_rec(pat, subject)) fab_list_addend_string(out, rel);
    }
    fab_list_free_string(all);
    return out;
}

static bool copy_tree(const char* from, const char* to)
{
    if (!fab_fs_make_dirs(to)) return false;

    void* names = fab_fs_list_dir(from);
    int n = fab_list_length_string(names);
    bool ok = true;
    for (int i = 0; i < n; ++i)
    {
        const char* nm = fab_list_get_string(names, i);
        char* s = fab_pth_join(from, nm);
        char* d = fab_pth_join(to, nm);
        if (is_dir_nofollow(s)) ok = copy_tree(s, d) && ok;
        else                    ok = fab_fs_copy_file(s, d) && ok;
    }
    fab_list_free_string(names);
    return ok;
}

// Merges into an existing destination. Refuses to copy a directory into itself.
extern "C" bool fab_fs_copy_dir(const char* from, const char* to)
{
    from = S(from); to = S(to);
    if (!is_dir_follow(from)) return false;

    char* a = fab_fs_abs_path(from);
    char* b = fab_fs_abs_path(to);
    size_t la = strlen(a);
    bool inside = strncmp(a, b, la) == 0 && (b[la] == '\0' || b[la] == '/' || a[la - 1] == '/');
    if (inside) return false;

    return copy_tree(from, to);
}

extern "C" bool fab_fs_move(const char* from, const char* to)
{
    from = S(from); to = S(to);
    if (!fab_fs_exists(from)) return false;
    if (rename(from, to) == 0) return true;

    // rename failed (other device, ...): copy, and delete the source only after a full success
    if (is_dir_nofollow(from))
    {
        if (fab_fs_exists(to) || !fab_fs_copy_dir(from, to)) return false;
        return rm_rec(from);
    }
    if (!fab_fs_copy_file(from, to)) return false;
    return rm_leaf(from);
}

// Creates an empty file, or sets the modification time to "now"
extern "C" bool fab_fs_touch(const char* path)
{
    const char* p = S(path);
    if (!*p) return false;
    if (!fab_fs_exists(p))
    {
        FILE* f = fopen(p, "ab");
        return f && fclose(f) == 0;
    }
#ifdef _WIN32
    return _utime(p, nullptr) == 0;
#else
    return utime(p, nullptr) == 0;
#endif
}

extern "C" long long fab_fs_dir_size(const char* path)
{
    const char* p = S(path);
    if (!*p) p = ".";
    void* all = fab_fs_walk_dir(p);
    int n = fab_list_length_string(all);
    long long total = 0;
    for (int i = 0; i < n; ++i)
    {
        void* fi = fab_fi_stat(fab_pth_join(p, fab_list_get_string(all, i)));
        if (fi) { if (FI(fi)->exists) total += FI(fi)->size; free(fi); }
    }
    fab_list_free_string(all);
    return total;
}

struct FabTempDir { char* path; bool keep, gone; };

static bool mk_private(const char* p)                   // 0700: other users must not see it
{
#ifdef _WIN32
    return _mkdir(p) == 0;
#else
    return mkdir(p, 0700) == 0;
#endif
}

extern "C" void* fab_tmp_new(const char* prefix)
{
    static unsigned counter = 0;
    const char* pre = S(prefix);
    if (strchr(pre, '/') || strchr(pre, '\\')) pre = "fab_";

    char* base = fab_fs_temp_dir();
#ifdef _WIN32
    unsigned pid = (unsigned)GetCurrentProcessId();
#else
    unsigned pid = (unsigned)getpid();
#endif

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        char name[96];
        snprintf(name, sizeof name, "%s%x_%x_%x", pre, pid, (unsigned)time(nullptr), ++counter);
        char* full = fab_pth_join(base, name);

        if (!mk_private(full)) continue;                // exists already: next name (mkdir is atomic)

        auto* t = (FabTempDir*)calloc(1, sizeof(FabTempDir));
        if (!t) { rm_dir(full); return nullptr; }
        size_t n = strlen(full);
        t->path = (char*)malloc(n + 1);
        if (!t->path) { rm_dir(full); free(t); return nullptr; }
        memcpy(t->path, full, n + 1);
        return t;
    }
    return nullptr;
}

#define TD(h) ((FabTempDir*)(h))

extern "C" const char* fab_tmp_path(void* h) { return h ? dup_n(TD(h)->path, strlen(TD(h)->path)) : ""; }

// A path inside the directory; "" if the result would leave it (absolute part or "..")
extern "C" char* fab_tmp_join(void* h, const char* name)
{
    if (!h) return dup_n("", 0);
    char* res = fab_pth_normalize(fab_pth_join(TD(h)->path, S(name)));
    size_t lp = strlen(TD(h)->path);
    bool inside = strncmp(res, TD(h)->path, lp) == 0 && (res[lp] == '\0' || res[lp] == '/');
    return inside ? res : dup_n("", 0);
}

extern "C" void fab_tmp_keep(void* h) { if (h) TD(h)->keep = true; }

extern "C" bool fab_tmp_remove(void* h)
{
    if (!h) return false;
    if (TD(h)->gone) return true;
    bool ok = rm_rec(TD(h)->path);
    if (ok) TD(h)->gone = true;
    return ok;
}

// Called by `delete t;` and by class destructors
extern "C" void fab_tmp_free(void* h)
{
    if (!h) return;
    if (!TD(h)->keep && !TD(h)->gone) rm_rec(TD(h)->path);
    free(TD(h)->path);
    free(h);
}