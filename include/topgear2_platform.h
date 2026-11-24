#ifndef TOPGEAR2_PLATFORM_H
#define TOPGEAR2_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Cross-platform UTF-8 aware fopen wrapper. */
FILE* tg2_fopen_utf8(const char *path, const char *mode);

/* Resolve directory containing the running executable (without trailing slash). */
bool tg2_get_exe_dir(char *out, size_t cap);

/* Resolve persistent user data directory (%APPDATA%\TopGear2Recomp or ~/.local/share/topgear2-recomp),
 * falling back to exe_dir if force_portable is true or portable.txt exists beside the executable. */
bool tg2_get_data_dir(char *out, size_t cap, bool force_portable);

/* Safely join dir and file with the platform path separator. */
void tg2_join_path(char *out, size_t cap, const char *dir, const char *file);

/* Open a native OS file picker dialog for SNES ROM files (*.sfc, *.smc). */
bool tg2_open_rom_dialog(char *out, size_t cap);

/* Scan a directory for any .sfc/.smc file matching the verified USA ROM SHA-256. */
bool tg2_find_valid_rom_in_dir(const char *dir, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* TOPGEAR2_PLATFORM_H */
