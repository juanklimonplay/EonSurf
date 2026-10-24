#ifndef EONSURF_MOCKUP_DETAILS_H
#define EONSURF_MOCKUP_DETAILS_H

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Prototipos y llamadas a mk_blend / eonsurf_draw_mockup_text definidos en eonsurf_mockup_engine.h */
static inline void mk_blend(uint32_t *fb, int pitch, int x, int y, uint32_t rgb, int alpha);
static inline int eonsurf_draw_mockup_text(uint32_t *fb, int pitch, int x, int y, int font_px,
                                           const char *str, uint32_t color_rgb, int with_black_stroke, int max_x);

/* ============================================================================
 * 1. ICONO DEL GLOBO TERRÁQUEO (🌐) PARA LA PESTAÑA "EonSurf"
 * ============================================================================ */
static inline void eonsurf_draw_globe_icon(uint32_t *fb, int pitch, int cx, int cy) {
    const int r = 9;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            float d = sqrtf((float)(dx * dx + dy * dy));
            /* Círculo exterior + meridiano vertical elíptico + paralelo horizontal */
            int is_ring = (d >= r - 1.4f && d <= r + 0.4f);
            float meridian = sqrtf((float)(dx * dx * 4 + dy * dy));
            int is_meridian = (meridian >= r - 1.2f && meridian <= r + 0.5f);
            int is_equator = (abs(dy) <= 1 || abs(dx) <= 1) && (d < r);

            if (is_ring || is_meridian || is_equator) {
                mk_blend(fb, pitch, cx + dx, cy + dy, 0x48A8E8, 230);
            }
        }
    }
}

/* ============================================================================
 * 2. EMBLEMA AZUL ESTILO PLAYSTATION A LA DERECHA DE LA BARRA DE URL
 * ============================================================================ */
static inline void eonsurf_draw_ps_emblem(uint32_t *fb, int pitch, int x, int y) {
    uint32_t col = 0x2888E8;
    /* Asta vertical central de la 'P' */
    for (int py = y - 14; py <= y + 12; py++) {
        for (int px = x; px < x + 5; px++) {
            mk_blend(fb, pitch, px, py, col, 255);
        }
    }
    /* Arco superior derecho de la 'P' */
    for (int py = y - 14; py <= y - 1; py++) {
        for (int px = x + 5; px <= x + 15; px++) {
            float dx = (float)(px - (x + 6));
            float dy = (float)(py - (y - 7.5f));
            float d = sqrtf(dx * dx + dy * dy * 1.4f);
            if (d >= 4.2f && d <= 8.5f) {
                mk_blend(fb, pitch, px, py, col, 255);
            }
        }
    }
    /* Sombra tumbada en perspectiva ('S' inferior característica) */
    for (int py = y + 5; py <= y + 12; py++) {
        int shift = (py - (y + 5)) * 2;
        for (int px = x - 14 + shift; px < x - 2 + shift; px++) {
            mk_blend(fb, pitch, px, py, 0x1E6CC8, 230);
        }
        for (int px = x + 5 - shift / 2; px < x + 17 - shift / 2; px++) {
            mk_blend(fb, pitch, px, py, 0x1E6CC8, 230);
        }
    }
}

/* ============================================================================
 * 3. LÍNEA HORIZONTAL CONTINUA CON GLOW QUE UNE LA PESTAÑA ACTIVA
 * ============================================================================ */
static inline void eonsurf_draw_tab_baseline_glow(uint32_t *fb, int pitch, int active_tab_x, int active_tab_w) {
    int line_y = 50;
    for (int x = 0; x < 1280; x++) {
        /* No dibuja la línea por debajo de la abertura de la pestaña activa */
        if (x >= active_tab_x + 4 && x <= active_tab_x + active_tab_w - 4) continue;

        /* En los bordes cercanos a la pestaña activa brilla en cian neón; lejos se atenúa */
        int dist = (x < active_tab_x) ? (active_tab_x - x) : (x - (active_tab_x + active_tab_w));
        int glow_a = 220 - (dist / 4);
        if (glow_a < 70) glow_a = 70;

        mk_blend(fb, pitch, x, line_y,     0x55E8FF, glow_a);
        mk_blend(fb, pitch, x, line_y + 1, 0x00A8E8, glow_a / 2);
    }
}

/* ============================================================================
 * 4. CAJA DE FOCO / SELECCIÓN DETRÁS DEL PÁRRAFO BAJO EL CURSOR + CITAS [1]
 * ============================================================================ */
static inline void eonsurf_draw_paragraph_focus_box(uint32_t *fb, int pitch, int cursor_y) {
    /* Si el cursor está sobre el área de lectura, oscurece sutilmente esa franja de 2 renglones */
    if (cursor_y < 235 || cursor_y > 610) return;

    /* Alinear a la grilla de renglones de 25px */
    int band_y = 242 + ((cursor_y - 242) / 50) * 50;
    for (int py = band_y - 4; py < band_y + 48; py++) {
        if (py < 195 || py >= 650) continue;
        for (int px = 68; px < 480; px++) {
            /* Degradado horizontal que se funde hacia la derecha como en el segundo párrafo del mockup */
            int fade = 195 - ((px - 68) * 195) / 412;
            mk_blend(fb, pitch, px, py, 0x09131F, fade);
        }
    }
}

/* 
 * 5. Renderizador de Palabras con Detección Automática de Citas Superíndice [1], [2], [3]
 */
static inline int eonsurf_draw_word_with_citations(uint32_t *fb, int pitch, int x, int y,
                                                   const char *word, uint32_t col, int max_x) {
    /* Verificar si la palabra termina en una cita tipo "[1]" o "[2]" */
    const char *bracket = strchr(word, '[');
    if (bracket && strchr(bracket, ']')) {
        char main_part[96] = {0};
        size_t mlen = (size_t)(bracket - word);
        if (mlen > 0 && mlen < sizeof(main_part)) {
            memcpy(main_part, word, mlen);
            main_part[mlen] = '\0';
            x = eonsurf_draw_mockup_text(fb, pitch, x, y, 18, main_part, col, 1, max_x);
        }
        /* Dibujar el "[1]" 5 píxeles más arriba y en tamaño 13px color celeste link */
        x = eonsurf_draw_mockup_text(fb, pitch, x + 1, y - 5, 13, bracket, 0x62B8FF, 1, max_x);
        return x;
    }
    return eonsurf_draw_mockup_text(fb, pitch, x, y, 18, word, col, 1, max_x);
}

#endif /* EONSURF_MOCKUP_DETAILS_H */
