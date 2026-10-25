#ifndef EONSURF_PS3_ERGONOMICS_H
#define EONSURF_PS3_ERGONOMICS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <io/pad.h>

#include "eonsurf_md_clean.h"

/* ============================================================================
 * 1. PURIFICADOR QUIRURGICO DE WIKIPEDIA (ELIMINA PLANTILLAS, JSON Y ARCHIVOS)
 * ============================================================================ */

static inline void eonsurf_strip_wiki_garbage(const char *src, char *dst, size_t max_len) {
    if (!src || !dst || max_len == 0) return;

    /* 1. Si quedo el resto de un atributo HTML al inicio (ej. id="mw-content-text"...>), saltarlo */
    const char *start = src;
    const char *first_gt = strchr(src, '>');
    if (first_gt && (first_gt - src) < 140 && (strstr(src, "mw-content") || strstr(src, "mw-parser"))) {
        start = first_gt + 1;
    }

    /* 2. Si detectamos fragmentos residuales de JSON MediaWiki (data-mw, wt, params, etc.), saltar hasta después del corchete/llave final */
    if (strstr(start, "\"parts\":[") || strstr(start, "{\"target\"") || strstr(start, "\"wt\":")) {
        const char *end_json = strstr(start, "]}]");
        if (!end_json) end_json = strstr(start, "}}");
        if (end_json) {
            start = end_json + 2;
            while (*start == '}' || *start == ']' || *start == '"' || *start == '>' || *start == '\'' || *start == ' ') start++;
        }
    }

    size_t di = 0;
    int brace_depth = 0; /* Cuenta llaves {{ ... }} */
    int file_depth  = 0; /* Cuenta [[Archivo:...]] o [[File:...]] */

    for (size_t i = 0; start[i] != '\0' && di + 2 < max_len; i++) {
        /* Saltar plantillas {{ ... }} */
        if (start[i] == '{' && start[i + 1] == '{') {
            brace_depth++; i++; continue;
        }
        if (start[i] == '}' && start[i + 1] == '}') {
            if (brace_depth > 0) brace_depth--;
            i++; continue;
        }
        if (brace_depth > 0) continue;

        /* Saltar imagenes incrustadas [[Archivo:...]] o [[File:...]] */
        if (start[i] == '[' && start[i + 1] == '[') {
            if (strncasecmp(&start[i + 2], "Archivo:", 8) == 0 ||
                strncasecmp(&start[i + 2], "File:", 5) == 0 ||
                strncasecmp(&start[i + 2], "Imagen:", 7) == 0) {
                file_depth = 1;
                i++; continue;
            }
        }
        if (file_depth > 0) {
            if (start[i] == '[' && start[i + 1] == '[') { file_depth++; i++; continue; }
            if (start[i] == ']' && start[i + 1] == ']') { file_depth--; i++; continue; }
            continue;
        }

        /* Convertir enlaces Wiki [[Destino|Texto Visible]] -> [Texto Visible](Destino) */
        if (start[i] == '[' && start[i + 1] == '[') {
            const char *close = strstr(&start[i + 2], "]]");
            if (close && (close - &start[i]) < 180) {
                const char *pipe = memchr(&start[i + 2], '|', (size_t)(close - &start[i + 2]));
                const char *target = &start[i + 2];
                size_t tlen = pipe ? (size_t)(pipe - target) : (size_t)(close - target);
                const char *label = pipe ? (pipe + 1) : target;
                size_t llen = (size_t)(close - label);

                if (di + llen + tlen + 40 < max_len) {
                    dst[di++] = '[';
                    for (size_t k = 0; k < llen; k++) dst[di++] = label[k];
                    dst[di++] = ']';
                    dst[di++] = '(';
                    if (strncmp(target, "http", 4) != 0) {
                        const char *prefix = "https://es.wikipedia.org/wiki/";
                        while (*prefix) dst[di++] = *prefix++;
                    }
                    for (size_t k = 0; k < tlen; k++) {
                        char c = target[k];
                        dst[di++] = (c == ' ') ? '_' : c;
                    }
                    dst[di++] = ')';
                }
                i = (size_t)((close + 1) - start);
                continue;
            }
        }

        /* Limpiar entidades HTML escapadas (&lt;ref...&gt; o &nbsp;) */
        if (strncmp(&start[i], "&lt;", 4) == 0) {
            const char *gt = strstr(&start[i + 4], "&gt;");
            if (gt && (gt - &start[i]) < 200) {
                i = (size_t)((gt + 3) - start);
                continue;
            }
        }
        if (strncmp(&start[i], "&nbsp;", 6) == 0) {
            dst[di++] = ' ';
            i += 5;
            continue;
        }

        /* Ignorar fragmentos JSON residuales como {"wt":"..."} */
        if (strncmp(&start[i], "{\"wt\":", 6) == 0) {
            i += 5;
            continue;
        }

        dst[di++] = start[i];
    }
    dst[di] = '\0';
}

/* ============================================================================
 * 2. FISICA DE PUNTERO DUALSHOCK 3 + VIBRACION HAPTICA + IMAN DE CRUCETA
 * ============================================================================ */

typedef enum {
    CURSOR_ARROW = 0,
    CURSOR_HAND_LINK = 1,
    CURSOR_IBEAM_URL = 2
} EonCursorMode;

static int g_rumble_timer = 0;
static int g_last_hovered_link = -1;
static int g_dpad_link_idx = -1;
static int g_smart_zoom_level = 0; /* 0 = 18px, 1 = 24px, 2 = 30px */

/* Dispara un micro-pulso de 50ms en el motor chico del DualShock 3 */
static inline void eonsurf_trigger_haptic_tick(int pad_port) {
    padActParam act;
    memset(&act, 0, sizeof(act));
    act.small_motor = 1;
    act.large_motor = 0;
    ioPadSetActDirect(pad_port, &act);
    g_rumble_timer = 3;  /* Dura 3 frames a 60 FPS (50 ms) */
}

static inline void eonsurf_update_haptic_frame(int pad_port) {
    if (g_rumble_timer > 0) {
        g_rumble_timer--;
        if (g_rumble_timer == 0) {
            padActParam act;
            memset(&act, 0, sizeof(act));
            ioPadSetActDirect(pad_port, &act);
        }
    }
}

/*
 * Actualiza el cursor con aceleracion exponencial, friccion magnetica sobre enlaces,
 * salto automatico con Cruceta (D-Pad) y Zoom Inteligente con L3.
 */
static inline EonCursorMode eonsurf_update_ps3_ergonomics(int *cursor_x, int *cursor_y,
                                                          int stick_lx, int stick_ly,
                                                          int dpad_up_pressed, int dpad_down_pressed,
                                                          int l3_pressed, int pad_port,
                                                          const EonCleanLink *links, int link_count) {
    eonsurf_update_haptic_frame(pad_port);

    /* 1. Boton L3: Alternar Smart Zoom de sillon (18px -> 24px -> 30px -> 18px) */
    if (l3_pressed) {
        g_smart_zoom_level = (g_smart_zoom_level + 1) % 3;
        eonsurf_trigger_haptic_tick(pad_port);
    }

    /* 2. Cruceta Arriba / Abajo: Saltar magneticamente de enlace en enlace (Link Snap) */
    if ((dpad_down_pressed || dpad_up_pressed) && link_count > 0 && links) {
        for (int step = 0; step < link_count; step++) {
            if (dpad_down_pressed)
                g_dpad_link_idx = (g_dpad_link_idx + 1) % link_count;
            else
                g_dpad_link_idx = (g_dpad_link_idx - 1 + link_count) % link_count;

            if (links[g_dpad_link_idx].visible) {
                *cursor_x = links[g_dpad_link_idx].x + (links[g_dpad_link_idx].w / 2);
                *cursor_y = links[g_dpad_link_idx].y + (links[g_dpad_link_idx].h / 2);
                eonsurf_trigger_haptic_tick(pad_port);
                break;
            }
        }
    }

    /* 3. Detectar que hay debajo del cursor ahora mismo */
    int hovered_link = -1;
    if (links) {
        for (int i = 0; i < link_count; i++) {
            if (!links[i].visible) continue;
            if (*cursor_x >= links[i].x && *cursor_x <= (links[i].x + links[i].w) &&
                *cursor_y >= links[i].y && *cursor_y <= (links[i].y + links[i].h)) {
                hovered_link = i;
                break;
            }
        }
    }

    /* Si acaba de entrar a un enlace nuevo, vibrar suavemente el joystick */
    if (hovered_link != -1 && hovered_link != g_last_hovered_link) {
        eonsurf_trigger_haptic_tick(pad_port);
    }
    g_last_hovered_link = hovered_link;

    /* 4. Curva de aceleracion analogica + Friccion Magnetica al pisar un link */
    float nx = (float)(stick_lx - 128) / 128.0f;
    float ny = (float)(stick_ly - 128) / 128.0f;
    if (fabsf(nx) > 0.10f || fabsf(ny) > 0.10f) {
        /* Si esta sobre un link, frena la velocidad al 45% (efecto iman de NetFront) */
        float speed = (hovered_link != -1) ? 5.2f : 12.0f;
        *cursor_x += (int)(nx * fabsf(nx) * speed);
        *cursor_y += (int)(ny * fabsf(ny) * speed);
        if (*cursor_x < 8) *cursor_x = 8;
        if (*cursor_x > 1270) *cursor_x = 1270;
        if (*cursor_y < 8) *cursor_y = 8;
        if (*cursor_y > 710) *cursor_y = 710;
    }

    /* 5. Devolver el modo visual del cursor segun la zona */
    if (*cursor_y >= 58 && *cursor_y <= 100 && *cursor_x >= 182 && *cursor_x <= 965) {
        return CURSOR_IBEAM_URL; /* Sobre la barra de URL */
    }
    if (hovered_link != -1 || (*cursor_y >= 8 && *cursor_y <= 50 && *cursor_x <= 1240)) {
        return CURSOR_HAND_LINK; /* Sobre un enlace o pestana superior */
    }
    return CURSOR_ARROW;
}

/* Devuelve el tamano de fuente activo segun el Zoom L3 (18px, 24px o 30px) */
static inline int eonsurf_get_active_body_font_px(void) {
    if (g_smart_zoom_level == 1) return 24;
    if (g_smart_zoom_level == 2) return 30;
    return 18;
}

#endif /* EONSURF_PS3_ERGONOMICS_H */
