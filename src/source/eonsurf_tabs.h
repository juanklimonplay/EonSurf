#ifndef EONSURF_TABS_H
#define EONSURF_TABS_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MAX_TABS 4
#define TAB_BODY_MAX 16384

typedef struct {
    char url[512];
    char title[128];
    char body[TAB_BODY_MAX];
    int  scroll_y;
    int  max_scroll_y;
    int  http_code;
    int  active;      /* 1 = Pestaña abierta, 0 = Pestaña vacía/cerrada */
} EonTab;

static EonTab g_tabs[MAX_TABS];
static int    g_current_tab = 0;
static int    g_tab_count = 1;

/* Inicializar pestañas con estado por defecto */
static inline void eonsurf_tabs_init(void) {
    memset(g_tabs, 0, sizeof(g_tabs));

    /* Pestaña 0: Portal de Inicio */
    g_tabs[0].active = 1;
    strncpy(g_tabs[0].url, "eonsurf://portal", sizeof(g_tabs[0].url) - 1);
    strncpy(g_tabs[0].title, "Portal EonSurf", sizeof(g_tabs[0].title) - 1);
    strncpy(g_tabs[0].body,
            "Bienvenido a EonSurf - Navegador Nativo de Proxima Generacion para PlayStation 3.\n\n"
            "Elegi uno de los marcadores rapidos para comenzar a navegar:\n\n"
            "\x01https://es.wikipedia.org/wiki/PlayStation_3\x02 [1] Wikipedia: Historia y Arquitectura de PlayStation 3\n\n"
            "\x01https://es.wikipedia.org/wiki/Cell_(microprocesador)\x02 [2] Wikipedia: Microprocesador Cell Broadband Engine\n\n"
            "\x01https://raw.githubusercontent.com/aldostools/webMAN-MOD/master/README.md\x02 [3] webMAN MOD: Documentacion Oficial y Descargas\n\n"
            "\x01https://github.com/aldostools/webMAN-MOD/releases/download/1.47.50/webMAN_MOD_1.47.50_Installer.pkg\x02 [4] Descargar webMAN MOD 1.47.50 (.pkg directo a /dev_hdd0/packages)\n\n"
            "\x01https://es.wikipedia.org/wiki/RSX_Reality_Synthesizer\x02 [5] RSX Reality Synthesizer: Especificaciones de la GPU de PS3\n",
            sizeof(g_tabs[0].body) - 1);
    g_tabs[0].scroll_y = 0;
    g_tabs[0].max_scroll_y = 0;
    g_tabs[0].http_code = 200;

    /* Pestaña 1: Wikipedia - PlayStation 3 */
    g_tabs[1].active = 1;
    strncpy(g_tabs[1].url, "https://es.wikipedia.org/wiki/PlayStation_3", sizeof(g_tabs[1].url) - 1);
    strncpy(g_tabs[1].title, "Wikipedia - PS3", sizeof(g_tabs[1].title) - 1);
    strncpy(g_tabs[1].body,
            "PlayStation 3 (oficialmente abreviada como PS3) es la tercera videoconsola de sobremesa producida por Sony Computer Entertainment.\n\n"
            "Fue la quinta y ultima consola de Sony en ser disenada por Ken Kutaragi y formo parte de las videoconsolas de septima generacion. Sus competidores fueron la Xbox 360 de Microsoft y la Wii de Nintendo.\n\n"
            "El procesador central de la consola es el Cell Broadband Engine desarrollado por Sony, Toshiba e IBM (STI), acompanado de la GPU RSX Reality Synthesizer de NVIDIA.",
            sizeof(g_tabs[1].body) - 1);
    g_tabs[1].scroll_y = 0;
    g_tabs[1].max_scroll_y = 0;
    g_tabs[1].http_code = 200;

    g_current_tab = 0;
    g_tab_count = 2;
}

/* Cambiar de pestaña activa */
static inline int eonsurf_tab_select(int tab_idx) {
    if (tab_idx < 0 || tab_idx >= MAX_TABS || !g_tabs[tab_idx].active) return 0;
    g_current_tab = tab_idx;
    return 1;
}

/* Abrir una nueva pestaña */
static inline int eonsurf_tab_open_new(const char *url, const char *title) {
    int free_slot = -1;
    for (int i = 0; i < MAX_TABS; i++) {
        if (!g_tabs[i].active) { free_slot = i; break; }
    }
    if (free_slot == -1) return -1; /* Máximo de 4 pestañas alcanzado */

    g_tabs[free_slot].active = 1;
    strncpy(g_tabs[free_slot].url, url ? url : "eonsurf://portal", sizeof(g_tabs[free_slot].url) - 1);
    strncpy(g_tabs[free_slot].title, title ? title : "Nueva Pestana", sizeof(g_tabs[free_slot].title) - 1);
    strncpy(g_tabs[free_slot].body,
            "Nueva pestana en blanco.\nPresiona Triangulo (^) para escribir una direccion web con el teclado de Sony.",
            sizeof(g_tabs[free_slot].body) - 1);
    g_tabs[free_slot].scroll_y = 0;
    g_tabs[free_slot].max_scroll_y = 0;
    g_tabs[free_slot].http_code = 200;

    g_current_tab = free_slot;
    g_tab_count++;
    return free_slot;
}

/* Cerrar pestaña */
static inline int eonsurf_tab_close(int tab_idx) {
    if (tab_idx < 0 || tab_idx >= MAX_TABS || !g_tabs[tab_idx].active) return 0;
    if (g_tab_count <= 1) return 0; /* No cerrar la única pestaña existente */

    g_tabs[tab_idx].active = 0;
    g_tab_count--;

    /* Si cerramos la que estábamos viendo, cambiar a otra válida */
    if (g_current_tab == tab_idx) {
        for (int i = 0; i < MAX_TABS; i++) {
            if (g_tabs[i].active) { g_current_tab = i; break; }
        }
    }
    return 1;
}

/* Detecta clics sobre las pestañas superiores o el botón '+' */
/* Devuelve:
 *  0..3 = Clic en pestaña para cambiar
 *  10..13 = Clic en botón 'x' de la pestaña i-10 para cerrarla
 *  99 = Clic en botón '+' para abrir pestaña nueva
 *  -1 = No clic en tabs
 */
static inline int eonsurf_tabs_check_click(int cursor_x, int cursor_y) {
    if (cursor_y < 6 || cursor_y > 48) return -1;

    int tab_w = 230;
    int tab_start_x = 48;
    int slot_x = tab_start_x;

    for (int i = 0; i < MAX_TABS; i++) {
        if (!g_tabs[i].active) continue;

        /* Comprobar si tocó la 'x' de cerrar pestaña */
        int close_btn_x = slot_x + tab_w - 28;
        if (cursor_x >= close_btn_x && cursor_x <= close_btn_x + 22 &&
            cursor_y >= 10 && cursor_y <= 38) {
            return 10 + i;
        }

        /* Comprobar si tocó el cuerpo de la pestaña */
        if (cursor_x >= slot_x && cursor_x < close_btn_x) {
            return i;
        }

        slot_x += tab_w + 8;
    }

    /* Comprobar si tocó el botón '+' */
    if (cursor_x >= slot_x && cursor_x <= slot_x + 36 &&
        cursor_y >= 10 && cursor_y <= 40) {
        return 99;
    }

    return -1;
}

#endif /* EONSURF_TABS_H */
