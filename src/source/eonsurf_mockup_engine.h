#ifndef EONSURF_MOCKUP_ENGINE_H
#define EONSURF_MOCKUP_ENGINE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* stb_truetype.h already included by main.c */
#include "eonsurf_interactive_media.h"
#include "eonsurf_dynamic_web.h"
#include "eonsurf_modern_ui.h"
#include "eonsurf_mockup_details.h"
#include "eonsurf_tabs.h"
#include "eonsurf_cursor.h"
#include "eonsurf_article_extractor.h"
#include "eonsurf_block_engine.h"
#include "eonsurf_rich_blocks.h"

/* ============================================================================
 * 1. MEZCLADOR SUB-PÍXEL ARGB DE ALTA VELOCIDAD
 * ============================================================================ */

static inline void mk_blend(uint32_t *fb, int pitch, int x, int y, uint32_t rgb, int alpha) {
    if ((unsigned)x >= 1280 || (unsigned)y >= 720 || alpha <= 0) return;
    uint32_t *dst = &fb[y * pitch + x];
    if (alpha >= 255) {
        *dst = 0xFF000000 | (rgb & 0xFFFFFF);
        return;
    }
    uint32_t bg = *dst;
    int inv = 256 - alpha;
    uint32_t r = (((rgb >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * inv) >> 8;
    uint32_t g = (((rgb >> 8)  & 0xFF) * alpha + ((bg >> 8)  & 0xFF) * inv) >> 8;
    uint32_t b = ((rgb         & 0xFF) * alpha + (bg         & 0xFF) * inv) >> 8;
    *dst = 0xFF000000 | (r << 16) | (g << 8) | b;
}

/* ============================================================================
 * 2. MOTOR TRUETYPE (stb_truetype) CON CACHÉ EN RAM Y BORDE NEGRO (STROKE)
 * ============================================================================ */

static stbtt_fontinfo g_font;
static uint8_t       *g_font_buffer = NULL;
static int            g_ttf_loaded = 0;

/* Caché rápida de glifos para no recalcular curvas Bezier en cada frame (60 FPS) */
typedef struct {
    uint32_t codepoint;
    int      font_px;
    int      w, h, xoff, yoff, advance;
    uint8_t *bitmap;
} CachedGlyph;

#define GLYPH_CACHE_SIZE 256
static CachedGlyph g_glyph_cache[GLYPH_CACHE_SIZE];

static inline int eonsurf_init_mockup_font(void) {
    if (g_ttf_loaded) return 1;
    const char *paths[] = {
        "/dev_hdd0/game/EONS00001/USRDIR/Roboto-Regular.ttf",
        "/dev_hdd0/game/EONS00001/USRDIR/startpage/Roboto-Regular.ttf",
        "/dev_flash/data/font/SCE-PS3-VR-R-LATIN.TTF",
        "/dev_flash/data/font/SCE-PS3-SR-R-LATIN.TTF"
    };
    FILE *fp = NULL;
    for (int i = 0; i < 4; i++) {
        fp = fopen(paths[i], "rb");
        if (fp) break;
    }
    if (!fp) return 0;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    g_font_buffer = (uint8_t *)malloc(sz);
    if (!g_font_buffer) { fclose(fp); return 0; }
    fread(g_font_buffer, 1, sz, fp);
    fclose(fp);

    if (stbtt_InitFont(&g_font, g_font_buffer, stbtt_GetFontOffsetForIndex(g_font_buffer, 0))) {
        memset(g_glyph_cache, 0, sizeof(g_glyph_cache));
        g_ttf_loaded = 1;
        return 1;
    }
    return 0;
}

static inline CachedGlyph* get_cached_glyph(uint32_t cp, int font_px) {
    uint32_t idx = ((cp * 31) ^ (uint32_t)font_px) & (GLYPH_CACHE_SIZE - 1);
    CachedGlyph *cg = &g_glyph_cache[idx];
    if (cg->codepoint == cp && cg->font_px == font_px && cg->bitmap) {
        return cg;
    }
    if (cg->bitmap) {
        stbtt_FreeBitmap(cg->bitmap, NULL);
        cg->bitmap = NULL;
    }
    float scale = stbtt_ScaleForPixelHeight(&g_font, (float)font_px);
    int adv, lsb;
    stbtt_GetCodepointHMetrics(&g_font, cp, &adv, &lsb);
    cg->bitmap = stbtt_GetCodepointBitmap(&g_font, 0, scale, cp, &cg->w, &cg->h, &cg->xoff, &cg->yoff);
    cg->advance = (int)(adv * scale);
    cg->codepoint = cp;
    cg->font_px = font_px;
    return cg;
}

/* Decodificador UTF-8 a Unicode para tildes, Ñ y símbolos */
static inline uint32_t mk_utf8_next(const char **s) {
    const uint8_t *p = (const uint8_t *)(*s);
    if (!*p) return 0;
    if (*p < 0x80) { (*s)++; return *p; }
    if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        uint32_t cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
        (*s) += 2; return cp;
    }
    if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        uint32_t cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        (*s) += 3; return cp;
    }
    (*s)++; return '?';
}

/* 
 * Dibuja texto TrueType con o sin contorno negro (stroke = 1 activa el borde del mockup)
 */
static inline int eonsurf_draw_mockup_text(uint32_t *fb, int pitch, int x, int y, int font_px,
                                           const char *str, uint32_t color_rgb, int with_black_stroke, int max_x) {
    if (!g_ttf_loaded || !str) return x;
    int pen_x = x;
    int baseline = y + (int)(font_px * 0.82f);
    const char *p = str;

    while (*p) {
        uint32_t cp = mk_utf8_next(&p);
        if (!cp) break;
        if (cp == ' ') {
            pen_x += (font_px * 28) / 100;
            continue;
        }
        CachedGlyph *cg = get_cached_glyph(cp, font_px);
        if (!cg || !cg->bitmap) { pen_x += 8; continue; }
        if (pen_x + cg->advance >= max_x) break;

        int gx = pen_x + cg->xoff;
        int gy = baseline + cg->yoff;

        /* PASADA 1: Contorno negro suave de 1px (igual al texto sobre fondo gris del mockup) */
        if (with_black_stroke) {
            for (int r = 0; r < cg->h; r++) {
                for (int c = 0; c < cg->w; c++) {
                    int a = cg->bitmap[r * cg->w + c];
                    if (a > 45) {
                        int sa = (a * 190) >> 8;
                        mk_blend(fb, pitch, gx + c - 1, gy + r,     0x050A10, sa);
                        mk_blend(fb, pitch, gx + c + 1, gy + r,     0x050A10, sa);
                        mk_blend(fb, pitch, gx + c,     gy + r - 1, 0x050A10, sa);
                        mk_blend(fb, pitch, gx + c,     gy + r + 1, 0x050A10, sa);
                    }
                }
            }
        }

        /* PASADA 2: Cuerpo nítido de la letra con antialiasing de 256 niveles */
        for (int r = 0; r < cg->h; r++) {
            for (int c = 0; c < cg->w; c++) {
                int a = cg->bitmap[r * cg->w + c];
                if (a > 0) {
                    mk_blend(fb, pitch, gx + c, gy + r, color_rgb, a);
                }
            }
        }
        pen_x += cg->advance;
    }
    return pen_x;
}

/* ============================================================================
 * 3. FONDO CON ONDAS XMB Y SPOTLIGHT RADIAL DINAMICO
 * ============================================================================ */

static uint32_t g_mockup_bg_cache[1280 * 720] __attribute__((aligned(128)));
static int g_mockup_bg_ready = 0;

static inline void eonsurf_build_mockup_background(void) {
    if (g_mockup_bg_ready) return;

    for (int y = 0; y < 720; y++) {
        for (int x = 0; x < 1280; x++) {
            float ny = (float)y / 720.0f;
            float nx = (float)x / 1280.0f;
            float r = 7.0f  + ny * 10.0f;
            float g = 13.0f + ny * 18.0f;
            float b = 23.0f + ny * 28.0f;

            /* Ondas de seda luminosas XMB elegantes y fluidas */
            float wave1 = 440.0f + sinf(nx * 5.2f + 0.8f) * 38.0f + cosf(nx * 2.1f) * 18.0f;
            float dist1 = fabsf((float)y - wave1);
            if (dist1 < 65.0f) {
                float glow = (1.0f - (dist1 / 65.0f));
                glow = glow * glow * 0.65f;
                r += glow * 20.0f;
                g += glow * 135.0f;
                b += glow * 220.0f;
            }

            float wave2 = 475.0f + sinf(nx * 4.0f + 2.2f) * 45.0f;
            float dist2 = fabsf((float)y - wave2);
            if (dist2 < 45.0f) {
                float glow2 = (1.0f - (dist2 / 45.0f));
                glow2 = glow2 * glow2 * 0.45f;
                r += glow2 * 10.0f;
                g += glow2 * 95.0f;
                b += glow2 * 180.0f;
            }

            if (r > 255.0f) r = 255.0f;
            if (g > 255.0f) g = 255.0f;
            if (b > 255.0f) b = 255.0f;
            g_mockup_bg_cache[y * 1280 + x] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
    g_mockup_bg_ready = 1;
}

/* Dibuja el lienzo web real continuo (Viewport de fondo blanco auténtico #FFFFFF con borde gris sutil) */
static inline void eonsurf_draw_content_backdrop(uint32_t *fb, int pitch, int max_x, int max_y) {
    int start_x = 48;
    int end_x = max_x + 12;
    int start_y = 114;
    int end_y = max_y;

    for (int y = start_y; y <= end_y; y++) {
        if ((unsigned)y >= 720) continue;
        for (int x = start_x; x <= end_x; x++) {
            if ((unsigned)x >= 1280) continue;

            /* Borde perimetral gris claro #D0D7DE estilo ventana web de Chrome */
            int is_border = (y == start_y || y == end_y || x == start_x || x == end_x);
            if (is_border) {
                fb[y * pitch + x] = 0xFFD0D7DE;
            } else {
                /* Fondo blanco limpio continuo auténtico del documento web */
                fb[y * pitch + x] = 0xFFFFFFFF;
            }
        }
    }
}

/* ============================================================================
 * 4. PESTAÑAS CHROME, ICONOS TOOLBAR Y CURSOR NEÓN MOCKUP
 * ============================================================================ */

static inline void eonsurf_draw_chrome_tab(uint32_t *fb, int pitch, int x, int y, int w, int h,
                                           const char *title, int is_active, int is_wiki) {
    /* Pestanas modernas redondeadas ergonomicas tipo píldora */
    int r = 10; /* Radio de curvatura superior */
    uint32_t fill_top = is_active ? 0x1E293B : 0x0F172A;
    uint32_t fill_bot = is_active ? 0x131D2A : 0x0A0F18;
    uint32_t border_col = is_active ? 0x00E5FF : 0x2A3B50;

    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            /* Redondeo en esquinas superiores */
            if (py < y + r) {
                if (px < x + r) {
                    float dx = (float)(x + r - px);
                    float dy = (float)(y + r - py);
                    if (dx * dx + dy * dy > (float)(r * r)) continue;
                } else if (px >= x + w - r) {
                    float dx = (float)(px - (x + w - r - 1));
                    float dy = (float)(y + r - py);
                    if (dx * dx + dy * dy > (float)(r * r)) continue;
                }
            }

            int is_top = (py == y);
            int is_left = (px == x && py >= y + r);
            int is_right = (px == x + w - 1 && py >= y + r);
            int is_border = is_top || is_left || is_right;

            if (is_border) {
                mk_blend(fb, pitch, px, py, border_col, is_active ? 255 : 120);
            } else {
                float t = (float)(py - y) / (float)h;
                uint32_t col = (t < 0.5f) ? fill_top : fill_bot;
                mk_blend(fb, pitch, px, py, col, is_active ? 245 : 180);
            }
        }
    }

    if (is_wiki) {
        for (int iy = y + 11; iy < y + 31; iy++) {
            for (int ix = x + 10; ix < x + 30; ix++) {
                mk_blend(fb, pitch, ix, iy, 0xF0F4F8, 255);
            }
        }
        eonsurf_draw_mockup_text(fb, pitch, x + 13, y + 13, 15, "W", 0x10151C, 0, x + 32);
    } else {
        /* Icono de Globo terraqueo Sci-Fi en azul cian centrado */
        eonsurf_draw_globe_icon(fb, pitch, x + 20, y + 21);
    }

    /* Titulo de la pestana limpio sin numeracion y boton 'x' */
    eonsurf_draw_mockup_text(fb, pitch, x + 36, y + 12, 16, title,
                             is_active ? 0xFFFFFF : 0x94A3B8, 0, x + w - 28);
    eonsurf_draw_mockup_text(fb, pitch, x + w - 22, y + 12, 15, "x", 0x64748B, 0, x + w - 6);
}

static inline void eonsurf_draw_toolbar_icons(uint32_t *fb, int pitch) {
    /* Botón Atrás (←) Cian neón */
    for (int i = -9; i <= 9; i++) {
        mk_blend(fb, pitch, 42 + i, 78, 0x35E8C8, 255);
        mk_blend(fb, pitch, 42 + i, 79, 0x35E8C8, 255);
    }
    for (int i = 0; i <= 7; i++) {
        mk_blend(fb, pitch, 33 + i, 78 - i, 0x35E8C8, 255);
        mk_blend(fb, pitch, 34 + i, 78 - i, 0x35E8C8, 255);
        mk_blend(fb, pitch, 33 + i, 78 + i, 0x35E8C8, 255);
        mk_blend(fb, pitch, 34 + i, 78 + i, 0x35E8C8, 255);
    }

    /* Botón Adelante (→) Cian neón */
    for (int i = -9; i <= 9; i++) {
        mk_blend(fb, pitch, 92 + i, 78, 0x35E8C8, 255);
        mk_blend(fb, pitch, 92 + i, 79, 0x35E8C8, 255);
    }
    for (int i = 0; i <= 7; i++) {
        mk_blend(fb, pitch, 101 - i, 78 - i, 0x35E8C8, 255);
        mk_blend(fb, pitch, 100 - i, 78 - i, 0x35E8C8, 255);
        mk_blend(fb, pitch, 101 - i, 78 + i, 0x35E8C8, 255);
        mk_blend(fb, pitch, 100 - i, 78 + i, 0x35E8C8, 255);
    }

    /* Botón Recargar (↻) Flecha circular neón */
    int rcx = 142, rcy = 78, rr = 9;
    for (int dy = -rr; dy <= rr; dy++) {
        for (int dx = -rr; dx <= rr; dx++) {
            float dist = sqrtf((float)(dx * dx + dy * dy));
            if (dist >= rr - 1.8f && dist <= rr + 0.4f) {
                /* Abertura en el cuadrante superior derecho */
                if (!(dx > 1 && dy < -1)) {
                    mk_blend(fb, pitch, rcx + dx, rcy + dy, 0x35E8C8, 255);
                }
            }
        }
    }
    /* Punta de la flecha de recarga */
    for (int k = 0; k <= 5; k++) {
        mk_blend(fb, pitch, rcx + 5 - k, rcy - 10 + k, 0x35E8C8, 255);
        mk_blend(fb, pitch, rcx + 5 - k, rcy - 10 - k, 0x35E8C8, 255);
    }
}

static inline void eonsurf_draw_glowing_pill(uint32_t *fb, int pitch, int x, int y, int w, int h,
                                             uint32_t fill_col, uint32_t glow_col) {
    int radius = h / 2;
    int expand = 6;

    for (int py = y - expand; py < y + h + expand; py++) {
        if ((unsigned)py >= 720) continue;
        for (int px = x - expand; px < x + w + expand; px++) {
            if ((unsigned)px >= 1280) continue;

            float dx = 0.0f, dy = 0.0f;
            if (px < x + radius) dx = (float)((x + radius) - px);
            else if (px >= x + w - radius) dx = (float)(px - (x + w - radius - 1));

            if (py < y + radius) dy = (float)((y + radius) - py);
            else if (py >= y + h - radius) dy = (float)(py - (y + h - radius - 1));

            float dist = sqrtf(dx * dx + dy * dy);
            float r = (float)radius;

            if (dist <= r - 1.5f) {
                mk_blend(fb, pitch, px, py, fill_col, 235);
            } else if (dist <= r + 0.5f) {
                mk_blend(fb, pitch, px, py, 0x80F0FF, 255);
            } else if (dist <= r + expand) {
                float t = 1.0f - ((dist - r) / (float)expand);
                int glow_alpha = (int)(t * t * 145.0f);
                mk_blend(fb, pitch, px, py, glow_col, glow_alpha);
            }
        }
    }
}

static inline void eonsurf_draw_https_padlock(uint32_t *fb, int pitch, int x, int y) {
    for (int py = y + 5; py < y + 16; py++) {
        for (int px = x; px < x + 13; px++) {
            mk_blend(fb, pitch, px, py, 0x38E54D, 255);
        }
    }
    for (int py = y; py < y + 5; py++) {
        for (int px = x + 3; px < x + 10; px++) {
            if (py < y + 2 || px < x + 5 || px >= x + 8)
                mk_blend(fb, pitch, px, py, 0x38E54D, 255);
        }
    }
}

static inline void eonsurf_draw_neon_cursor(uint32_t *fb, int pitch, int cx, int cy) {
    for (int dy = 0; dy < 34; dy++) {
        for (int dx = 0; dx < 24; dx++) {
            if (dx <= (dy * 16) / 30 && (dy < 24 || dx < 10)) {
                int is_edge = (dx == 0 || dy == 0 || dx == (dy * 16) / 30 || (dy == 23 && dx >= 8));
                if (is_edge) {
                    mk_blend(fb, pitch, cx + dx, cy + dy, 0xCCFFFF, 255);
                    mk_blend(fb, pitch, cx + dx + 1, cy + dy, 0x00E5FF, 140);
                } else {
                    mk_blend(fb, pitch, cx + dx, cy + dy, 0x18C8B0, 165);
                }
            }
        }
    }
}

/* ============================================================================
 * 5. COMPOSITOR DE FRAME MOCKUP V2
 * ============================================================================ */

static inline void eonsurf_render_complete_mockup_frame(uint32_t *fb, int pitch,
                                                        const char *current_url,
                                                        const char *article_title,
                                                        const char *article_body,
                                                        int cursor_x, int cursor_y,
                                                        int scroll_y, int *out_max_scroll) {
    eonsurf_init_mockup_font();
    eonsurf_build_mockup_background();
    memcpy(fb, g_mockup_bg_cache, 1280 * 720 * sizeof(uint32_t));

    /* Pestañas superiores dinámicas escalables estilo Chrome (Hasta 12 pestañas) */
    int tab_w = eonsurf_get_dynamic_tab_width();
    int tab_x = 48;
    int active_tab_x = 48;
    for (int t = 0; t < MAX_TABS; t++) {
        if (!g_tabs[t].active) continue;
        int is_act = (t == g_current_tab);
        if (is_act) active_tab_x = tab_x;
        int is_wiki = (strstr(g_tabs[t].url, "wikipedia") != NULL);
        eonsurf_draw_chrome_tab(fb, pitch, tab_x, 8, tab_w, 42, g_tabs[t].title, is_act, is_wiki);

        tab_x += tab_w + 4;
    }
    /* Boton '+' para nueva pestana */
    eonsurf_draw_mockup_text(fb, pitch, tab_x + 6, 17, 24, "+", 0x38E8FF, 0, tab_x + 32);

    /* Linea horizontal neon continua que une la pestana activa */
    eonsurf_draw_tab_baseline_glow(fb, pitch, active_tab_x, tab_w);

    /* Barra de herramientas con ← → ↻ */
    eonsurf_draw_toolbar_icons(fb, pitch);

    /* Barra de URL superior (Pill neón en Y: 58-100, X: 182-965) */
    eonsurf_draw_glowing_pill(fb, pitch, 182, 58, 783, 42, 0x0E1824, 0x20D8FF);
    eonsurf_draw_https_padlock(fb, pitch, 202, 71);
    eonsurf_draw_mockup_text(fb, pitch, 228, 69, 18, current_url, 0xF0F6FC, 0, 955);

    /* Barra superior de progreso de red en tiempo real (Neón Cian) */
    extern float g_load_percent;
    extern int g_net_loading;
    if (g_net_loading && g_load_percent > 0.0f) {
        int bar_w = (int)((955 - 192) * (g_load_percent / 100.0f));
        if (bar_w > 763) bar_w = 763;
        if (bar_w < 6) bar_w = 6;
        for (int py = 98; py <= 101; py++) {
            for (int px = 192; px < 192 + bar_w; px++) {
                mk_blend(fb, pitch, px, py, 0x00E5FF, 230);
            }
        }
    }

    /* Badge Telemetria Cell B.E. + Medidor RAM XDR / GDDR3 y FPS Dinamico */
    /* Reubicado entre la barra de URL y el centro de control [ES] en X: 978 a 1188 */
    extern float g_measured_fps;
    extern int g_ram_used_mb;
    
    /* Semaforo de RAM: Verde (< 120MB), Amarillo (120-190MB), Rojo (> 190MB de 256MB) */
    uint32_t ram_color = 0x38BDF8;
    if (g_ram_used_mb < 120) ram_color = 0x22C55E;      /* Verde */
    else if (g_ram_used_mb < 190) ram_color = 0xFACC15; /* Amarillo */
    else ram_color = 0xEF4444;                          /* Rojo */

    char fps_label[48];
    snprintf(fps_label, sizeof(fps_label), "RAM %d/256MB  %.1fFPS",
             g_ram_used_mb, (g_measured_fps > 10.0f && g_measured_fps < 120.0f) ? g_measured_fps : 60.0f);
    eonsurf_draw_glowing_pill(fb, pitch, 978, 62, 210, 34, 0x101C28, ram_color);
    
    /* Punto LED indicador de estado de RAM ubicado limpiamente a la izquierda del texto */
    for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            if (dx * dx + dy * dy <= 9) {
                mk_blend(fb, pitch, 992 + dx, 79 + dy, ram_color, 255);
            }
        }
    }
    eonsurf_draw_mockup_text(fb, pitch, 1004, 70, 14, fps_label, 0xE8F4FF, 0, 1185);

    /* 3. Emblema Hexagonal Neón [ES] EonSurf a la derecha (Centro en X: 1222, Y: 79) */
    int is_es_hover = (cursor_x >= 1200 && cursor_x <= 1255 && cursor_y >= 58 && cursor_y <= 100);
    eonsurf_draw_hex_es_emblem(fb, pitch, 1225, 79, is_es_hover);

    /* Título H1 a 34px sobre el panel de lectura */
    /* Titulo principal renderizado como Hero Card en eonsurf_render_rich_web_blocks */

    extern int g_sidebar_visible;
    extern int g_dock_visible;
    /* La columna lateral solo se muestra si el usuario la tiene activa */
    int has_infobox = g_sidebar_visible && (g_layout.infobox_rgba_tex != NULL || strstr(current_url, "wikipedia") != NULL);
    int content_max_x = has_infobox ? 865 : 1220;
    int content_max_y = g_dock_visible ? 655 : 695;

    /* Dibuja el panel Dark Glass dinamico que se expande a 1220px si la barra lateral esta oculta */
    eonsurf_draw_content_backdrop(fb, pitch, content_max_x, content_max_y);

    if (has_infobox) {
        /* Columna Derecha: Ficha Técnica / Infobox auténtica de Wikipedia sobre fondo blanco */
        int ib_x = 888;
        int ib_y = 120;
        int ib_w = 340;
        int ib_h = content_max_y - 128;

        /* Recuadro de la ficha técnica con fondo #F8F9FA y borde perimetral gris #A2A9B1 */
        for (int py = ib_y; py < ib_y + ib_h; py++) {
            if ((unsigned)py >= 720) continue;
            for (int px = ib_x; px < ib_x + ib_w; px++) {
                if ((unsigned)px >= 1280) continue;
                int is_border = (py == ib_y || py == ib_y + ib_h - 1 || px == ib_x || px == ib_x + ib_w - 1);
                fb[py * pitch + px] = is_border ? 0xFFA2A9B1 : 0xFFF8F9FA;
            }
        }

        /* Título de la ficha técnica centrado con fondo lavanda claro de Wikipedia (#CEE0F2) */
        for (int py = ib_y + 1; py < ib_y + 34; py++) {
            for (int px = ib_x + 1; px < ib_x + ib_w - 1; px++) {
                fb[py * pitch + px] = 0xFFD8E4F0;
            }
        }
        eonsurf_draw_mockup_text(fb, pitch, ib_x + 14, ib_y + 8, 17, article_title, 0x10151C, 0, ib_x + ib_w - 14);

        int img_box_y = ib_y + 42;
        int img_box_h = 168;

        /* Área de imagen centrada sobre fondo blanco */
        for (int py = img_box_y; py < img_box_y + img_box_h; py++) {
            for (int px = ib_x + 14; px < ib_x + ib_w - 14; px++) {
                fb[py * pitch + px] = 0xFFFFFFFF;
            }
        }

        if (g_layout.infobox_rgba_tex) {
            eonsurf_blit_infobox_image(fb, pitch, g_layout.infobox_rgba_tex, g_layout.img_w, g_layout.img_h,
                                       ib_x + 18, img_box_y + 4, ib_w - 36, img_box_h - 8);
        } else {
            /* Chip procedural elegante si es página de inicio o aún no descargó foto */
            eonsurf_draw_isometric_chip(fb, pitch, ib_x + ib_w / 2, img_box_y + 84);
        }

        /* Tabla clave-valor de Wikipedia con filas alternadas y texto negro nítido */
        int base_y = img_box_y + img_box_h + 10;
        for (int i = 0; i < g_info_row_count && i < 6; i++) {
            int ry = base_y + (i * 38);
            if (ry + 36 >= ib_y + ib_h) break;

            /* Fondo de celda de clave (#EAECF0) */
            for (int fy = ry; fy < ry + 36; fy++) {
                for (int fx = ib_x + 6; fx < ib_x + 130; fx++) {
                    fb[fy * pitch + fx] = 0xFFEAECF0;
                }
                for (int fx = ib_x + 130; fx < ib_x + ib_w - 6; fx++) {
                    fb[fy * pitch + fx] = (i % 2 == 0) ? 0xFFFFFFFF : 0xFFF8F9FA;
                }
                fb[fy * pitch + (ib_x + 130)] = 0xFFA2A9B1; /* Línea divisoria vertical */
            }
            /* Línea divisoria horizontal inferior */
            for (int fx = ib_x + 6; fx < ib_x + ib_w - 6; fx++) {
                fb[(ry + 35) * pitch + fx] = 0xFFA2A9B1;
            }

            /* Clave (th) en negro negrita #101418 */
            eonsurf_draw_mockup_text(fb, pitch, ib_x + 10, ry + 10, 14, g_info_rows[i].key, 0x101418, 0, ib_x + 128);
            /* Valor (td) en gris oscuro #202124 */
            eonsurf_draw_mockup_text(fb, pitch, ib_x + 136, ry + 10, 14, g_info_rows[i].val, 0x202124, 0, ib_x + ib_w - 10);
        }
    }

    /* RENDERIZADO VISUAL ENRIQUECIDO POR BLOQUES WEB, TARJETAS Y HERO BANNERS */
    eonsurf_render_rich_web_blocks(fb, pitch, article_body, article_title, content_max_x,
                                  cursor_x, cursor_y, scroll_y, out_max_scroll);
    int max_s = (out_max_scroll) ? *out_max_scroll : 0;

    /* Barra de scroll visual proporcional */
    int scrollbar_track_h = 345;
    int scrollbar_y = 235;
    if (max_s > 0) {
        int thumb_h = (380 * scrollbar_track_h) / (max_s + 400);
        if (thumb_h < 24) thumb_h = 24;
        int thumb_y = scrollbar_y + (scroll_y * (scrollbar_track_h - thumb_h)) / max_s;
        /* Pista de scroll */
        for (int sy = scrollbar_y; sy < scrollbar_y + scrollbar_track_h; sy++) {
            for (int sx = 0; sx < 10; sx++) {
                mk_blend(fb, pitch, content_max_x + 8 + sx, sy, 0x1E2E40, 140);
            }
        }
        /* Deslizador (Thumb) Nen interactivo */
        for (int ty = thumb_y; ty < thumb_y + thumb_h; ty++) {
            for (int tx = 0; tx < 10; tx++) {
                mk_blend(fb, pitch, content_max_x + 8 + tx, ty, 0x38BDF8, 240);
            }
        }
    }

        /* Menú de Configuración OSD Flotante (al presionar START o clic en emblema ES) */
    extern int g_settings_menu_open;
    if (g_settings_menu_open) {
        eonsurf_draw_glass_panel(fb, pitch, 820, 105, 440, 440, 14, 0x0F172A, 0x020617, 0x00E5FF, 250);
        eonsurf_draw_mockup_text(fb, pitch, 845, 122, 20, "Centro de Control EonSurf", 0x38BDF8, 1, 1240);

        /* Opciones de Interfaz */
        eonsurf_draw_mockup_text(fb, pitch, 845, 155, 15, "Interfaz y Visualizacion:", 0xFFFFFF, 1, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 178, 14, g_sidebar_visible ? "[X] Barra Lateral: Visible" : "[ ] Barra Lateral: Oculta (Lienzo 100%)", g_sidebar_visible ? 0x22C55E : 0x94A3B8, 0, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 200, 14, g_dock_visible ? "[X] Dock Inferior: Activo" : "[ ] Dock Inferior: Oculto", g_dock_visible ? 0x22C55E : 0x94A3B8, 0, 1240);

        /* Acceso Rápido y Navegación */
        eonsurf_draw_mockup_text(fb, pitch, 845, 230, 15, "Accesos Rapidos y Herramientas:", 0xFFFFFF, 1, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 252, 14, "-> 1. Portal de Inicio (eonsurf://portal)", 0x38BDF8, 0, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 274, 14, "-> 2. Explorador de Disco (file:///dev_hdd0/)", 0x38BDF8, 0, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 296, 14, "-> 3. Guardar Pagina Actual en Marcadores", 0xFACC15, 0, 1240);

        /* Multimedia y Sonido */
        extern int g_music_playing;
        eonsurf_draw_mockup_text(fb, pitch, 845, 326, 15, "Musica y Ambiente:", 0xFFFFFF, 1, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 348, 14, g_music_playing ? "[>] Audio Ambiente: Reproduciendo" : "[ ] Audio Ambiente: En Pausa", g_music_playing ? 0x22C55E : 0x94A3B8, 0, 1240);

        /* Puntero */
        eonsurf_draw_mockup_text(fb, pitch, 845, 376, 15, "Estilo de Puntero:", 0xFFFFFF, 1, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 398, 14, (g_cursor_style == 0) ? "[*] Flecha Clasica OS" : (g_cursor_style == 1 ? "[*] Neon Cyan Precision" : "[*] Reticula Tactica"), 0x38BDF8, 0, 1240);

        /* Atajos Rápidos */
        eonsurf_draw_mockup_text(fb, pitch, 845, 435, 13, "L1/R1: Cambiar Pestanas | START / O: Cerrar Menu", 0x94A3B8, 0, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 845, 458, 12, "EonSurf v0.3.1 | Created by JuAnKLiMoN | 2006-2026", 0x38BDF8, 1, 1240);
    }

    extern int g_dock_visible;
    if (g_dock_visible) {
        /* Barra de navegación flotante ultra-compacta estilo Glass */
        eonsurf_draw_glass_panel(fb, pitch, 360, 672, 560, 38, 19, 0x1E293B, 0x0F172A, 0x38BDF8, 200);

        eonsurf_draw_ps3_button_icon(fb, pitch, 382, 691, 'T');
        eonsurf_draw_mockup_text(fb, pitch, 398, 683, 14, "Buscar", 0xE2E8F0, 1, 480);

        eonsurf_draw_ps3_button_icon(fb, pitch, 475, 691, 'X');
        eonsurf_draw_mockup_text(fb, pitch, 491, 683, 14, "Abrir", 0xE2E8F0, 1, 560);

        eonsurf_draw_ps3_button_icon(fb, pitch, 560, 691, 'O');
        eonsurf_draw_mockup_text(fb, pitch, 576, 683, 14, "Atras", 0xE2E8F0, 1, 650);

        eonsurf_draw_ps3_button_icon(fb, pitch, 645, 691, 'S');
        eonsurf_draw_mockup_text(fb, pitch, 661, 683, 14, "Menu", 0xE2E8F0, 1, 740);

        eonsurf_draw_mockup_text(fb, pitch, 735, 683, 13, "Cuadrado: Barra Lat.", 0x38BDF8, 0, 910);
    }

    /* Barra flotante de descarga OSD */
    eonsurf_draw_download_osd(fb, pitch);

    /* Puntero del ratón ergonómico con feedback de carga titilante */
    eonsurf_draw_cursor(fb, pitch, cursor_x, cursor_y);
}

#endif /* EONSURF_MOCKUP_ENGINE_H */
