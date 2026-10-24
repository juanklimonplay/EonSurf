#ifndef EONSURF_OSK_H
#define EONSURF_OSK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sysutil/sysutil.h>
#include <sysutil/osk.h>
#include <sys/memory.h>

#define OSK_MAX_CHARS 256

/* Estados de la máquina asíncrona del teclado Sony */
typedef enum {
    EON_OSK_IDLE = 0,
    EON_OSK_ACTIVE,
    EON_OSK_FINISHED,
    EON_OSK_CANCELED
} EonOskState;

static volatile EonOskState g_osk_state = EON_OSK_IDLE;
static sys_mem_container_t  g_osk_container = 0;
static int                  g_osk_container_created = 0;

/* Buffers alineados a 128 bytes en formato UTF-16 Big-Endian para Lv2 */
static uint16_t g_osk_title_u16[64]           __attribute__((aligned(128)));
static uint16_t g_osk_init_u16[OSK_MAX_CHARS] __attribute__((aligned(128)));
static uint16_t g_osk_out_u16[OSK_MAX_CHARS]  __attribute__((aligned(128)));
static char     g_osk_result_utf8[OSK_MAX_CHARS];

static oskCallbackReturnParam g_osk_out_param __attribute__((aligned(128)));
static oskInputFieldInfo      g_osk_field     __attribute__((aligned(128)));
static oskParam               g_osk_param     __attribute__((aligned(128)));

/* Convierte ASCII/UTF-8 a UTF-16BE (uint16_t) requerido por Sony OSK */
static inline void utf8_to_u16be(const char *src, uint16_t *dst, size_t max_len) {
    size_t i = 0;
    for (; src[i] != '\0' && i + 1 < max_len; i++) {
        dst[i] = (uint16_t)((unsigned char)src[i]);
    }
    dst[i] = 0;
}

/* Convierte la respuesta UTF-16BE de Sony OSK a char* UTF-8 para EonSurf */
static inline void u16be_to_utf8(const uint16_t *src, char *dst, size_t max_len) {
    size_t di = 0;
    for (size_t i = 0; src[i] != 0 && di + 1 < max_len; i++) {
        uint16_t cp = src[i];
        if (cp < 0x80) {
            dst[di++] = (char)cp;
        } else if (cp < 0x800 && di + 2 < max_len) {
            dst[di++] = (char)(0xC0 | (cp >> 6));
            dst[di++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    dst[di] = '\0';
}

/* 
 * 1. Procesa los eventos de OSK dentro del callback principal SYSUTIL_EVENT_SLOT0
 */
static inline void eonsurf_osk_handle_sysutil_event(uint64_t status) {
    switch ((uint32_t)status) {
        case SYSUTIL_OSK_DONE:
            g_osk_out_param.res = OSK_OK;
            g_osk_out_param.len = OSK_MAX_CHARS - 1;
            g_osk_out_param.str = g_osk_out_u16;
            oskGetInputText(&g_osk_out_param);
            oskAbort();
            oskUnloadAsync(&g_osk_out_param);
            break;

        case SYSUTIL_OSK_CLIENT_CANCELED:
            oskAbort();
            oskUnloadAsync(&g_osk_out_param);
            g_osk_state = EON_OSK_CANCELED;
            break;

        case SYSUTIL_OSK_UNLOADED:
            if (g_osk_container_created) {
                sysMemContainerDestroy(g_osk_container);
                g_osk_container_created = 0;
            }
            if (g_osk_state != EON_OSK_CANCELED) {
                u16be_to_utf8(g_osk_out_u16, g_osk_result_utf8, sizeof(g_osk_result_utf8));
                g_osk_state = EON_OSK_FINISHED;
            } else {
                g_osk_state = EON_OSK_IDLE;
            }
            break;

        default:
            break;
    }
}

/* 
 * 2. Inicia la apertura asíncrona del teclado oficial Sony OSK
 */
static inline int eonsurf_osk_open(const char *prompt_title, const char *initial_text) {
    if (g_osk_state == EON_OSK_ACTIVE) return 0;

    /* Crear contenedor de memoria de 4 MB para el subsistema OSK */
    if (sysMemContainerCreate(&g_osk_container, 4 * 1024 * 1024) != 0) {
        return 0;
    }
    g_osk_container_created = 1;

    utf8_to_u16be(prompt_title ? prompt_title : "EonSurf: Ingresar URL o Busqueda Web",
                  g_osk_title_u16, 64);
    utf8_to_u16be(initial_text ? initial_text : "",
                  g_osk_init_u16, OSK_MAX_CHARS);
    memset(g_osk_out_u16, 0, sizeof(g_osk_out_u16));

    g_osk_field.message   = g_osk_title_u16;
    g_osk_field.startText = g_osk_init_u16;
    g_osk_field.maxLength = OSK_MAX_CHARS - 1;

    memset(&g_osk_param, 0, sizeof(g_osk_param));
    g_osk_param.allowedPanels = OSK_PANEL_TYPE_ALPHABET | OSK_PANEL_TYPE_NUMERAL | OSK_PANEL_TYPE_URL;
    g_osk_param.firstViewPanel = OSK_PANEL_TYPE_URL;
    g_osk_param.controlPoint.x = 0.0f;
    g_osk_param.controlPoint.y = 0.0f;
    g_osk_param.prohibitFlags  = OSK_PROHIBIT_RETURN;

    oskSetKeyLayoutOption(OSK_10KEY_PANEL | OSK_FULLKEY_PANEL);
    oskSetInitialInputDevice(OSK_DEVICE_PAD);

    if (oskLoadAsync(g_osk_container, &g_osk_param, &g_osk_field) == 0) {
        g_osk_state = EON_OSK_ACTIVE;
        return 1;
    }

    sysMemContainerDestroy(g_osk_container);
    g_osk_container_created = 0;
    return 0;
}

/* 
 * 3. Consultar en cada iteración del bucle principal:
 * Si el usuario terminó de escribir y presionó START/ENTER, devuelve 1 con el texto.
 */
static inline int eonsurf_osk_poll_result(char *out_text, size_t max_len) {
    if (g_osk_state == EON_OSK_FINISHED) {
        g_osk_state = EON_OSK_IDLE;
        if (strlen(g_osk_result_utf8) > 0) {
            strncpy(out_text, g_osk_result_utf8, max_len - 1);
            out_text[max_len - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

/* Devuelve 1 si el teclado Sony está abierto (para pausar el cursor de fondo) */
static inline int eonsurf_osk_is_active(void) {
    return (g_osk_state == EON_OSK_ACTIVE);
}

#endif /* EONSURF_OSK_H */
