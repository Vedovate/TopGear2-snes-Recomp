#define _POSIX_C_SOURCE 200809L
#include "topgear2_platform.h"
#include "topgear2_sha256.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#else
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

static bool has_rom_extension(const char *name) {
    if (!name) return false;
    size_t len = strlen(name);
    if (len < 4u) return false;
    const char *ext = name + len - 4u;
    if (ext[0] != '.') return false;
    char c1 = (ext[1] >= 'A' && ext[1] <= 'Z') ? (char)(ext[1] + 32) : ext[1];
    char c2 = (ext[2] >= 'A' && ext[2] <= 'Z') ? (char)(ext[2] + 32) : ext[2];
    char c3 = (ext[3] >= 'A' && ext[3] <= 'Z') ? (char)(ext[3] + 32) : ext[3];
    return (c1 == 's' && c2 == 'f' && c3 == 'c') ||
           (c1 == 's' && c2 == 'm' && c3 == 'c');
}

FILE* tg2_fopen_utf8(const char *path, const char *mode) {
    if (!path || !mode) return NULL;
#ifdef _WIN32
    wchar_t wpath[1024];
    wchar_t wmode[16];
    int wp = MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 1024);
    int wm = MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 16);
    if (wp > 0 && wm > 0) {
        return _wfopen(wpath, wmode);
    }
#endif
    return fopen(path, mode);
}

void tg2_join_path(char *out, size_t cap, const char *dir, const char *file) {
    if (!out || cap == 0u) return;
    if (!dir || !dir[0]) {
        snprintf(out, cap, "%s", file ? file : "");
        return;
    }
    if (!file || !file[0]) {
        snprintf(out, cap, "%s", dir);
        return;
    }
    size_t dlen = strlen(dir);
    char last = dir[dlen - 1u];
#ifdef _WIN32
    const char *sep = (last == '\\' || last == '/') ? "" : "\\";
#else
    const char *sep = (last == '/') ? "" : "/";
#endif
    snprintf(out, cap, "%s%s%s", dir, sep, file);
}

static bool mkdir_recursive_utf8(const char *path) {
    if (!path || !path[0]) return false;
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0u) return false;
    if (tmp[len - 1u] == '/' || tmp[len - 1u] == '\\') {
        tmp[len - 1u] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
#ifdef _WIN32
            if ((p - tmp) == 2 && tmp[1] == ':') continue;
#endif
            char saved = *p;
            *p = '\0';
#ifdef _WIN32
            wchar_t wdir[1024];
            if (MultiByteToWideChar(CP_UTF8, 0, tmp, -1, wdir, 1024) > 0) {
                CreateDirectoryW(wdir, NULL);
            }
#else
            mkdir(tmp, 0755);
#endif
            *p = saved;
        }
    }

#ifdef _WIN32
    wchar_t wfinal[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, tmp, -1, wfinal, 1024) <= 0) return false;
    if (!CreateDirectoryW(wfinal, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) return false;
    }
    return true;
#else
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return false;
    return true;
#endif
}

bool tg2_get_exe_dir(char *out, size_t cap) {
    if (!out || cap == 0u) return false;
#ifdef _WIN32
    wchar_t wbuf[1024];
    DWORD len = GetModuleFileNameW(NULL, wbuf, 1024);
    if (len > 0 && len < 1024) {
        char u8[2048];
        if (WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, u8, sizeof(u8), NULL, NULL) > 0) {
            char *slash = strrchr(u8, '\\');
            char *fslash = strrchr(u8, '/');
            char *sep = (slash && fslash) ? ((slash > fslash) ? slash : fslash) : (slash ? slash : fslash);
            if (sep) {
                *sep = '\0';
                snprintf(out, cap, "%s", u8);
                return true;
            }
        }
    }
#elif defined(__APPLE__)
    char raw[1024];
    uint32_t sz = sizeof(raw);
    if (_NSGetExecutablePath(raw, &sz) == 0) {
        char resolved[1024];
        const char *src = realpath(raw, resolved) ? resolved : raw;
        char copy[1024];
        snprintf(copy, sizeof(copy), "%s", src);
        char *sep = strrchr(copy, '/');
        if (sep) {
            if (sep == copy) sep[1] = '\0';
            else *sep = '\0';
            snprintf(out, cap, "%s", copy);
            return true;
        }
    }
#else
    char raw[1024];
    ssize_t len = readlink("/proc/self/exe", raw, sizeof(raw) - 1u);
    if (len > 0) {
        raw[len] = '\0';
        char *sep = strrchr(raw, '/');
        if (sep) {
            if (sep == raw) sep[1] = '\0';
            else *sep = '\0';
            snprintf(out, cap, "%s", raw);
            return true;
        }
    }
#endif
    snprintf(out, cap, ".");
    return false;
}

bool tg2_get_data_dir(char *out, size_t cap, bool force_portable) {
    if (!out || cap == 0u) return false;

    char exe_dir[1024];
    tg2_get_exe_dir(exe_dir, sizeof(exe_dir));

    char portable_marker[1024];
    tg2_join_path(portable_marker, sizeof(portable_marker), exe_dir, "portable.txt");
    FILE *pf = tg2_fopen_utf8(portable_marker, "rb");
    if (pf) {
        fclose(pf);
        force_portable = true;
    }

    if (force_portable) {
        snprintf(out, cap, "%s", exe_dir);
        return true;
    }

#ifdef _WIN32
    const wchar_t *wappdata = _wgetenv(L"APPDATA");
    if (wappdata && wappdata[0]) {
        char appdata_u8[1024];
        if (WideCharToMultiByte(CP_UTF8, 0, wappdata, -1, appdata_u8, sizeof(appdata_u8), NULL, NULL) > 0) {
            char data_dir[1024];
            tg2_join_path(data_dir, sizeof(data_dir), appdata_u8, "TopGear2Recomp");
            if (mkdir_recursive_utf8(data_dir)) {
                snprintf(out, cap, "%s", data_dir);
                return true;
            }
        }
    }
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    char base[1024];
    bool have_base = false;
    if (xdg && xdg[0]) {
        snprintf(base, sizeof(base), "%s", xdg);
        have_base = true;
    } else {
        const char *home = getenv("HOME");
        if (home && home[0]) {
            tg2_join_path(base, sizeof(base), home, ".local/share");
            have_base = true;
        }
    }
    if (have_base) {
        char data_dir[1024];
        tg2_join_path(data_dir, sizeof(data_dir), base, "topgear2-recomp");
        if (mkdir_recursive_utf8(data_dir)) {
            snprintf(out, cap, "%s", data_dir);
            return true;
        }
    }
#endif

    snprintf(out, cap, "%s", exe_dir);
    return true;
}

bool tg2_open_rom_dialog(char *out, size_t cap) {
    if (!out || cap == 0u) return false;
    out[0] = '\0';

#ifdef _WIN32
    typedef BOOL (WINAPI *PFN_GetOpenFileNameW)(LPOPENFILENAMEW);
    HMODULE comdlg = LoadLibraryA("comdlg32.dll");
    if (!comdlg) return false;
    PFN_GetOpenFileNameW pGetOpenFileNameW =
        (PFN_GetOpenFileNameW)(uintptr_t)GetProcAddress(comdlg, "GetOpenFileNameW");
    if (!pGetOpenFileNameW) {
        FreeLibrary(comdlg);
        return false;
    }

    wchar_t wfile[1024];
    memset(wfile, 0, sizeof(wfile));

    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"SNES ROM (*.sfc;*.smc)\0*.sfc;*.smc\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = wfile;
    ofn.nMaxFile = 1024;
    ofn.lpstrTitle = L"Select Top Gear 2 (USA) SNES ROM";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    BOOL ok = pGetOpenFileNameW(&ofn);
    FreeLibrary(comdlg);
    if (!ok) return false;

    int converted = WideCharToMultiByte(CP_UTF8, 0, wfile, -1, out, (int)cap, NULL, NULL);
    return converted > 0 && out[0] != '\0';
#elif defined(__APPLE__)
    FILE *p = popen("osascript -e 'POSIX path of (choose file of type {\"sfc\",\"smc\"} with prompt \"Select Top Gear 2 (USA) ROM\")' 2>/dev/null", "r");
    if (!p) return false;
    char buf[1024];
    bool ok = (fgets(buf, sizeof(buf), p) != NULL);
    int rc = pclose(p);
    if (!ok || rc != 0) return false;
    size_t len = strlen(buf);
    while (len > 0u && (buf[len - 1u] == '\n' || buf[len - 1u] == '\r')) buf[--len] = '\0';
    if (len == 0u) return false;
    snprintf(out, cap, "%s", buf);
    return true;
#else
    const char *cmds[2] = {
        "zenity --file-selection --title=\"Select Top Gear 2 (USA) ROM\" --file-filter=\"SNES ROMs (*.sfc *.smc) | *.sfc *.smc\" 2>/dev/null",
        "kdialog --getopenfilename . \"*.sfc *.smc|SNES ROMs\" 2>/dev/null"
    };
    for (int i = 0; i < 2; i++) {
        FILE *p = popen(cmds[i], "r");
        if (!p) continue;
        char buf[1024];
        bool got = (fgets(buf, sizeof(buf), p) != NULL);
        int rc = pclose(p);
        if (got && rc == 0) {
            size_t len = strlen(buf);
            while (len > 0u && (buf[len - 1u] == '\n' || buf[len - 1u] == '\r')) buf[--len] = '\0';
            if (len > 0u) {
                snprintf(out, cap, "%s", buf);
                return true;
            }
        }
    }
    return false;
#endif
}

static bool check_rom_candidate(const char *path) {
    FILE *f = tg2_fopen_utf8(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long fsz = ftell(f);
    if (fsz <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return false; }

    size_t sz = (size_t)fsz;
    if (sz != TOPGEAR2_ROM_SIZE && sz != (TOPGEAR2_ROM_SIZE + 512u)) {
        fclose(f);
        return false;
    }

    uint8_t *buf = (uint8_t*)malloc(sz);
    if (!buf) { fclose(f); return false; }
    size_t nr = fread(buf, 1u, sz, f);
    fclose(f);
    if (nr != sz) { free(buf); return false; }

    const uint8_t *rom_ptr = ((sz % 1024u) == 512u) ? (buf + 512u) : buf;
    size_t rom_len = ((sz % 1024u) == 512u) ? (sz - 512u) : sz;
    bool ok = (tg2_verify_rom_buffer(rom_ptr, rom_len, NULL) == TG2_ROM_OK);
    free(buf);
    return ok;
}

bool tg2_find_valid_rom_in_dir(const char *dir, char *out, size_t cap) {
    if (!dir || !dir[0] || !out || cap == 0u) return false;
    out[0] = '\0';

#ifdef _WIN32
    char pattern[1024];
    tg2_join_path(pattern, sizeof(pattern), dir, "*");
    wchar_t wpat[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, pattern, -1, wpat, 1024) <= 0) return false;

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(wpat, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        char fname[512];
        if (WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, fname, sizeof(fname), NULL, NULL) <= 0) continue;
        if (!has_rom_extension(fname)) continue;

        char full[1024];
        tg2_join_path(full, sizeof(full), dir, fname);
        if (check_rom_candidate(full)) {
            snprintf(out, cap, "%s", full);
            found = true;
            break;
        }
    } while (FindNextFileW(h, &fd));

    FindClose(h);
    return found;
#else
    DIR *dp = opendir(dir);
    if (!dp) return false;
    struct dirent *ent;
    bool found = false;
    while ((ent = readdir(dp)) != NULL) {
        if (!has_rom_extension(ent->d_name)) continue;
        char full[1024];
        tg2_join_path(full, sizeof(full), dir, ent->d_name);
        if (check_rom_candidate(full)) {
            snprintf(out, cap, "%s", full);
            found = true;
            break;
        }
    }
    closedir(dp);
    return found;
#endif
}
