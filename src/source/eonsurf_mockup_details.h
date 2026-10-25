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
    const int r = 10;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            float d = sqrtf((float)(dx * dx + dy * dy));
            /* Círculo exterior + meridiano vertical elíptico + paralelo horizontal */
            int is_ring = (d >= r - 1.4f && d <= r + 0.4f);
            float meridian = sqrtf((float)(dx * dx * 3.8f + dy * dy));
            int is_meridian = (meridian >= r - 1.2f && meridian <= r + 0.5f);
            int is_equator = (abs(dy) <= 1) && (d < r);

            if (is_ring || is_meridian || is_equator) {
                mk_blend(fb, pitch, cx + dx, cy + dy, 0x48D8FF, 255);
            }
        }
    }
}

/* ============================================================================
 * 2. EMBLEMA HEXAGONAL NEÓN [ES] (EonSurf) A LA DERECHA
 * ============================================================================ */
static inline void eonsurf_draw_hex_es_emblem(uint32_t *fb, int pitch, int cx, int cy) {
    /* Hexágono estilizado sci-fi con glow */
    int r = 22;
    for (int dy = -r - 2; dy <= r + 2; dy++) {
        for (int dx = -r - 2; dx <= r + 2; dx++) {
            float adx = fabsf((float)dx);
            float ady = fabsf((float)dy);
            /* Distancia hexagonal */
            float hdist = (adx * 0.866025f + ady * 0.5f);
            float max_d = (ady > hdist) ? ady : hdist;

            if (max_d <= (float)r && max_d >= (float)(r - 2)) {
                mk_blend(fb, pitch, cx + dx, cy + dy, 0x38E8FF, 255);
            } else if (max_d < (float)(r - 2) && max_d >= (float)(r - 4)) {
                mk_blend(fb, pitch, cx + dx, cy + dy, 0x0C2236, 180);
            } else if (max_d <= (float)(r + 3) && max_d > (float)r) {
                int glow = (int)((1.0f - (max_d - r) / 3.0f) * 110.0f);
                mk_blend(fb, pitch, cx + dx, cy + dy, 0x00E5FF, glow);
            }
        }
    }
    /* Letras "ES" centradas con tipografía nítida */
    eonsurf_draw_mockup_text(fb, pitch, cx - 12, cy - 9, 16, "ES", 0x48F8FF, 0, cx + 18);
}

/* Emblema PlayStation retrocompatible si se requiere */
static inline void eonsurf_draw_ps_emblem(uint32_t *fb, int pitch, int x, int y) {
    eonsurf_draw_hex_es_emblem(fb, pitch, x, y);
}

/* ============================================================================
 * 3. LÍNEA HORIZONTAL CONTINUA CON GLOW QUE UNE LA PESTAÑA ACTIVA
 * ============================================================================ */
static inline void eonsurf_draw_tab_baseline_glow(uint32_t *fb, int pitch, int active_tab_x, int active_tab_w) {
    int line_y = 50;
    for (int x = 0; x < 1280; x++) {
        /* No dibuja la línea por debajo de la abertura de la pestaña activa */
        if (x >= active_tab_x + 6 && x <= active_tab_x + active_tab_w - 6) continue;

        /* En los bordes cercanos a la pestaña activa brilla en cian neón; lejos se atenúa */
        int dist = (x < active_tab_x) ? (active_tab_x - x) : (x - (active_tab_x + active_tab_w));
        int glow_a = 230 - (dist / 4);
        if (glow_a < 70) glow_a = 70;

        mk_blend(fb, pitch, x, line_y,     0x55E8FF, glow_a);
        mk_blend(fb, pitch, x, line_y + 1, 0x00A8E8, glow_a / 2);
    }
}

/* ============================================================================
 * 4. CAJA DE FOCO / SELECCIÓN DETRÁS DEL PÁRRAFO BAJO EL CURSOR + CITAS [1]
 * ============================================================================ */
static inline void eonsurf_draw_paragraph_focus_box(uint32_t *fb, int pitch, int cursor_y) {
    /* Si el cursor está sobre el área de lectura, resalta sutilmente esa franja */
    if (cursor_y < 235 || cursor_y > 610) return;

    /* Alinear a la grilla de renglones de 24px */
    int band_y = 238 + ((cursor_y - 238) / 48) * 48;
    for (int py = band_y - 4; py < band_y + 46; py++) {
        if (py < 210 || py >= 635) continue;
        for (int px = 84; px < 540; px++) {
            /* Degradado horizontal suave que se funde hacia la derecha */
            int fade = 90 - ((px - 84) * 90) / 456;
            if (fade > 0) {
                mk_blend(fb, pitch, px, py, 0x0A1828, fade);
            }
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

/* ============================================================================
 * 6. ILUSTRACIÓN DE CHIP 3D ISOMÉTRICO "EonSurf Core"
 * ============================================================================ */
static inline void eonsurf_draw_isometric_chip(uint32_t *fb, int pitch, int cx, int cy) {
    /* Cuerpo del chip en perspectiva isométrica con patas plateadas y núcleo cian */
    int w = 56, h = 32;

    /* Patas laterales plateadas */
    for (int i = -4; i <= 4; i++) {
        int px1 = cx - w - 8 + i * 4;
        int py1 = cy + i * 2;
        for (int k = 0; k < 6; k++) {
            mk_blend(fb, pitch, cx - 44 - k, cy - 10 + i * 5, 0x94A3B8, 240);
            mk_blend(fb, pitch, cx + 44 + k, cy - 10 + i * 5, 0x94A3B8, 240);
        }
    }

    /* Placa base oscura */
    for (int dy = -h; dy <= h; dy++) {
        int span = (int)((1.0f - fabsf((float)dy) / (float)h) * (float)w);
        for (int dx = -span; dx <= span; dx++) {
            int px = cx + dx;
            int py = cy + dy;
            /* Degradado de luz cenital */
            uint32_t col = (dy < 0) ? 0x1E2E40 : 0x0F1A26;
            mk_blend(fb, pitch, px, py, col, 255);
        }
    }

    /* Borde exterior biselado brillante */
    for (int dy = -h; dy <= h; dy++) {
        int span = (int)((1.0f - fabsf((float)dy) / (float)h) * (float)w);
        mk_blend(fb, pitch, cx - span, cy + dy, 0x38BDF8, 255);
        mk_blend(fb, pitch, cx + span, cy + dy, 0x38BDF8, 255);
    }

    /* Núcleo de silicio interior con resplandor neón cian */
    int cw = 28, ch = 16;
    for (int dy = -ch; dy <= ch; dy++) {
        int span = (int)((1.0f - fabsf((float)dy) / (float)ch) * (float)cw);
        for (int dx = -span; dx <= span; dx++) {
            int px = cx + dx;
            int py = cy + dy;
            mk_blend(fb, pitch, px, py, 0x00E5FF, 220);
        }
    }
    /* Logo espiral blanco en el centro */
    for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            if (abs(dx) + abs(dy) <= 3) {
                mk_blend(fb, pitch, cx + dx, cy + dy, 0xFFFFFF, 255);
            }
        }
    }
}

#endif /* EONSURF_MOCKUP_DETAILS_H */
