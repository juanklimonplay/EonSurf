/*
 * EonSurf - Superficie nativa libnsfb para PlayStation 3 (RSX Framebuffer)
 * Conecta el motor de dibujo de NetSurf directo con la memoria de pantalla de la PS3.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <io/pad.h>
#include <rsx/rsx.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"

#include "nsfb.h"
#include "surface.h"
#include "plot.h"

#define CB_SIZE   0x100000
#define HOST_SIZE (32 * 1024 * 1024)

static gcmContextData *ctx = NULL;
static u32 *fb[2] = {NULL, NULL};
static u32 fb_off[2];
static int pad_initialized = 0;
static int dirty = 0;
static padData last_pad;
static int have_pad = 0;
static int cross_down = 0;
static int virtual_mouse_x = 640;
static int virtual_mouse_y = 360;

static void on_sysutil(u64 status, u64 param, void *ud)
{
    (void)param; (void)ud;
    if (status == SYSUTIL_EXIT_GAME) {
        exit(0);
    }
}

static int ps3_surface_defaults(nsfb_t *nsfb)
{
    nsfb->width = 1280;
    nsfb->height = 720;
    nsfb->format = NSFB_FMT_XRGB8888;
    select_plotters(nsfb);
    return 0;
}

static int ps3_surface_initialise(nsfb_t *nsfb)
{
    if (ctx == NULL) {
        sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, on_sysutil, NULL);

        void *host = memalign(1024 * 1024, HOST_SIZE);
        if (!host) return -1;

        rsxInit(&ctx, CB_SIZE, HOST_SIZE, host);

        videoState st;
        videoResolution res;
        videoConfiguration vc;
        videoGetState(0, 0, &st);
        videoGetResolution(st.displayMode.resolution, &res);

        memset(&vc, 0, sizeof(vc));
        vc.resolution = st.displayMode.resolution;
        vc.format = VIDEO_BUFFER_FORMAT_XRGB;
        vc.pitch = res.width * sizeof(u32);
        videoConfigure(0, &vc, NULL, 0);
        gcmSetFlipMode(GCM_FLIP_VSYNC);

        nsfb->width = res.width;
        nsfb->height = res.height;
        nsfb->linelen = res.width * sizeof(u32);

        for (int i = 0; i < 2; i++) {
            fb[i] = rsxMemalign(64, nsfb->height * nsfb->linelen);
            rsxAddressToOffset(fb[i], &fb_off[i]);
            gcmSetDisplayBuffer(i, fb_off[i], nsfb->linelen, nsfb->width, nsfb->height);
        }

        /* Un solo buffer visible: NetSurf redibuja por zonas, asi que
         * alternar buffers dejaria contenido viejo en el otro. */
        memset(fb[0], 0xff, nsfb->height * nsfb->linelen);
        gcmResetFlipStatus();
        gcmSetFlip(ctx, 0);
        rsxFlushBuffer(ctx);
        gcmSetWaitFlip(ctx);

        ioPadInit(7);
        pad_initialized = 1;
    }

    nsfb->ptr = (uint8_t *)fb[0];
    return 0;
}

/* libnsfb no provee geometry por defecto: llamarla con NULL salta a 0.
 * La resolucion real la fija videoGetState() en initialise; aca solo
 * aceptamos el formato pedido antes de iniciar. */
static int ps3_surface_geometry(nsfb_t *nsfb, int width, int height,
                                enum nsfb_format_e format)
{
    if (ctx == NULL) {
        if (width > 0) nsfb->width = width;
        if (height > 0) nsfb->height = height;
        nsfb->linelen = nsfb->width * sizeof(u32);
    }
    if (format != NSFB_FMT_ANY) nsfb->format = NSFB_FMT_XRGB8888;
    select_plotters(nsfb);
    return 0;
}

static int ps3_surface_finalise(nsfb_t *nsfb)
{
    (void)nsfb;
    if (ctx) {
        gcmSetWaitFlip(ctx);
        rsxFinish(ctx, 1);
    }
    if (pad_initialized) {
        ioPadEnd();
        pad_initialized = 0;
    }
    return 0;
}

/* Muestra el buffer (siempre el 0) sin bloquear: si el flip anterior
 * todavia no termino, queda pendiente y lo reintenta el proximo update o
 * input. La PS3 escanea el buffer sola, pero RPCS3 solo presenta en flip. */
static void ps3_present(void)
{
    if (!dirty || ctx == NULL || gcmGetFlipStatus() != 0)
        return;
    gcmResetFlipStatus();
    gcmSetFlip(ctx, 0);
    rsxFlushBuffer(ctx);
    dirty = 0;
}

static int ps3_surface_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    (void)nsfb;
    (void)box;
    dirty = 1;
    ps3_present();
    return 0;
}

static bool ps3_surface_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
    (void)nsfb;
    (void)timeout;
    sysUtilCheckCallback();
    ps3_present();

    if (!pad_initialized) return false;

    padInfo pinfo;
    padData pdata;
    ioPadGetInfo(&pinfo);

    if (pinfo.status[0]) {
        /* ioPadGetData solo trae datos cuando el mando cambio (len > 0);
         * si no, seguimos con el ultimo estado conocido. */
        ioPadGetData(0, &pdata);
        if (pdata.len > 0) {
            last_pad = pdata;
            have_pad = 1;
        }
        if (!have_pad)
            return false;

        /* Cruz: un clic por pulsacion (DOWN al apretar, UP al soltar) */
        if (last_pad.BTN_CROSS != cross_down) {
            cross_down = last_pad.BTN_CROSS;
            event->type = cross_down ? NSFB_EVENT_KEY_DOWN : NSFB_EVENT_KEY_UP;
            event->value.keycode = NSFB_KEY_MOUSE_1;
            return true;
        }

        int ax = last_pad.ANA_L_H - 128;
        int ay = last_pad.ANA_L_V - 128;

        if (abs(ax) > 15 || abs(ay) > 15) {
            int nx = virtual_mouse_x + ax / 8;
            int ny = virtual_mouse_y + ay / 8;

            if (nx < 0) nx = 0;
            if (nx >= nsfb->width) nx = nsfb->width - 1;
            if (ny < 0) ny = 0;
            if (ny >= nsfb->height) ny = nsfb->height - 1;

            /* Solo hay evento si el cursor se movio de verdad: fbtk_event
             * junta movimientos en un bucle hasta que input devuelve false,
             * asi que un stick descentrado contra el borde lo colgaba. */
            if (nx != virtual_mouse_x || ny != virtual_mouse_y) {
                virtual_mouse_x = nx;
                virtual_mouse_y = ny;
                event->type = NSFB_EVENT_MOVE_ABSOLUTE;
                event->value.vector.x = virtual_mouse_x;
                event->value.vector.y = virtual_mouse_y;
                event->value.vector.z = 0;
                return true;
            }
        }
    }

    return false;
}

static nsfb_surface_rtns_t ps3_rt_surface = {
    .defaults = ps3_surface_defaults,
    .initialise = ps3_surface_initialise,
    .finalise = ps3_surface_finalise,
    .geometry = ps3_surface_geometry,
    .parameters = NULL,
    .input = ps3_surface_input,
    .claim = NULL,
    .update = ps3_surface_update,
    .cursor = NULL,
};

/* NSFB_SURFACE_PS3 lo agrega tweak-sources.sh al enum de libnsfb.h. Antes habia
 * un #ifndef que, al ser un enum y no una macro, registraba esta superficie como
 * RAM: las imagenes y miniaturas recibian la pantalla real y al liberarlas se
 * apagaban el RSX y el mando. */
NSFB_SURFACE_DEF(ps3, NSFB_SURFACE_PS3, &ps3_rt_surface)
