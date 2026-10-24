#ifndef EONSURF_DOWNLOADER_H
#define EONSURF_DOWNLOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <curl/curl.h>

typedef struct {
    int      active;          /* 1 = Descargando, 2 = Completado, -1 = Error, 0 = Inactivo */
    char     filename[128];
    char     dest_path[256];
    uint64_t bytes_now;
    uint64_t bytes_total;
    int      percent;
    int      status_timer;    /* Mantiene el cartel unos segundos en pantalla */
    FILE    *fp;
} EonDownloadState;

static EonDownloadState g_dl = {0};
static char g_disk_write_buf[65536] __attribute__((aligned(128))); /* Buffer de 64 KB alineado a caché Cell */

/* 
 * 1. Detecta si la URL clickeada con X apunta a un archivo descargable (.pkg, .zip, .rar, .7z, .iso)
 */
static inline int eonsurf_is_downloadable_file(const char *url) {
    if (!url) return 0;
    const char *q = strchr(url, '?');
    size_t len = q ? (size_t)(q - url) : strlen(url);
    if (len < 5) return 0;

    const char *ext = url + len - 4;
    if (strncasecmp(ext, ".pkg", 4) == 0 ||
        strncasecmp(ext, ".zip", 4) == 0 ||
        strncasecmp(ext, ".rar", 4) == 0 ||
        strncasecmp(ext, ".iso", 4) == 0 ||
        (len > 3 && strncasecmp(url + len - 3, ".7z", 3) == 0)) {
        return 1;
    }
    return 0;
}

/* Extrae el nombre limpio del archivo desde la URL (ej. "webMAN_MOD_1.47.46.pkg") */
static inline void extract_clean_filename(const char *url, char *out_name, size_t max_len) {
    const char *slash = strrchr(url, '/');
    const char *start = slash ? (slash + 1) : url;
    const char *q = strchr(start, '?');
    size_t len = q ? (size_t)(q - start) : strlen(start);

    if (len == 0 || len >= max_len) {
        strncpy(out_name, "EonSurf_Package.pkg", max_len - 1);
        out_name[max_len - 1] = '\0';
        return;
    }
    memcpy(out_name, start, len);
    out_name[len] = '\0';
}

/* Callback de escritura directa a disco en bloques de 64 KB */
static size_t pkg_write_disk_cb(void *ptr, size_t size, size_t nmemb, void *stream) {
    FILE *fp = (FILE *)stream;
    if (!fp) return 0;
    return fwrite(ptr, size, nmemb, fp);
}

/* Callback de progreso en vivo de libcurl */
static int pkg_progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                           curl_off_t ultotal, curl_off_t ulnow) {
    (void)clientp; (void)ultotal; (void)ulnow;
    g_dl.bytes_now   = (uint64_t)dlnow;
    g_dl.bytes_total = (uint64_t)dltotal;
    if (dltotal > 0) {
        g_dl.percent = (int)((dlnow * 100) / dltotal);
    }
    return 0; /* Retornar != 0 si se desea cancelar */
}

/* 
 * 2. Inicia la descarga del archivo hacia la carpeta oficial de la PS3:
 * - Si es .pkg -> /dev_hdd0/packages/
 * - Si es otro -> /dev_hdd0/downloads/
 */
static inline int eonsurf_start_direct_download(const char *file_url) {
    if (g_dl.active == 1) return 0; /* Ya hay una descarga en curso */

    extract_clean_filename(file_url, g_dl.filename, sizeof(g_dl.filename));

    /* Crear directorios destino en el disco duro de la PS3 por si no existen */
    mkdir("/dev_hdd0/packages", 0777);
    mkdir("/dev_hdd0/downloads", 0777);

    int is_pkg = (strstr(g_dl.filename, ".pkg") != NULL || strstr(g_dl.filename, ".PKG") != NULL);
    snprintf(g_dl.dest_path, sizeof(g_dl.dest_path), "%s/%s",
             is_pkg ? "/dev_hdd0/packages" : "/dev_hdd0/downloads",
             g_dl.filename);

    g_dl.fp = fopen(g_dl.dest_path, "wb");
    if (!g_dl.fp) {
        g_dl.active = -1;
        g_dl.status_timer = 180;
        return 0;
    }

    /* Activar buffer de 64 KB alineado a 128 bytes para el disco mecánico */
    setvbuf(g_dl.fp, g_disk_write_buf, _IOFBF, sizeof(g_disk_write_buf));

    g_dl.active = 1;
    g_dl.bytes_now = 0;
    g_dl.bytes_total = 0;
    g_dl.percent = 0;

    CURL *curl = curl_easy_init();
    if (!curl) {
        fclose(g_dl.fp);
        g_dl.fp = NULL;
        g_dl.active = -1;
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, file_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, pkg_write_disk_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, g_dl.fp);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, pkg_progress_cb);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); /* Crítico para redirecciones de GitHub Releases */
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "EonSurf/1.0 (PlayStation 3; PKG-Downloader)");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 65536L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (g_dl.fp) {
        fflush(g_dl.fp);
        fclose(g_dl.fp);
        g_dl.fp = NULL;
    }

    if (res == CURLE_OK) {
        g_dl.active = 2; /* Completado con éxito */
        g_dl.percent = 100;
        g_dl.status_timer = 240; /* Cartel verde durante 4 segundos (240 frames a 60 FPS) */
        return 1;
    } else {
        g_dl.active = -1; /* Error de red */
        g_dl.status_timer = 180;
        return 0;
    }
}

#endif /* EONSURF_DOWNLOADER_H */
