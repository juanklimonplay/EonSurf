/*
 * EonSurf - Teclado en pantalla nativo de PS3 (sysutil/osk)
 * Invoca el teclado oficial del sistema operativo de la consola.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <sysutil/osk.h>
#include <sysutil/sysutil.h>

static oskCallbackReturnParam osk_output;
static int osk_done = 0;

static void osk_callback(u64 status, u64 param, void *userdata)
{
    (void)param; (void)userdata;
    if (status == SYSUTIL_OSK_DONE) {
        oskAbort();
        oskUnloadAsync(&osk_output);
        osk_done = 1;
    }
}

int eonsurf_abrir_teclado(const char *titulo, const char *texto_inicial, char *resultado, size_t max_len)
{
    oskParam osk_param;
    oskInputFieldInfo input_info;
    u16 str_init[512] = {0};
    u16 str_msg[128] = {0};

    osk_done = 0;

    /* Convertir ASCII a UTF-16 para la API de PS3 */
    for (size_t i = 0; i < strlen(texto_inicial) && i < 511; i++) str_init[i] = (u16)texto_inicial[i];
    for (size_t i = 0; i < strlen(titulo) && i < 127; i++) str_msg[i] = (u16)titulo[i];

    input_info.message = str_msg;
    input_info.init_text = str_init;
    input_info.limit = max_len > 511 ? 511 : max_len - 1;

    memset(&osk_param, 0, sizeof(oskParam));
    osk_param.firstViewPanel = OSK_PANEL_TYPE_ALPHABET_FULL_WIDTH;
    osk_param.allowedPanels = OSK_PANEL_TYPE_ALPHABET | OSK_PANEL_TYPE_URL;

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT1, osk_callback, NULL);

    if (oskSetKeyLayoutOption(OSK_10KEY_PANEL) != 0) {
        /* Continuar con layout por defecto */
    }

    if (oskLoadAsync(&osk_param, &input_info) != 0) {
        return -1;
    }

    while (!osk_done) {
        sysUtilCheckCallback();
    }

    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT1);

    if (osk_output.res == OSK_OK) {
        /* Convertir resultado UTF-16 a UTF-8/ASCII */
        size_t len = osk_output.length;
        for (size_t i = 0; i < len && i < max_len - 1; i++) {
            resultado[i] = (char)osk_output.str[i];
        }
        resultado[len < max_len ? len : max_len - 1] = '\0';
        return 0;
    }

    return -1;
}
