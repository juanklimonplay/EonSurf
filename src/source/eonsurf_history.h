#ifndef EONSURF_HISTORY_H
#define EONSURF_HISTORY_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MAX_HISTORY_SLOTS 16
#define SNAPSHOT_TEXT_MAX 16384 /* 16 KB por snapshot (Total = 256 KB en RAM XDR) */

typedef struct {
    char url[256];
    char formatted_text[SNAPSHOT_TEXT_MAX];
    int  scroll_y;
    int  http_code;
    size_t bytes_downloaded;
    uint8_t valid;
} EonHistoryEntry;

static EonHistoryEntry g_history[MAX_HISTORY_SLOTS];
static int g_hist_cur = -1;  /* Índice de la página actual */
static int g_hist_top = -1;  /* Índice máximo alcanzado (para poder ir Forward '>') */

/* Coordenadas exactas de botones visuales '←', '→' y '↻' en la toolbar superior */
#define BTN_BACK_X    24
#define BTN_BACK_Y    60
#define BTN_BACK_W    36
#define BTN_BACK_H    36

#define BTN_FWD_X     74
#define BTN_FWD_Y     60
#define BTN_FWD_W     36
#define BTN_FWD_H     36

#define BTN_RELOAD_X  124
#define BTN_RELOAD_Y  60
#define BTN_RELOAD_W  36
#define BTN_RELOAD_H  36

/* 
 * 1. Llamar ANTES de saltar a una nueva URL:
 * Actualiza el scroll_y de la página donde estabas parado para recordarlo al volver.
 */
static inline void eonsurf_history_save_current_scroll(int current_scroll_y) {
    if (g_hist_cur >= 0 && g_hist_cur < MAX_HISTORY_SLOTS && g_history[g_hist_cur].valid) {
        g_history[g_hist_cur].scroll_y = current_scroll_y;
    }
}

/* 
 * 2. Llamar cuando se carga una página NUEVA (por clic en link, pestaña o búsqueda):
 * Empuja la página a la pila y trunca el historial futuro si habías vuelto atrás.
 */
static inline void eonsurf_history_push(const char *url, const char *formatted_text,
                                        int scroll_y, int http_code, size_t bytes_dl) {
    /* Si ya estamos en la misma URL exacta, solo actualizamos su contenido */
    if (g_hist_cur >= 0 && strcmp(g_history[g_hist_cur].url, url) == 0) {
        strncpy(g_history[g_hist_cur].formatted_text, formatted_text, SNAPSHOT_TEXT_MAX - 1);
        g_history[g_hist_cur].formatted_text[SNAPSHOT_TEXT_MAX - 1] = '\0';
        g_history[g_hist_cur].scroll_y = scroll_y;
        return;
    }

    /* Si la pila llegó al tope de 16 páginas, desplazamos 1 lugar hacia atrás (FIFO) */
    if (g_hist_cur + 1 >= MAX_HISTORY_SLOTS) {
        memmove(&g_history[0], &g_history[1], sizeof(EonHistoryEntry) * (MAX_HISTORY_SLOTS - 1));
        g_hist_cur = MAX_HISTORY_SLOTS - 2;
    }

    g_hist_cur++;
    g_hist_top = g_hist_cur; /* Al abrir un link nuevo, se limpia la rama Forward */

    EonHistoryEntry *e = &g_history[g_hist_cur];
    strncpy(e->url, url, sizeof(e->url) - 1);
    e->url[sizeof(e->url) - 1] = '\0';

    strncpy(e->formatted_text, formatted_text, SNAPSHOT_TEXT_MAX - 1);
    e->formatted_text[SNAPSHOT_TEXT_MAX - 1] = '\0';

    e->scroll_y = scroll_y;
    e->http_code = http_code;
    e->bytes_downloaded = bytes_dl;
    e->valid = 1;
}

/* 
 * 3. Volver Atrás (Botón Círculo 'O', D-Pad Izquierda o clic en '<'):
 * Restaura en < 0.1 ms la página anterior desde RAM sin usar internet.
 * Devuelve 1 si volvió atrás con éxito, 0 si ya estás en la primera página.
 */
static inline int eonsurf_history_go_back(int current_scroll_y, char *out_url, char *out_formatted_text,
                                          int *out_scroll_y, int *out_http_code, size_t *out_bytes_dl) {
    if (g_hist_cur <= 0) return 0; /* No hay páginas anteriores */

    /* Guardar dónde dejó el scroll en la página actual antes de retroceder */
    eonsurf_history_save_current_scroll(current_scroll_y);

    g_hist_cur--;
    EonHistoryEntry *e = &g_history[g_hist_cur];

    strcpy(out_url, e->url);
    strcpy(out_formatted_text, e->formatted_text);
    if (out_scroll_y) *out_scroll_y = e->scroll_y;
    if (out_http_code) *out_http_code = e->http_code;
    if (out_bytes_dl) *out_bytes_dl = e->bytes_downloaded;
    return 1;
}

/* 
 * 4. Ir Adelante (D-Pad Derecha o clic en '>'):
 * Restaura la página siguiente si el usuario había retrocedido antes.
 */
static inline int eonsurf_history_go_forward(int current_scroll_y, char *out_url, char *out_formatted_text,
                                             int *out_scroll_y, int *out_http_code, size_t *out_bytes_dl) {
    if (g_hist_cur < 0 || g_hist_cur >= g_hist_top) return 0; /* No hay página siguiente */

    eonsurf_history_save_current_scroll(current_scroll_y);

    g_hist_cur++;
    EonHistoryEntry *e = &g_history[g_hist_cur];

    strcpy(out_url, e->url);
    strcpy(out_formatted_text, e->formatted_text);
    if (out_scroll_y) *out_scroll_y = e->scroll_y;
    if (out_http_code) *out_http_code = e->http_code;
    if (out_bytes_dl) *out_bytes_dl = e->bytes_downloaded;
    return 1;
}

/* 
 * 5. Detecta si el usuario hizo clic con el cursor (Botón X) sobre los botones '←', '→' o '↻':
 * Devuelve: -1 si clickeó '←' (Back), +1 si clickeó '→' (Forward), 2 si clickeó '↻' (Reload), 0 si no tocó ninguno.
 */
static inline int eonsurf_check_nav_buttons_click(int cursor_x, int cursor_y) {
    if (cursor_x >= BTN_BACK_X && cursor_x <= (BTN_BACK_X + BTN_BACK_W) &&
        cursor_y >= BTN_BACK_Y && cursor_y <= (BTN_BACK_Y + BTN_BACK_H)) {
        return -1; /* Click en '←' */
    }
    if (cursor_x >= BTN_FWD_X && cursor_x <= (BTN_FWD_X + BTN_FWD_W) &&
        cursor_y >= BTN_FWD_Y && cursor_y <= (BTN_FWD_Y + BTN_FWD_H)) {
        return 1;  /* Click en '→' */
    }
    if (cursor_x >= BTN_RELOAD_X && cursor_x <= (BTN_RELOAD_X + BTN_RELOAD_W) &&
        cursor_y >= BTN_RELOAD_Y && cursor_y <= (BTN_RELOAD_Y + BTN_RELOAD_H)) {
        return 2;  /* Click en '↻' (Reload) */
    }
    return 0;
}

/* Devuelve 1 si hay historial para volver atrás o avanzar (útil para colorear los botones < y >) */
static inline int eonsurf_can_go_back(void)    { return (g_hist_cur > 0); }
static inline int eonsurf_can_go_forward(void) { return (g_hist_cur >= 0 && g_hist_cur < g_hist_top); }

#endif /* EONSURF_HISTORY_H */
