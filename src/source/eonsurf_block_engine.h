#ifndef EONSURF_BLOCK_ENGINE_H
#define EONSURF_BLOCK_ENGINE_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "eonsurf_md_clean.h"

/* Prototipo de mk_blend y text render para integración */
static inline void mk_blend(uint32_t *fb, int pitch, int x, int y, uint32_t rgb, int alpha);
static inline int eonsurf_draw_mockup_text(uint32_t *fb, int pitch, int x, int y, int font_px,
                                           const char *str, uint32_t color_rgb, int with_black_stroke, int max_x);

/* Tipos de bloques web enriquecidos */
typedef enum {
    BLOCK_HERO_HEADER = 0, /* Banner H1 principal con insignia y fondo elegante */
    BLOCK_SECTION_H2,     /* Encabezado H2 de sección con tarjeta y barra de acento cian */
    BLOCK_SUBSECTION_H3,  /* Encabezado H3 secundario */
    BLOCK_CARD_ARTICLE,   /* Párrafo dentro de tarjeta Dark Glass con relieve */
    BLOCK_CODE_BOX,       /* Bloque de código con fondo carbón, borde neón y badge CODE */
    BLOCK_QUOTE_CALLOUT,  /* Cita destacada con barra vertical dorada */
    BLOCK_NAV_BUTTONS,    /* Fila/Tarjeta de navegación con botones interactivos */
    BLOCK_HR_DIVIDER      /* Separador horizontal degradado */
} EonBlockType;

#define MAX_WEB_BLOCKS 64

typedef struct {
    EonBlockType type;
    char text[1024];
    char link_url[256];
    uint32_t bg_color;
    uint32_t border_color;
    uint32_t text_color;
    int is_button;
    int height;
} EonWebBlock;

static EonWebBlock g_web_blocks[MAX_WEB_BLOCKS];
static int g_web_block_count = 0;
static char g_last_parsed_source_head[64] = "";

/*
 * Parser estructurado que convierte Markdown o HTML extraído en Bloques Web Estilizados
 */
static inline void eonsurf_parse_content_into_blocks(const char *source, const char *page_title) {
    if (!source || strlen(source) == 0) {
        g_web_block_count = 0;
        return;
    }

    /* Cache rápida si la fuente no cambió */
    if (strncmp(g_last_parsed_source_head, source, 63) == 0 && g_web_block_count > 0) {
        return;
    }
    strncpy(g_last_parsed_source_head, source, 63);
    g_last_parsed_source_head[63] = '\0';
    g_web_block_count = 0;

    /* 1. Bloque Hero Header inicial con el título de la página */
    if (g_web_block_count < MAX_WEB_BLOCKS && page_title && strlen(page_title) > 0) {
        EonWebBlock *b = &g_web_blocks[g_web_block_count++];
        b->type = BLOCK_HERO_HEADER;
        snprintf(b->text, sizeof(b->text), "%s", page_title);
        b->bg_color = 0x1A283C;
        b->border_color = 0x38BDF8;
        b->text_color = 0xFFFFFF;
    }

    const char *p = source;
    char line[1024];

    while (*p && g_web_block_count < MAX_WEB_BLOCKS) {
        /* Leer una línea o bloque de texto */
        size_t li = 0;
        while (*p && *p != '\n' && li + 1 < sizeof(line)) {
            line[li++] = *p++;
        }
        line[li] = '\0';
        if (*p == '\n') p++;

        /* Limpieza de espacios en extremos */
        while (li > 0 && (line[li - 1] == ' ' || line[li - 1] == '\r')) line[--li] = '\0';
        if (li == 0) continue;

        EonWebBlock *b = &g_web_blocks[g_web_block_count++];
        memset(b, 0, sizeof(EonWebBlock));

        /* Detectar tipo de bloque según sintaxis */
        if (strncmp(line, "### ", 4) == 0) {
            b->type = BLOCK_SUBSECTION_H3;
            snprintf(b->text, sizeof(b->text), "%s", line + 4);
            b->text_color = 0x38BDF8;
            b->border_color = 0x0284C7;
            b->bg_color = 0x121A26;
        } else if (strncmp(line, "## ", 3) == 0) {
            b->type = BLOCK_SECTION_H2;
            snprintf(b->text, sizeof(b->text), "%s", line + 3);
            b->text_color = 0xFFFFFF;
            b->border_color = 0x00E5FF;
            b->bg_color = 0x182434;
        } else if (strncmp(line, "# ", 2) == 0) {
            b->type = BLOCK_HERO_HEADER;
            snprintf(b->text, sizeof(b->text), "%s", line + 2);
            b->text_color = 0xFFFFFF;
            b->border_color = 0x38BDF8;
            b->bg_color = 0x1E293B;
        } else if (strncmp(line, "[CODE]", 6) == 0 || strncmp(line, "```", 3) == 0) {
            b->type = BLOCK_CODE_BOX;
            const char *code_content = (line[0] == '[') ? line + 6 : line + 3;
            while (*code_content == ' ') code_content++;
            snprintf(b->text, sizeof(b->text), "%s", strlen(code_content) > 0 ? code_content : "SOURCE CODE");
            b->bg_color = 0x0A1018;
            b->border_color = 0x22C55E;
            b->text_color = 0x86EFAC;
        } else if (line[0] == '>' || line[0] == '|') {
            b->type = BLOCK_QUOTE_CALLOUT;
            const char *quote = line + 1;
            while (*quote == ' ') quote++;
            snprintf(b->text, sizeof(b->text), "%s", quote);
            b->bg_color = 0x131A24;
            b->border_color = 0xFACC15;
            b->text_color = 0xFEF08A;
        } else if (strncmp(line, "---", 3) == 0 || strncmp(line, "===", 3) == 0) {
            b->type = BLOCK_HR_DIVIDER;
            b->border_color = 0x38BDF8;
        } else if (line[0] == '*' || (line[0] == '-' && line[1] == ' ')) {
            /* Elemento de lista interactivo / Botón de navegación */
            b->type = BLOCK_NAV_BUTTONS;
            const char *item = (line[0] == '*') ? line + 1 : line + 2;
            while (*item == ' ') item++;
            snprintf(b->text, sizeof(b->text), "%s", item);
            b->bg_color = 0x14202E;
            b->border_color = 0x38BDF8;
            b->text_color = 0x7DD3FC;
            b->is_button = 1;
        } else {
            /* Párrafo en tarjeta con diseño editorial */
            b->type = BLOCK_CARD_ARTICLE;
            snprintf(b->text, sizeof(b->text), "%s", line);
            b->bg_color = 0x0E1622;
            b->border_color = 0x1E293B;
            b->text_color = 0xE2E8F0;
        }
    }
}

/*
 * Dibuja un rectángulo con esquinas suaves y fondo translúcido para tarjetas web
 */
static inline void eonsurf_draw_web_card_box(uint32_t *fb, int pitch, int x, int y, int w, int h,
                                             uint32_t bg_col, uint32_t border_col, int border_thick,
                                             int is_hover, int r) {
    if (y + h < 0 || y >= 720 || x + w < 0 || x >= 1280) return;
    int end_y = y + h;
    int end_x = x + w;

    for (int py = y; py < end_y; py++) {
        if ((unsigned)py >= 720) continue;
        for (int px = x; px < end_x; px++) {
            if ((unsigned)px >= 1280) continue;

            /* Chequear esquinas redondeadas */
            if (py < y + r) {
                if (px < x + r) {
                    float dx = (float)(x + r - px);
                    float dy = (float)(y + r - py);
                    if (dx * dx + dy * dy > (float)(r * r)) continue;
                } else if (px >= end_x - r) {
                    float dx = (float)(px - (end_x - r - 1));
                    float dy = (float)(y + r - py);
                    if (dx * dx + dy * dy > (float)(r * r)) continue;
                }
            } else if (py >= end_y - r) {
                if (px < x + r) {
                    float dx = (float)(x + r - px);
                    float dy = (float)(py - (end_y - r - 1));
                    if (dx * dx + dy * dy > (float)(r * r)) continue;
                } else if (px >= end_x - r) {
                    float dx = (float)(px - (end_x - r - 1));
                    float dy = (float)(py - (end_y - r - 1));
                    if (dx * dx + dy * dy > (float)(r * r)) continue;
                }
            }

            int is_border = (py < y + border_thick || py >= end_y - border_thick ||
                             px < x + border_thick || px >= end_x - border_thick);

            if (is_border) {
                mk_blend(fb, pitch, px, py, border_col, is_hover ? 255 : 180);
            } else {
                mk_blend(fb, pitch, px, py, bg_col, is_hover ? 230 : 190);
            }
        }
    }
}

#endif /* EONSURF_BLOCK_ENGINE_H */
