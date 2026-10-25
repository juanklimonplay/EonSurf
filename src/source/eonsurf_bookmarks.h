#ifndef EONSURF_BOOKMARKS_H
#define EONSURF_BOOKMARKS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define EON_MAX_BOOKMARKS 32
#define EON_BOOKMARK_FILE "/dev_hdd0/game/EONS00001/USRDIR/bookmarks.txt"
#define EON_HISTORY_FILE  "/dev_hdd0/game/EONS00001/USRDIR/history.txt"

typedef struct {
    char title[64];
    char url[256];
    uint32_t timestamp;
} EonBookmarkItem;

static EonBookmarkItem g_bookmarks[EON_MAX_BOOKMARKS];
static int g_bookmark_count = 0;
static int g_bookmarks_panel_open = 0;

/* Inicializar marcadores con accesos directos por defecto */
static inline void eonsurf_bookmarks_init(void) {
    g_bookmark_count = 0;
    
    /* Intentar cargar desde disco duro de PS3 */
    FILE *f = fopen(EON_BOOKMARK_FILE, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f) && g_bookmark_count < EON_MAX_BOOKMARKS) {
            char *tab = strchr(line, '\t');
            if (tab) {
                *tab = '\0';
                char *url = tab + 1;
                /* Eliminar salto de línea */
                char *nl = strchr(url, '\n');
                if (nl) *nl = '\0';
                char *cr = strchr(url, '\r');
                if (cr) *cr = '\0';

                strncpy(g_bookmarks[g_bookmark_count].title, line, sizeof(g_bookmarks[g_bookmark_count].title) - 1);
                strncpy(g_bookmarks[g_bookmark_count].url, url, sizeof(g_bookmarks[g_bookmark_count].url) - 1);
                g_bookmark_count++;
            }
        }
        fclose(f);
    }

    /* Si no había archivo guardado, crear los defaults esenciales de la scene PS3 */
    if (g_bookmark_count == 0) {
        strncpy(g_bookmarks[0].title, "PlayStation 3 (Wikipedia)", 63);
        strncpy(g_bookmarks[0].url, "https://es.wikipedia.org/wiki/PlayStation_3", 255);

        strncpy(g_bookmarks[1].title, "Cell Broadband Engine", 63);
        strncpy(g_bookmarks[1].url, "https://es.wikipedia.org/wiki/Cell_(microprocesador)", 255);

        strncpy(g_bookmarks[2].title, "webMAN MOD Documentation", 63);
        strncpy(g_bookmarks[2].url, "https://raw.githubusercontent.com/aldostools/webMAN-MOD/master/README.md", 255);

        strncpy(g_bookmarks[3].title, "PSL1GHT SDK Repository", 63);
        strncpy(g_bookmarks[3].url, "https://raw.githubusercontent.com/ps3dev/PSL1GHT/master/README.md", 255);

        strncpy(g_bookmarks[4].title, "DuckDuckGo Lite", 63);
        strncpy(g_bookmarks[4].url, "https://lite.duckduckgo.com/lite/", 255);

        g_bookmark_count = 5;
    }
}

/* Guardar marcadores persistentes en disco */
static inline void eonsurf_bookmarks_save(void) {
    FILE *f = fopen(EON_BOOKMARK_FILE, "w");
    if (f) {
        for (int i = 0; i < g_bookmark_count; i++) {
            fprintf(f, "%s\t%s\n", g_bookmarks[i].title, g_bookmarks[i].url);
        }
        fclose(f);
    }
}

/* Añadir marcador */
static inline int eonsurf_bookmark_add(const char *title, const char *url) {
    if (g_bookmark_count >= EON_MAX_BOOKMARKS || !url || strlen(url) == 0) return 0;
    
    /* Evitar duplicados exactos */
    for (int i = 0; i < g_bookmark_count; i++) {
        if (strcmp(g_bookmarks[i].url, url) == 0) return 0;
    }

    strncpy(g_bookmarks[g_bookmark_count].title, (title && strlen(title) > 0) ? title : url, 63);
    strncpy(g_bookmarks[g_bookmark_count].url, url, 255);
    g_bookmark_count++;
    eonsurf_bookmarks_save();
    return 1;
}

/* Guardar entrada en historial persistente */
static inline void eonsurf_history_log_visit(const char *title, const char *url) {
    if (!url || strncmp(url, "eonsurf://", 10) == 0) return;
    FILE *f = fopen(EON_HISTORY_FILE, "a");
    if (f) {
        fprintf(f, "%s\t%s\n", (title && strlen(title) > 0) ? title : url, url);
        fclose(f);
    }
}

#endif /* EONSURF_BOOKMARKS_H */
