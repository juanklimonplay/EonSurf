#ifndef EONSURF_LAYOUT_H
#define EONSURF_LAYOUT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARAGRAPHS   64
#define MAX_LINKS        256
#define MAX_INFOBOX_ROWS 16

/* Caja clickeable para cuando el usuario presiona X con el cursor */
typedef struct {
    int16_t x, y, w, h;
    char url[256];
} EonLinkHitbox;

/* Párrafo o subtítulo individual en la columna izquierda (70%) */
typedef struct {
    uint8_t type;       /* 0 = Párrafo normal (18px/font), 1 = H1 (32px), 2 = H2 (24px) */
    char text[1024];    /* Texto limpio en UTF-8 */
} EonBlock;

/* Fila clave-valor para la tarjeta derecha (30% Infobox) */
typedef struct {
    char key[48];
    char value[96];
} EonInfoboxRow;

/* Página estructurada lista para renderizar a 60 FPS en RSX */
typedef struct __attribute__((aligned(128))) {
    char title[128];
    char subtitle[128];
    
    /* Columna Izquierda (70% del ancho: X=40 a X=860 en 720p) */
    uint32_t block_count;
    EonBlock blocks[MAX_PARAGRAPHS];
    
    /* Columna Derecha / Infobox (30% del ancho: X=890 a X=1240 en 720p) */
    char main_image_url[256];
    uint32_t *infobox_rgba_tex; /* Textura decodificada convertida a ARGB RSX nativo */
    uint16_t img_w, img_h;
    char img_caption[128];
    uint32_t info_row_count;
    EonInfoboxRow info_rows[MAX_INFOBOX_ROWS];

    /* Mapa de enlaces activos en el viewport actual */
    uint32_t link_count;
    EonLinkHitbox links[MAX_LINKS];
} EonPageLayout;

/* Decodifica 1 caracter UTF-8 y avanza el puntero (Soporta tildes, Ñ, símbolos) */
static inline uint32_t utf8_next_codepoint(const char **s) {
    const uint8_t *p = (const uint8_t *)(*s);
    if (*p == 0) return 0;
    if (*p < 0x80) { (*s)++; return *p; }
    if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        uint32_t cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
        (*s) += 2;
        return cp;
    }
    if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        uint32_t cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        (*s) += 3;
        return cp;
    }
    (*s)++;
    return '?';
}

/* Mezcla de píxel con antialiasing de 8 bits en formato Big-Endian ARGB de PS3 */
static inline void blend_glyph_pixel(uint32_t *fb, int pitch_pixels, int x, int y, uint8_t alpha, uint32_t color_rgb) {
    if (x < 0 || x >= 1280 || y < 0 || y >= 720 || alpha == 0) return;
    uint32_t *dst = &fb[y * pitch_pixels + x];
    if (alpha == 255) {
        *dst = 0xFF000000 | color_rgb;
        return;
    }
    uint32_t bg = *dst;
    uint32_t inv_a = 255 - alpha;
    uint32_t r = (((color_rgb >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * inv_a) >> 8;
    uint32_t g = (((color_rgb >> 8)  & 0xFF) * alpha + ((bg >> 8)  & 0xFF) * inv_a) >> 8;
    uint32_t b = ((color_rgb         & 0xFF) * alpha + (bg         & 0xFF) * inv_a) >> 8;
    *dst = 0xFF000000 | (r << 16) | (g << 8) | b;
}

/* Dibuja píldoras redondeadas con borde brillante estilo EonSurf UI con SDF entera */
static inline void draw_rounded_pill(uint32_t *fb, int pitch, int x, int y, int w, int h, int radius, uint32_t fill_col, uint32_t border_col) {
    int r2 = radius * radius;
    int inner_r2 = (radius - 2) * (radius - 2);

    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= 720) continue;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= 1280) continue;

            int dx = 0, dy = 0;
            if (px < x + radius) dx = (x + radius) - px;
            else if (px >= x + w - radius) dx = px - (x + w - radius - 1);

            if (py < y + radius) dy = (y + radius) - py;
            else if (py >= y + h - radius) dy = py - (y + h - radius - 1);

            int dist2 = dx * dx + dy * dy;
            if (dist2 <= r2) {
                int is_edge = (dist2 >= inner_r2) || (px - x < 2) || (x + w - 1 - px < 2) || (py - y < 2) || (y + h - 1 - py < 2);
                fb[py * pitch + px] = is_edge ? border_col : fill_col;
            }
        }
    }
}

#endif /* EONSURF_LAYOUT_H */
