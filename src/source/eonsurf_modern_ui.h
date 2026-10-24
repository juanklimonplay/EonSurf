#ifndef EONSURF_MODERN_UI_H
#define EONSURF_MODERN_UI_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * 1. MEZCLADOR DE PÍXELES CON ALPHA BLENDING REAL (ANTI-ALIASING SUAVE)
 * ============================================================================ */

static inline uint32_t lerp_color(uint32_t c1, uint32_t c2, int t_0_to_256) {
    int inv = 256 - t_0_to_256;
    uint32_t r = (((c1 >> 16) & 0xFF) * inv + ((c2 >> 16) & 0xFF) * t_0_to_256) >> 8;
    uint32_t g = (((c1 >> 8)  & 0xFF) * inv + ((c2 >> 8)  & 0xFF) * t_0_to_256) >> 8;
    uint32_t b = ((c1         & 0xFF) * inv + (c2         & 0xFF) * t_0_to_256) >> 8;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

static inline void put_pixel_aa(uint32_t *fb, int pitch, int x, int y, uint32_t rgb, int alpha_0_255) {
    if (x < 0 || x >= 1280 || y < 0 || y >= 720 || alpha_0_255 <= 0) return;
    uint32_t *dst = &fb[y * pitch + x];
    if (alpha_0_255 >= 255) {
        *dst = 0xFF000000 | (rgb & 0xFFFFFF);
        return;
    }
    *dst = lerp_color(*dst, rgb, alpha_0_255);
}

/* ============================================================================
 * 2. FONDO DEGRADADO TITANIUM Y PANELES "DARK GLASS" (SDF SUB-PIXEL AA)
 * ============================================================================ */

/* Pinta el fondo general con degradado suave Azul Noche -> Gris Titanio */
static inline void eonsurf_draw_titanium_background(uint32_t *fb, int pitch) {
    for (int y = 0; y < 720; y++) {
        int t = (y * 256) / 720;
        uint32_t row_col = lerp_color(0xFF0A121C, 0xFF182634, t);
        uint32_t *row = &fb[y * pitch];
        for (int x = 0; x < 1280; x++) {
            row[x] = row_col;
        }
    }
}

/* 
 * Dibuja tarjetas redondeadas con:
 * - Degradado interno vertical (top_color -> bottom_color)
 * - Transparencia Dark Glass (opacity 0..255)
 * - Borde suavizado sin serrucho (Anti-Aliased Subpixel Border)
 */
static inline void eonsurf_draw_glass_panel(uint32_t *fb, int pitch, int x, int y, int w, int h,
                                            int radius, uint32_t top_col, uint32_t bot_col,
                                            uint32_t border_col, int opacity) {
    float r = (float)radius;
    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= 720) continue;
        int grad_t = ((py - y) * 256) / (h > 0 ? h : 1);
        uint32_t fill_col = lerp_color(top_col, bot_col, grad_t);

        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= 1280) continue;

            float dx = 0.0f, dy = 0.0f;
            if (px < x + radius) dx = (float)(x + radius - px);
            else if (px >= x + w - radius) dx = (float)(px - (x + w - radius - 1));

            if (py < y + radius) dy = (float)(y + radius - py);
            else if (py >= y + h - radius) dy = (float)(py - (y + h - radius - 1));

            if (dx > 0.0f || dy > 0.0f) {
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > r + 0.8f) continue;

                float edge_alpha = 1.0f;
                if (dist > r - 0.5f) {
                    edge_alpha = 1.0f - (dist - (r - 0.5f)) / 1.3f;
                    if (edge_alpha < 0.0f) edge_alpha = 0.0f;
                }

                if (dist >= r - 1.8f) {
                    put_pixel_aa(fb, pitch, px, py, border_col, (int)(edge_alpha * 255.0f));
                } else {
                    put_pixel_aa(fb, pitch, px, py, fill_col, opacity);
                }
            } else {
                int is_border = (px - x < 2) || (x + w - 1 - px < 2) || (py - y < 2) || (y + h - 1 - py < 2);
                if (is_border) {
                    put_pixel_aa(fb, pitch, px, py, border_col, 220);
                } else {
                    put_pixel_aa(fb, pitch, px, py, fill_col, opacity);
                }
            }
        }
    }
}

/* ============================================================================
 * 3. ICONOS VECTORIALES OFICIALES DEL DUALSHOCK 3 CON ANTI-ALIASING REAL
 * ============================================================================ */

static inline void eonsurf_draw_ps3_button_icon(uint32_t *fb, int pitch, int cx, int cy, char type) {
    const int r = 12;
    /* 1. Esfera circular con degradado e iluminación suave */
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            float d = sqrtf((float)(dx * dx + dy * dy));
            if (d <= r + 0.5f) {
                int a = (d > r - 0.5f) ? (int)((r + 0.5f - d) * 255.0f) : 255;
                uint32_t col = (d > r - 1.5f) ? 0xFF4A607A : 0xFF141E2B;
                put_pixel_aa(fb, pitch, cx + dx, cy + dy, col, a);
            }
        }
    }

    /* 2. Símbolo geométrico oficial PlayStation */
    if (type == 'O') { /* Círculo Rojo */
        for (int dy = -6; dy <= 6; dy++) {
            for (int dx = -6; dx <= 6; dx++) {
                float d = sqrtf((float)(dx * dx + dy * dy));
                if (d >= 4.2f && d <= 6.2f) put_pixel_aa(fb, pitch, cx + dx, cy + dy, 0xFFFF4444, 255);
            }
        }
    } else if (type == 'X') { /* Cruz Azul Celeste */
        for (int i = -4; i <= 4; i++) {
            for (int w = -1; w <= 1; w++) {
                put_pixel_aa(fb, pitch, cx + i + w, cy + i, 0xFF44AAFF, 255);
                put_pixel_aa(fb, pitch, cx + i + w, cy - i, 0xFF44AAFF, 255);
            }
        }
    } else if (type == 'S') { /* Cuadrado Rosa */
        for (int i = -4; i <= 4; i++) {
            put_pixel_aa(fb, pitch, cx + i, cy - 4, 0xFFFF66CC, 255);
            put_pixel_aa(fb, pitch, cx + i, cy + 4, 0xFFFF66CC, 255);
            put_pixel_aa(fb, pitch, cx - 4, cy + i, 0xFFFF66CC, 255);
            put_pixel_aa(fb, pitch, cx + 4, cy + i, 0xFFFF66CC, 255);
        }
    } else if (type == 'T') { /* Triángulo Verde */
        for (int y = -5; y <= 4; y++) {
            int half_w = ((y + 5) * 5) / 9;
            put_pixel_aa(fb, pitch, cx - half_w, cy + y, 0xFF00E676, 255);
            put_pixel_aa(fb, pitch, cx + half_w, cy + y, 0xFF00E676, 255);
            if (y == 4) {
                for (int x = -half_w; x <= half_w; x++) put_pixel_aa(fb, pitch, cx + x, cy + y, 0xFF00E676, 255);
            }
        }
    }
}

#endif /* EONSURF_MODERN_UI_H */
