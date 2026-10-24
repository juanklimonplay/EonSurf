#ifndef EONSURF_INTERACTIVE_MEDIA_H
#define EONSURF_INTERACTIVE_MEDIA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <curl/curl.h>

#define MAX_PAGE_LINKS 128

typedef struct {
    char label[128];
    char url[256];
    int16_t x, y, w, h;       /* Coordenadas calculadas al dibujar en pantalla */
    uint8_t visible;          /* 1 si está dentro del área visible actual */
} EonCleanLink;

/* Estado global de enlaces e imagen de la pestaña activa */
static EonCleanLink g_links[MAX_PAGE_LINKS];
static int g_link_count = 0;

static uint32_t *g_infobox_tex = NULL;
static int g_infobox_w = 0;
static int g_infobox_h = 0;

/* Dibuja una línea horizontal rápida (para subrayar enlaces) */
static inline void draw_hline_fast(uint32_t *fb, int pitch, int x1, int x2, int y, uint32_t color) {
    if (y < 0 || y >= 720) return;
    if (x1 < 0) x1 = 0;
    if (x2 >= 1280) x2 = 1279;
    for (int x = x1; x <= x2; x++) {
        fb[y * pitch + x] = color;
    }
}

/* 
 * Llamar cuando el usuario presiona el Botón X (Cruz):
 * Devuelve la URL del enlace bajo el cursor, o NULL si no hizo clic sobre un link.
 */
static inline const char* eonsurf_check_link_click(int cursor_x, int cursor_y) {
    for (int i = 0; i < g_link_count; i++) {
        if (!g_links[i].visible) continue;
        if (cursor_x >= g_links[i].x && cursor_x <= (g_links[i].x + g_links[i].w) &&
            cursor_y >= g_links[i].y && cursor_y <= (g_links[i].y + g_links[i].h)) {
            return g_links[i].url;
        }
    }
    return NULL;
}

/* Blitter con escalado en punto fijo para recuadro del Infobox */
static inline void eonsurf_blit_infobox_image(uint32_t *fb, int pitch_pixels,
                                              const uint32_t *img_argb, int img_w, int img_h,
                                              int dst_x, int dst_y, int dst_w, int dst_h) {
    if (!img_argb || img_w <= 0 || img_h <= 0) return;

    for (int y = 0; y < dst_h; y++) {
        int py = dst_y + y;
        if (py < 0 || py >= 720) continue;

        int src_y = (y * img_h) / dst_h;
        const uint32_t *src_row = &img_argb[src_y * img_w];
        uint32_t *dst_row = &fb[py * pitch_pixels + dst_x];

        for (int x = 0; x < dst_w; x++) {
            int px = dst_x + x;
            if (px < 0 || px >= 1280) continue;
            int src_x = (x * img_w) / dst_w;
            dst_row[x] = src_row[src_x];
        }
    }
}

typedef struct {
    uint8_t *data;
    size_t size;
    size_t cap;
} EonImgBuffer;

static size_t img_curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    EonImgBuffer *mem = (EonImgBuffer *)userp;
    if (mem->size + realsize >= mem->cap) return 0; /* Límite duro anti-OOM (2 MB máx) */
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    return realsize;
}

/* Busca la primera URL de imagen (.png o .jpg) en el HTML o Markdown */
static inline int eonsurf_find_first_image_url(const char *raw_source, char *out_img_url, size_t max_len) {
    const char *p = raw_source;
    while ((p = strstr(p, "https://")) != NULL) {
        const char *end = p;
        while (*end && *end != '"' && *end != '\'' && *end != ')' && *end != ' ' && *end != '\n' && *end != '>') {
            end++;
        }
        size_t len = (size_t)(end - p);
        if (len > 12 && len < max_len) {
            if (strncmp(end - 4, ".png", 4) == 0 || strncmp(end - 4, ".jpg", 4) == 0 || strncmp(end - 5, ".jpeg", 5) == 0) {
                memcpy(out_img_url, p, len);
                out_img_url[len] = '\0';
                return 1;
            }
        }
        p = end;
    }

    const char *wiki_img = strstr(raw_source, "//upload.wikimedia.org/");
    if (wiki_img) {
        const char *end = wiki_img;
        while (*end && *end != '"' && *end != '\'' && *end != ' ') end++;
        size_t len = (size_t)(end - wiki_img);
        if (len + 7 < max_len && (strncmp(end - 4, ".png", 4) == 0 || strncmp(end - 4, ".jpg", 4) == 0)) {
            snprintf(out_img_url, max_len, "https:%.*s", (int)len, wiki_img);
            return 1;
        }
    }
    return 0;
}

#endif /* EONSURF_INTERACTIVE_MEDIA_H */
