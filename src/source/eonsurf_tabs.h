#ifndef EONSURF_TABS_H
#define EONSURF_TABS_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MAX_TABS 12
#define TAB_BODY_MAX 16384

typedef struct {
    char url[512];
    char title[128];
    char body[TAB_BODY_MAX];
    int  scroll_y;
    int  max_scroll_y;
    int  http_code;
    int  active;      /* 1 = Pestana abierta, 0 = Pestana vacia/cerrada */
    int  is_sleeping; /* 1 = Pestaña en reposo para ahorrar RAM (< 256MB) */
    uint32_t last_accessed_sec;
} EonTab;

static EonTab g_tabs[MAX_TABS];
static int    g_current_tab = 0;
static int    g_tab_count = 1;

/* Inicializar pestanas con estado por defecto */
static inline void eonsurf_tabs_init(void) {
    memset(g_tabs, 0, sizeof(g_tabs));

    /* Pestana 0: Portal de Inicio */
    g_tabs[0].active = 1;
    strncpy(g_tabs[0].url, "eonsurf://portal", sizeof(g_tabs[0].url) - 1);
    strncpy(g_tabs[0].title, "Portal EonSurf", sizeof(g_tabs[0].title) - 1);
    strncpy(g_tabs[0].body,
            "Bienvenido al motor nativo Dark Glass corriendo sobre el silicio Cell B.E. a 60 FPS.\n\n"
            "## Accesos Directos de la Scene PS3 (Presiona X sobre un enlace)\n"
            " - [PlayStation 3 en Wikipedia](https://es.wikipedia.org/wiki/PlayStation_3)\n"
            " - [Procesador Cell Broadband Engine](https://es.wikipedia.org/wiki/Cell_(microprocesador))\n"
            " - [Documentacion Oficial webMAN MOD](https://raw.githubusercontent.com/aldostools/webMAN-MOD/master/README.md)\n"
            " - [Repositorio PSL1GHT SDK](https://raw.githubusercontent.com/ps3dev/PSL1GHT/master/README.md)\n\n"
            "## Estado del Motor EonSurf Core\n"
            " - Renderizador TrueType con Anti-Aliasing y Sombra Sub-pixel activo.\n"
            " - Descargador directo de paquetes .PKG hacia /dev_hdd0/packages/ listo.\n"
            " - Presiona Triangulo para buscar en la Web o escribir cualquier URL.",
            sizeof(g_tabs[0].body) - 1);
    g_tabs[0].scroll_y = 0;
    g_tabs[0].max_scroll_y = 0;
    g_tabs[0].http_code = 200;

    /* Pestana 1: Wikipedia - PlayStation 3 */
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

/* Cambiar de pestana activa */
static inline int eonsurf_tab_select(int tab_idx) {
    if (tab_idx < 0 || tab_idx >= MAX_TABS || !g_tabs[tab_idx].active) return 0;
    g_current_tab = tab_idx;
    return 1;
}

/* Abrir una nueva pestana dinamica (hasta MAX_TABS respetando limite de RAM 256MB) */
static inline int eonsurf_tab_open_new(const char *url, const char *title) {
    int free_slot = -1;
    for (int i = 0; i < MAX_TABS; i++) {
        if (!g_tabs[i].active) { free_slot = i; break; }
    }
    if (free_slot == -1) return -1; /* Limite de pestanas simultaneas alcanzado */

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

/* Cerrar pestana */
static inline int eonsurf_tab_close(int tab_idx) {
    if (tab_idx < 0 || tab_idx >= MAX_TABS || !g_tabs[tab_idx].active) return 0;
    if (g_tab_count <= 1) return 0; /* No cerrar la unica pestana existente */

    g_tabs[tab_idx].active = 0;
    g_tab_count--;

    /* Si cerramos la que estabamos viendo, cambiar a otra valida */
    if (g_current_tab == tab_idx) {
        for (int i = 0; i < MAX_TABS; i++) {
            if (g_tabs[i].active) { g_current_tab = i; break; }
        }
    }
    return 1;
}

/* Ancho dinámico de pestaña según la cantidad de pestañas activas */
static inline int eonsurf_get_dynamic_tab_width(void) {
    int visible_tabs = 0;
    for (int i = 0; i < MAX_TABS; i++) {
        if (g_tabs[i].active) visible_tabs++;
    }
    if (visible_tabs <= 0) visible_tabs = 1;
    int available_w = 930;
    int tab_w = (available_w - 40) / visible_tabs;
    if (tab_w > 230) tab_w = 230;
    if (tab_w < 90) tab_w = 90;
    return tab_w;
}

/* Detecta clics sobre las pestanas superiores o el boton '+' */
/* Devuelve:
 *  0..MAX_TABS-1 = Clic en pestana para cambiar
 *  20..20+MAX_TABS-1 = Clic en boton 'x' de la pestana i-20 para cerrarla
 *  99 = Clic en boton '+' para abrir pestana nueva
 *  -1 = No clic en tabs
 */
static inline int eonsurf_tabs_check_click(int cursor_x, int cursor_y) {
    if (cursor_y < 6 || cursor_y > 48) return -1;

    int tab_w = eonsurf_get_dynamic_tab_width();

    int slot_x = 48;
    for (int i = 0; i < MAX_TABS; i++) {
        if (!g_tabs[i].active) continue;

        /* Comprobar si toco la 'x' de cerrar pestana */
        int close_btn_x = slot_x + tab_w - 24;
        if (cursor_x >= close_btn_x && cursor_x <= close_btn_x + 22 &&
            cursor_y >= 10 && cursor_y <= 38) {
            return 20 + i;
        }

        /* Comprobar si toco el cuerpo de la pestana */
        if (cursor_x >= slot_x && cursor_x < close_btn_x) {
            return i;
        }

        slot_x += tab_w + 4;
    }

    /* Comprobar si toco el boton '+' */
    if (cursor_x >= slot_x && cursor_x <= slot_x + 36 &&
        cursor_y >= 10 && cursor_y <= 40) {
        return 99;
    }

    return -1;
}

/* Pone en reposo las pestanas en segundo plano inactivas por mas de 10 minutos (600s) */
static inline void eonsurf_tabs_check_sleep_timeout(uint32_t current_time_sec) {
    for (int i = 0; i < MAX_TABS; i++) {
        if (!g_tabs[i].active || i == g_current_tab) continue;
        if (current_time_sec > g_tabs[i].last_accessed_sec &&
            (current_time_sec - g_tabs[i].last_accessed_sec) > 600) {
            if (!g_tabs[i].is_sleeping) {
                g_tabs[i].is_sleeping = 1;
                /* Liberar buffer de texto de la pestana en reposo para liberar memoria */
                g_tabs[i].body[0] = '\0';
            }
        }
    }
}

#endif /* EONSURF_TABS_H */
