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
 * 3. FONDO CON ONDAS XMB Y SPOTLIGHT RADIAL GRIS-PLATA
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

            /* Ondas de seda luminosas XMB */
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

            /* Panel Izquierdo con Spotlight Radial */
            if (x >= 40 && x <= 890 && y >= 120 && y <= 645) {
                float dx = (float)(x - 450) / 520.0f;
                float dy = (float)(y - 210) / 340.0f;
                float radial = 1.0f - sqrtf(dx * dx + dy * dy);
                if (radial < 0.0f) radial = 0.0f;

                float glass_alpha = 0.22f + radial * 0.68f;
                r = r * (1.0f - glass_alpha) + (195.0f * radial + 30.0f * (1.0f - radial)) * glass_alpha;
                g = g * (1.0f - glass_alpha) + (205.0f * radial + 45.0f * (1.0f - radial)) * glass_alpha;
                b = b * (1.0f - glass_alpha) + (215.0f * radial + 62.0f * (1.0f - radial)) * glass_alpha;

                if (y == 120 || y == 121) { r = 110.0f; g = 140.0f; b = 165.0f; }
            }

            if (r > 255.0f) r = 255.0f;
            if (g > 255.0f) g = 255.0f;
            if (b > 255.0f) b = 255.0f;
            g_mockup_bg_cache[y * 1280 + x] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
    g_mockup_bg_ready = 1;
}

/* ============================================================================
 * 4. PESTAÑAS CHROME, ICONOS TOOLBAR Y CURSOR NEÓN MOCKUP
 * ============================================================================ */

static inline void eonsurf_draw_chrome_tab(uint32_t *fb, int pitch, int x, int y, int w, int h,
                                           const char *title, int is_active, int is_wiki) {
    uint32_t fill_top = is_active ? 0x202D3C : 0x121A24;
    uint32_t fill_bot = is_active ? 0x151F2B : 0x0D141C;
    uint32_t glow_col = is_active ? 0x55E8FF : 0x2E4052;

    for (int py = y; py < y + h; py++) {
        float t = (float)(py - y) / (float)h;
        float curve = sinf((1.0f - t) * 1.5708f);
        int inset = (int)(curve * 10.0f);

        int left_x  = x + inset;
        int right_x = x + w - inset;

        for (int px = left_x - 2; px <= right_x + 2; px++) {
            if (px >= left_x && px <= right_x) {
                int is_top_border = (py <= y + 1);
                int is_side_border = (px <= left_x + 1 || px >= right_x - 1);
                if (is_top_border || is_side_border) {
                    mk_blend(fb, pitch, px, py, glow_col, is_active ? 255 : 140);
                } else {
                    uint32_t col = (py < y + h / 2) ? fill_top : fill_bot;
                    mk_blend(fb, pitch, px, py, col, 240);
                }
            } else if (is_active) {
                mk_blend(fb, pitch, px, py, 0x00D8FF, 95);
            }
        }
    }

    if (is_wiki) {
        for (int iy = y + 11; iy < y + 31; iy++) {
            for (int ix = x + 18; ix < x + 38; ix++) {
                mk_blend(fb, pitch, ix, iy, 0xF0F4F8, 255);
            }
        }
        eonsurf_draw_mockup_text(fb, pitch, x + 21, y + 13, 15, "W", 0x10151C, 0, x + 40);
    }

    eonsurf_draw_mockup_text(fb, pitch, x + 46, y + 12, 17, title,
                             is_active ? 0xFFFFFF : 0x90A4B8, 0, x + w - 34);
    eonsurf_draw_mockup_text(fb, pitch, x + w - 26, y + 12, 16, "x", 0x8899AA, 0, x + w - 8);
}

static inline void eonsurf_draw_toolbar_icons(uint32_t *fb, int pitch) {
    for (int i = -9; i <= 9; i++) {
        mk_blend(fb, pitch, 76 + i, 38, 0x35E8C8, 255);
        mk_blend(fb, pitch, 76 + i, 39, 0x35E8C8, 255);
    }
    for (int i = 0; i <= 7; i++) {
        mk_blend(fb, pitch, 67 + i, 38 - i, 0x35E8C8, 255);
        mk_blend(fb, pitch, 68 + i, 38 - i, 0x35E8C8, 255);
        mk_blend(fb, pitch, 67 + i, 38 + i, 0x35E8C8, 255);
        mk_blend(fb, pitch, 68 + i, 38 + i, 0x35E8C8, 255);
    }

    for (int i = -9; i <= 9; i++) {
        mk_blend(fb, pitch, 134 + i, 38, 0x6C8298, 255);
        mk_blend(fb, pitch, 134 + i, 39, 0x6C8298, 255);
    }
    for (int i = 0; i <= 7; i++) {
        mk_blend(fb, pitch, 143 - i, 38 - i, 0x6C8298, 255);
        mk_blend(fb, pitch, 142 - i, 38 - i, 0x6C8298, 255);
        mk_blend(fb, pitch, 143 - i, 38 + i, 0x6C8298, 255);
        mk_blend(fb, pitch, 142 - i, 38 + i, 0x6C8298, 255);
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

    /* Pestañas superiores estilo Chrome */
    eonsurf_draw_chrome_tab(fb, pitch, 48,  8, 240, 42, "EonSurf", 0, 0);
    /* 1. Dibuja el globo 🌐 en la Pestaña 1 ("EonSurf") */
    eonsurf_draw_globe_icon(fb, pitch, 72, 28);

    eonsurf_draw_chrome_tab(fb, pitch, 296, 8, 280, 42, "PlayStation 3 - Wikipedia", 1, 1);
    eonsurf_draw_mockup_text(fb, pitch, 588, 17, 22, "+", 0x90A4B8, 0, 620);

    /* 2. Une la pestaña activa con la línea horizontal brillante de punta a punta */
    eonsurf_draw_tab_baseline_glow(fb, pitch, 296, 280);

    /* Badge "Cell B.E. • 60 FPS" */
    eonsurf_draw_glowing_pill(fb, pitch, 1082, 11, 178, 34, 0x101C28, 0x00E5FF);
    eonsurf_draw_mockup_text(fb, pitch, 1098, 19, 16, "Cell B.E.  60 FPS", 0xE8F4FF, 0, 1255);
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
            mk_blend(fb, pitch, 1173 + dx, 27 + dy, 0x55F0FF, 255);

    /* Barra de herramientas */
    eonsurf_draw_toolbar_icons(fb, pitch);
    eonsurf_draw_glowing_pill(fb, pitch, 200, 58, 860, 42, 0x0E1824, 0x20D8FF);
    eonsurf_draw_https_padlock(fb, pitch, 218, 71);
    eonsurf_draw_mockup_text(fb, pitch, 242, 69, 18, current_url, 0xF0F6FC, 0, 1040);

    /* 3. Dibuja el emblema azul estilo PlayStation a la derecha de la barra de URL */
    eonsurf_draw_ps_emblem(fb, pitch, 1032, 79);

    /* Título H1 a 34px sobre el panel */
    eonsurf_draw_mockup_text(fb, pitch, 68, 126, 34, article_title, 0xFFFFFF, 1, 880);
    eonsurf_draw_mockup_text(fb, pitch, 72, 175, 14, "From Wikipedia, the free encyclopedia", 0x223040, 0, 600);

    /* Columna Derecha (30% Infobox) */
    eonsurf_draw_glass_panel(fb, pitch, 908, 120, 332, 525, 10, 0xFF1E2938, 0xFF111822, 0xFF0284C7, 240);
    eonsurf_draw_mockup_text(fb, pitch, 924, 130, 20, article_title, 0x00FFFFFF, 0, 1220);

    int img_box_y = 168;
    int img_box_h = 130;
    rect_rounded(922, img_box_y, 304, img_box_h, 6, 0x000b0f17);

    if (g_layout.infobox_rgba_tex && g_layout.img_w > 0 && g_layout.img_h > 0) {
        eonsurf_blit_infobox_image(fb, pitch, g_layout.infobox_rgba_tex, g_layout.img_w, g_layout.img_h,
                                   926, img_box_y + 4, 296, img_box_h - 8);
    } else {
        text_hd(990, img_box_y + 48, 1, "[ FOTO CONSOLA ]", 0x0038bdf8);
    }

    int table_start_y = img_box_y + img_box_h + 32;
    eonsurf_draw_dynamic_infobox_rows(908, table_start_y, 332, 120, 525);

    /* 4. Dibuja la franja oscura de foco detrás del párrafo donde apunta el cursor */
    eonsurf_draw_paragraph_focus_box(fb, pitch, cursor_y);

    /* Cuerpo del artículo con contorno negro (Stroke) y citas superíndice [1], [2] */
    int cur_x = 72, cur_y = 205;
    uint32_t cur_col = 0xF4F8FC;
    const char *p = article_body;
    char word[128];

    while (*p && cur_y < 630) {
        if (*p == '\x01') { cur_col = 0x62B8FF; p++; continue; }
        if (*p == '\x02') { cur_col = 0xF4F8FC; p++; continue; }
        if (*p == '\n')   { cur_x = 72; cur_y += 24; p++; continue; }
        if (*p == ' ')    { cur_x += 6; p++; continue; }

        int wi = 0;
        while (*p && *p != ' ' && *p != '\n' && *p != '\x01' && *p != '\x02' && wi < 120) {
            word[wi++] = *p++;
        }
        word[wi] = '\0';

        int est_w = wi * 9;
        if (cur_x + est_w > 875) {
            cur_x = 72;
            cur_y += 24;
            if (cur_y >= 630) break;
        }
        /* 5. Renderizador con soporte para citas elevadas [1] */
        cur_x = eonsurf_draw_word_with_citations(fb, pitch, cur_x, cur_y, word, cur_col, 880);
    }

    /* Dock Inferior Translúcido con los 5 controles oficiales */
    eonsurf_draw_glass_panel(fb, pitch, 315, 660, 650, 46, 23, 0x263240, 0x141C24, 0x4A6078, 215);

    eonsurf_draw_ps3_button_icon(fb, pitch, 348, 683, 'T');
    eonsurf_draw_mockup_text(fb, pitch, 368, 673, 16, "URL / Search", 0xE6EEF6, 1, 480);

    eonsurf_draw_ps3_button_icon(fb, pitch, 502, 683, 'X');
    eonsurf_draw_mockup_text(fb, pitch, 522, 673, 16, "Select", 0xE6EEF6, 1, 590);

    eonsurf_draw_ps3_button_icon(fb, pitch, 608, 683, 'O');
    eonsurf_draw_mockup_text(fb, pitch, 628, 673, 16, "Back", 0xE6EEF6, 1, 685);

    eonsurf_draw_ps3_button_icon(fb, pitch, 704, 683, 'S');
    eonsurf_draw_mockup_text(fb, pitch, 724, 673, 16, "Tabs", 0xE6EEF6, 1, 780);

    eonsurf_draw_ps3_button_icon(fb, pitch, 804, 683, 'O');
    text_hd(798, 675, 1, "R", 0x00ffffff);
    eonsurf_draw_mockup_text(fb, pitch, 824, 673, 16, "Scroll", 0xE6EEF6, 1, 940);

    /* Barra flotante de descarga OSD */
    eonsurf_draw_download_osd(fb, pitch);

    /* Cursor Neón grande con resplandor turquesa */
    eonsurf_draw_neon_cursor(fb, pitch, cursor_x, cursor_y);
}

#endif /* EONSURF_MOCKUP_ENGINE_H */
