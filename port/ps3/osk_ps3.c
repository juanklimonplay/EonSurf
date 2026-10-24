/*
 * EonSurf - Interfaz de teclado nativo de PS3 (sysutil/osk)
 * Implementación desacoplada y robusta inspirada en IRISMAN de Aldo Vargas.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sysutil/osk.h>
#include <sysutil/sysutil.h>
#include <sys/memory.h>

#define OSKDIALOG_FINISHED        0x503
#define OSKDIALOG_UNLOADED        0x504
#define OSKDIALOG_INPUT_ENTERED    0x505
#define OSKDIALOG_INPUT_CANCELED   0x506

static volatile int s_osk_event = 0;
static volatile bool s_osk_unloaded = false;
static sys_mem_container_t s_container_mem;
static oskCallbackReturnParam s_output_param;
static oskParam s_dialog_param;
static oskInputFieldInfo s_input_field;

static void osk_callback(u64 status, u64 param, void *userdata) {
    (void)param; (void)userdata;
    switch ((u32)status) {
        case OSKDIALOG_INPUT_CANCELED: s_osk_event = OSKDIALOG_INPUT_CANCELED; break;
        case OSKDIALOG_UNLOADED:       s_osk_unloaded = true; break;
        case OSKDIALOG_INPUT_ENTERED:  s_osk_event = OSKDIALOG_INPUT_ENTERED; break;
        case OSKDIALOG_FINISHED:       s_osk_event = OSKDIALOG_FINISHED; break;
        default: break;
    }
}

static void utf8_to_utf16(const uint8_t *src, uint16_t *dst) {
    while (*src) {
        if ((*src & 0x80) == 0) {
            *dst++ = *src++;
        } else if ((*src & 0xe0) == 0xc0) {
            *dst++ = ((*src & 0x1f) << 6) | (src[1] & 0x3f);
            src += 2;
        } else {
            *dst++ = '?';
            src++;
        }
    }
    *dst = 0;
}

static void utf16_to_utf8(const uint16_t *src, uint8_t *dst, size_t max_len) {
    size_t written = 0;
    while (*src && written + 1 < max_len) {
        if (*src < 0x80) {
            dst[written++] = (uint8_t)*src;
        } else {
            dst[written++] = '?';
        }
        src++;
    }
    dst[written] = 0;
}

int eonsurf_abrir_teclado(const char *titulo, const char *texto_inicial, char *resultado, size_t max_len) {
    if (!resultado || max_len == 0) return -1;
    if (sysMemContainerCreate(&s_container_mem, 4 * 1024 * 1024) < 0) {
        return -1;
    }

    uint16_t caption16[128] = {0};
    uint16_t init16[256] = {0};
    uint16_t out16[512] = {0};

    utf8_to_utf16((const uint8_t*)(titulo ? titulo : "EonSurf - Ingresar URL"), caption16);
    utf8_to_utf16((const uint8_t*)(texto_inicial ? texto_inicial : "https://"), init16);

    s_input_field.message = caption16;
    s_input_field.startText = init16;
    s_input_field.maxLength = (max_len > 255) ? 255 : (max_len - 1);

    s_output_param.res = OSK_NO_TEXT;
    s_output_param.len = s_input_field.maxLength;
    s_output_param.str = out16;

    oskSetKeyLayoutOption(OSK_10KEY_PANEL | OSK_FULLKEY_PANEL);
    s_dialog_param.firstViewPanel = OSK_PANEL_TYPE_ALPHABET_FULL_WIDTH;
    s_dialog_param.allowedPanels = (OSK_PANEL_TYPE_ALPHABET | OSK_PANEL_TYPE_NUMERAL);
    oskAddSupportLanguage(OSK_PANEL_TYPE_ALPHABET);
    oskSetLayoutMode(OSK_LAYOUTMODE_HORIZONTAL_ALIGN_CENTER);

    oskPoint pos = {0.0f, 0.0f};
    s_dialog_param.controlPoint = pos;
    s_dialog_param.prohibitFlags = 0;
    oskSetInitialInputDevice(OSK_DEVICE_PAD);

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, osk_callback, NULL);

    s_osk_event = 0;
    s_osk_unloaded = false;
    int action_res = 0;

    if (oskLoadAsync(s_container_mem, &s_dialog_param, &s_input_field) < 0) {
        sysMemContainerDestroy(s_container_mem);
        return -1;
    }

    while (!s_osk_unloaded) {
        sysUtilCheckCallback();
        usleep(10000);

        if (s_osk_event == OSKDIALOG_INPUT_ENTERED) {
            oskGetInputText(&s_output_param);
            s_osk_event = 0;
        } else if (s_osk_event == OSKDIALOG_INPUT_CANCELED) {
            oskAbort();
            oskUnloadAsync(&s_output_param);
            action_res = -1;
            s_osk_event = 0;
        } else if (s_osk_event == OSKDIALOG_FINISHED) {
            action_res = 1;
            oskUnloadAsync(&s_output_param);
            s_osk_event = 0;
        }
    }

    if (s_output_param.res == OSK_OK && action_res == 1) {
        utf16_to_utf8(out16, (uint8_t*)resultado, max_len);
    }

    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    sysMemContainerDestroy(s_container_mem);
    return (s_output_param.res == OSK_OK && action_res == 1) ? 0 : -1;
}
