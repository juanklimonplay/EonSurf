#ifndef EONSURF_CURSOR_H
#define EONSURF_CURSOR_H

#include <stdint.h>
#include <math.h>

/* Prototipo de mk_blend para llamadas tempranas */
static inline void mk_blend(uint32_t *fb, int pitch, int x, int y, uint32_t rgb, int alpha);

/* Modos de puntero */
typedef enum {
    CURSOR_STYLE_CLASSIC_ARROW = 0, /* Flecha clásica estilizada de sistema operativo (Recomendada) */
    CURSOR_STYLE_NEON_PRECISION,    /* Flecha estilizada moderna con resplandor cian */
    CURSOR_STYLE_CROSSHAIR          /* Cruz táctica de alta precisión */
} EonCursorStyle;

static EonCursorStyle g_cursor_style = CURSOR_STYLE_CLASSIC_ARROW;

/* Variable global que indica si la consola está esperando respuesta de internet */
static int g_net_loading = 0;
static int g_cursor_pulse_frame = 0;

/*
 * Dibuja el puntero del ratón con soporte de estilos y animación de carga titilante
 */
/* Modos dinamicos segun elemento apuntado (PS3 Ergonomics) */
typedef enum {
    CURSOR_MODE_NORMAL = 0,
    CURSOR_MODE_HAND_LINK,
    CURSOR_MODE_IBEAM_TEXT
} EonCursorDynamicMode;

static EonCursorDynamicMode g_cursor_dynamic_mode = CURSOR_MODE_NORMAL;

static inline void eonsurf_draw_cursor(uint32_t *fb, int pitch, int cx, int cy) {
    g_cursor_pulse_frame = (g_cursor_pulse_frame + 1) % 60;

    /* Factor de respiración/titileo mientras la web carga */
    int blink_alpha = 255;
    if (g_net_loading) {
        float s = sinf((float)g_cursor_pulse_frame * 0.20f);
        blink_alpha = (int)(140.0f + s * 115.0f);
        if (blink_alpha < 40) blink_alpha = 40;
        if (blink_alpha > 255) blink_alpha = 255;
    }

        /* MODO 1: MANITO / DEDO INDICE (Hover sobre enlaces o pestanas) */
    if (g_cursor_dynamic_mode == CURSOR_MODE_HAND_LINK) {
        static const uint16_t hand_mask[20] = {
            0b0001100000000000,
            0b0001100000000000,
            0b0001100000000000,
            0b0001100000000000,
            0b0001101100000000,
            0b0001111111000000,
            0b0111111111100000,
            0b1111111111100000,
            0b1111111111100000,
            0b1111111111100000,
            0b1111111111100000,
            0b0111111111100000,
            0b0011111111000000,
            0b0011111111000000,
            0b0011111111000000,
            0b0011111111000000,
            0b0001111110000000,
            0b0001111110000000,
            0b0000000000000000,
            0b0000000000000000
        };

        for (int r = 0; r < 20; r++) {
            for (int c = 0; c < 16; c++) {
                if (hand_mask[r] & (1 << (15 - c))) {
                    mk_blend(fb, pitch, cx + c + 2, cy + r + 2, 0x05080E, 90);
                }
            }
        }
        for (int r = 0; r < 20; r++) {
            for (int c = 0; c < 16; c++) {
                if (hand_mask[r] & (1 << (15 - c))) {
                    int edge = (r == 0 || r == 17 || c == 0 || c == 15);
                    if (edge) {
                        mk_blend(fb, pitch, cx + c, cy + r, 0x032838, 255);
                    } else {
                        mk_blend(fb, pitch, cx + c, cy + r, 0x38E8FF, blink_alpha);
                    }
                }
            }
        }
        return;
    }

    /* MODO 2: I-BEAM / CURSOR DE TEXTO (Hover sobre barra de URL) */
    if (g_cursor_dynamic_mode == CURSOR_MODE_IBEAM_TEXT) {
        int h = 18;
        for (int x = -4; x <= 4; x++) {
            mk_blend(fb, pitch, cx + x, cy - h/2, 0x00FFFF, blink_alpha);
            mk_blend(fb, pitch, cx + x, cy + h/2, 0x00FFFF, blink_alpha);
        }
        for (int y = -h/2; y <= h/2; y++) {
            mk_blend(fb, pitch, cx, cy + y, 0xFFFFFF, blink_alpha);
            mk_blend(fb, pitch, cx + 1, cy + y, 0x00E5FF, blink_alpha);
        }
        return;
    }

    if (g_cursor_style == CURSOR_STYLE_CLASSIC_ARROW) {
        /*
         * OPCIÓN 1: FLECHA CLÁSICA DE SISTEMA (Blanca brillante con sombra sutil y contorno oscuro)
         * Tamaño ergonómico optimizado (18x24 píxeles) para no tapar el texto de lectura.
         */
        static const uint16_t arrow_mask[22] = {
            0b1000000000000000,
            0b1100000000000000,
            0b1110000000000000,
            0b1111000000000000,
            0b1111100000000000,
            0b1111110000000000,
            0b1111111000000000,
            0b1111111100000000,
            0b1111111110000000,
            0b1111111111000000,
            0b1111111111100000,
            0b1111111111110000,
            0b1111111111111000,
            0b1111111000000000,
            0b1110111100000000,
            0b1100111100000000,
            0b1000011110000000,
            0b0000011110000000,
            0b0000001111000000,
            0b0000001111000000,
            0b0000000111000000,
            0b0000000010000000
        };

        static const uint16_t border_mask[22] = {
            0b1000000000000000,
            0b1100000000000000,
            0b1010000000000000,
            0b1001000000000000,
            0b1000100000000000,
            0b1000010000000000,
            0b1000001000000000,
            0b1000000100000000,
            0b1000000010000000,
            0b1000000001000000,
            0b1000000000100000,
            0b1000000000010000,
            0b1000001111111000,
            0b1000101000000000,
            0b1010100100000000,
            0b1100010010000000,
            0b1000010010000000,
            0b0000001001000000,
            0b0000001001000000,
            0b0000000100100000,
            0b0000000111100000,
            0b0000000010000000
        };

        /* Pasada 1: Sombra arrojada difusa (desplazada +2, +2) */
        for (int r = 0; r < 22; r++) {
            for (int c = 0; c < 16; c++) {
                if (arrow_mask[r] & (1 << (15 - c))) {
                    mk_blend(fb, pitch, cx + c + 2, cy + r + 2, 0x05080E, 90);
                }
            }
        }

        /* Pasada 2: Relleno y borde */
        for (int r = 0; r < 22; r++) {
            for (int c = 0; c < 16; c++) {
                if (arrow_mask[r] & (1 << (15 - c))) {
                    int is_border = (border_mask[r] & (1 << (15 - c)));
                    if (is_border) {
                        /* Borde negro carbón para contraste absoluto */
                        mk_blend(fb, pitch, cx + c, cy + r, 0x0C121A, 255);
                    } else {
                        /* Relleno blanco marfil; si carga red, titila en cian luminoso */
                        uint32_t col = g_net_loading ? 0x48D8FF : 0xFFFFFF;
                        mk_blend(fb, pitch, cx + c, cy + r, col, blink_alpha);
                    }
                }
            }
        }

        /* Indicador orbital de carga en tiempo real (Anillo giratorio Sci-Fi) */
        if (g_net_loading) {
            float base_ang = (float)g_cursor_pulse_frame * 0.22f;
            int center_x = cx + 24;
            int center_y = cy + 16;
            for (int p = 0; p < 6; p++) {
                float a = base_ang + (float)p * (3.14159f / 3.0f);
                int sx = center_x + (int)(cosf(a) * 9.0f);
                int sy = center_y + (int)(sinf(a) * 9.0f);
                int dot_alpha = 100 + p * 28;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        mk_blend(fb, pitch, sx + dx, sy + dy, 0x00E5FF, dot_alpha);
                    }
                }
            }
        }
    }
    else if (g_cursor_style == CURSOR_STYLE_NEON_PRECISION) {
        /* OPCIÓN 2: FLECHA NEÓN COMPACTA */
        for (int dy = 0; dy < 20; dy++) {
            for (int dx = 0; dx < 14; dx++) {
                if (dx <= (dy * 10) / 16 && (dy < 14 || dx < 6)) {
                    int is_edge = (dx == 0 || dy == 0 || dx == (dy * 10) / 16 || (dy == 13 && dx >= 4));
                    if (is_edge) {
                        mk_blend(fb, pitch, cx + dx, cy + dy, 0xCCFFFF, blink_alpha);
                    } else {
                        mk_blend(fb, pitch, cx + dx, cy + dy, 0x00C8B0, (blink_alpha * 190) >> 8);
                    }
                }
            }
        }
    }
    else {
        /* OPCIÓN 3: CRUZ DE PRECISIÓN */
        for (int i = -7; i <= 7; i++) {
            if (abs(i) > 2) {
                mk_blend(fb, pitch, cx + i, cy, 0xFFFFFF, blink_alpha);
                mk_blend(fb, pitch, cx, cy + i, 0xFFFFFF, blink_alpha);
            }
        }
        mk_blend(fb, pitch, cx, cy, 0x00E5FF, 255);
    }
}

#endif /* EONSURF_CURSOR_H */
