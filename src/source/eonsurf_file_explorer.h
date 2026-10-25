#ifndef EONSURF_FILE_EXPLORER_H
#define EONSURF_FILE_EXPLORER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/*
 * Genera una página web HTML/Markdown navegable para el explorador de archivos local
 * Soporta file:///dev_hdd0/, file:///dev_usb000/, file:///app_home/
 */
static inline int eonsurf_generate_local_directory_listing(const char *path, char *out_body, size_t max_body) {
    if (!path || !out_body || max_body < 256) return 0;

    const char *fs_path = path;
    if (strncmp(fs_path, "file://", 7) == 0) fs_path += 7;

    DIR *d = opendir(fs_path);
    if (!d) {
        snprintf(out_body, max_body,
                 "# Error al abrir directorio local\n\n"
                 "No se pudo acceder a la ruta: `%s`\n\n"
                 "- [Volver al Portal EonSurf](eonsurf://portal)\n"
                 "- [Explorar /dev_hdd0/](file:///dev_hdd0/)\n"
                 "- [Explorar /dev_usb000/](file:///dev_usb000/)",
                 fs_path);
        return 1;
    }

    size_t di = 0;
    di += snprintf(&out_body[di], max_body - di,
                   "# Explorador de Archivos PS3\n\n"
                   "Directorio actual: `%s`\n\n"
                   "- [.. (Directorio Superior)](file:///dev_hdd0/)\n\n"
                   "## Contenido:\n",
                   fs_path);

    struct dirent *dir;
    int count = 0;
    while ((dir = readdir(d)) != NULL && di + 256 < max_body && count < 64) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        char full_entry_path[512];
        snprintf(full_entry_path, sizeof(full_entry_path), "%s%s%s",
                 fs_path,
                 (fs_path[strlen(fs_path) - 1] == '/') ? "" : "/",
                 dir->d_name);

        int is_dir = 0;
        struct stat st;
        if (stat(full_entry_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            is_dir = 1;
        }

        if (is_dir) {
            di += snprintf(&out_body[di], max_body - di,
                           " - [[DIR] %s](file://%s/)\n",
                           dir->d_name, full_entry_path);
        } else {
            /* Identificar si es archivo .PKG, .TXT o .MD */
            di += snprintf(&out_body[di], max_body - di,
                           " - [[FILE] %s](file://%s)\n",
                           dir->d_name, full_entry_path);
        }
        count++;
    }
    closedir(d);

    if (count == 0 && di + 64 < max_body) {
        di += snprintf(&out_body[di], max_body - di, "*(Directorio vacio)*\n");
    }

    return 1;
}

#endif /* EONSURF_FILE_EXPLORER_H */
