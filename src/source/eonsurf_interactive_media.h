#ifndef EONSURF_INTERACTIVE_MEDIA_H
#define EONSURF_INTERACTIVE_MEDIA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <curl/curl.h>

#include "eonsurf_md_clean.h"

#define MAX_PAGE_LINKS 128

/* Estado global de enlaces e imagen de la pestaña activa */
extern EonCleanLink g_links[MAX_PAGE_LINKS];
extern int g_link_count;

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
        if (cursor_x >= (g_links[i].x - 4) && cursor_x <= (g_links[i].x + g_links[i].w + 4) &&
            cursor_y >= (g_links[i].y - 3) && cursor_y <= (g_links[i].y + g_links[i].h + 5)) {
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

/* Busca la primera URL de imagen (.png o .jpg) relevante en el HTML o Markdown */
static inline int eonsurf_find_first_image_url(const char *raw_source, char *out_img_url, size_t max_len) {
    if (!raw_source || !out_img_url || max_len < 32) return 0;

    /* 1. Priorizar fotos reales (.jpg/.jpeg) de Wikimedia o sitios web (evitar logotipos de 20px) */
    const char *scan = raw_source;
    while (scan && *scan) {
        const char *m1 = strstr(scan, "//thumb.wikimedia.org/");
        const char *m2 = strstr(scan, "//upload.wikimedia.org/");
        const char *found = NULL;
        if (m1 && m2) found = (m1 < m2) ? m1 : m2;
        else if (m1) found = m1;
        else if (m2) found = m2;
        if (!found) break;

        const char *end = found;
        while (*end && *end != '"' && *end != '\'' && *end != ' ' && *end != '>' && *end != '&') end++;
        size_t len = (size_t)(end - found);
        if (len > 12 && len + 8 < max_len) {
            char candidate[512] = "";
            snprintf(candidate, sizeof(candidate), "https:%.*s", (int)len, found);
            
            /* Evitar iconos de botones, logos diminutos y SVG rasterizados miniatura */
            if (strstr(candidate, "PlayStationTriangle") == NULL &&
                strstr(candidate, "PlayStationCircle") == NULL &&
                strstr(candidate, "PlayStationCross") == NULL &&
                strstr(candidate, "PlayStationSquare") == NULL &&
                strstr(candidate, "Wikidata") == NULL &&
                strstr(candidate, "Commons-logo") == NULL &&
                (strstr(candidate, ".jpg") != NULL || strstr(candidate, ".jpeg") != NULL || strstr(candidate, ".png") != NULL)) {
                /* Si Wikimedia entrega un thumbnail gigante de 1280px, reducirlo a 250px para RSX VRAM (140KB vs 3.5MB) */
                char *px_pos = strstr(candidate, "/1280px-");
                if (!px_pos) px_pos = strstr(candidate, "/800px-");
                if (!px_pos) px_pos = strstr(candidate, "/640px-");
                if (px_pos) {
                    char opt_cand[512] = "";
                    size_t p_prefix = (size_t)(px_pos - candidate);
                    const char *p_suffix = strstr(px_pos + 1, "-");
                    if (p_suffix) {
                        snprintf(opt_cand, sizeof(opt_cand), "%.*s/250px%s", (int)p_prefix, candidate, p_suffix);
                        strncpy(out_img_url, opt_cand, max_len - 1);
                        out_img_url[max_len - 1] = '\0';
                        return 1;
                    }
                }
                strncpy(out_img_url, candidate, max_len - 1);
                out_img_url[max_len - 1] = '\0';
                return 1;
            }
        }
        scan = end + 1;
    }

    /* 2. Soporte para etiquetas estándar <img ... src="..."> */
    const char *img_tag = raw_source;
    while ((img_tag = strstr(img_tag, "<img")) != NULL || (img_tag = strstr(img_tag, "<IMG")) != NULL) {
        const char *tag_end = strchr(img_tag, '>');
        if (!tag_end) break;
        const char *src_attr = strstr(img_tag, "src=\"");
        char quote = '"';
        if (!src_attr || src_attr > tag_end) {
            src_attr = strstr(img_tag, "src='");
            quote = '\'';
        }
        if (src_attr && src_attr < tag_end) {
            const char *val_start = src_attr + 5;
            const char *val_end = strchr(val_start, quote);
            if (val_end && val_end <= tag_end) {
                size_t vlen = (size_t)(val_end - val_start);
                if (vlen > 8 && vlen + 16 < max_len) {
                    if (val_start[0] == '/' && val_start[1] == '/') {
                        snprintf(out_img_url, max_len, "https:%.*s", (int)vlen, val_start);
                    } else if (strncmp(val_start, "http", 4) == 0) {
                        snprintf(out_img_url, max_len, "%.*s", (int)vlen, val_start);
                    } else {
                        img_tag = tag_end;
                        continue;
                    }
                    /* Descartar iconos diminutos de 1px o trackers */
                    if (strstr(out_img_url, "1px") == NULL && strstr(out_img_url, "tracking") == NULL) {
                        return 1;
                    }
                }
            }
        }
        img_tag = tag_end;
    }

    /* 3. Escaneo libre de URLs https con extensiones de imagen */
    const char *p = raw_source;
    while ((p = strstr(p, "https://")) != NULL) {
        const char *end = p;
        while (*end && *end != '"' && *end != '\'' && *end != ')' && *end != ' ' && *end != '\n' && *end != '>') {
            end++;
        }
        size_t len = (size_t)(end - p);
        if (len > 12 && len < max_len) {
            char temp_url[512] = "";
            size_t clen = len < sizeof(temp_url) - 1 ? len : sizeof(temp_url) - 1;
            memcpy(temp_url, p, clen);
            temp_url[clen] = '\0';
            char *q = strchr(temp_url, '?');
            if (q) *q = '\0'; /* Omitir query params para chequear extensión */
            size_t tlen = strlen(temp_url);
            if (tlen > 4) {
                if (strcasecmp(temp_url + tlen - 4, ".png") == 0 ||
                    strcasecmp(temp_url + tlen - 4, ".jpg") == 0 ||
                    strcasecmp(temp_url + tlen - 5, ".jpeg") == 0) {
                    strncpy(out_img_url, p, len < max_len - 1 ? len : max_len - 1);
                    out_img_url[len < max_len - 1 ? len : max_len - 1] = '\0';
                    return 1;
                }
            }
        }
        p = end;
    }
    return 0;
}


/* Descarga la imagen con libcurl y la decodifica a ARGB Big-Endian para el RSX */
static inline int eonsurf_fetch_and_decode_infobox_image(const char *img_url) {
    if (g_infobox_tex) {
        free(g_infobox_tex);
        g_infobox_tex = NULL;
        g_infobox_w = 0;
        g_infobox_h = 0;
    }

    EonImgBuffer buf;
    buf.cap = 2 * 1024 * 1024;
    buf.size = 0;
    buf.data = (uint8_t *)malloc(buf.cap);
    if (!buf.data) return 0;

    CURL *curl = curl_easy_init();
    if (!curl) { free(buf.data); return 0; }

    curl_easy_setopt(curl, CURLOPT_URL, img_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, img_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/120.0.0.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || buf.size < 64) {
        free(buf.data);
        return 0;
    }

    int w = 0, h = 0, ch = 0;
    uint8_t *raw_rgba = stbi_load_from_memory(buf.data, (int)buf.size, &w, &h, &ch, 4);
    free(buf.data);

    if (!raw_rgba || w <= 0 || h <= 0) {
        if (raw_rgba) stbi_image_free(raw_rgba);
        return 0;
    }

    g_infobox_tex = (uint32_t *)malloc(w * h * sizeof(uint32_t));
    if (g_infobox_tex) {
        for (int i = 0; i < w * h; i++) {
            uint8_t r = raw_rgba[i * 4 + 0];
            uint8_t g = raw_rgba[i * 4 + 1];
            uint8_t b = raw_rgba[i * 4 + 2];
            uint8_t a = raw_rgba[i * 4 + 3];
            if (a < 255) {
                r = (r * a + 0x20 * (255 - a)) >> 8;
                g = (g * a + 0x26 * (255 - a)) >> 8;
                b = (b * a + 0x2E * (255 - a)) >> 8;
            }
            g_infobox_tex[i] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
        g_infobox_w = w;
        g_infobox_h = h;
    }

    stbi_image_free(raw_rgba);
    return (g_infobox_tex != NULL);
}

#endif /* EONSURF_INTERACTIVE_MEDIA_H */
