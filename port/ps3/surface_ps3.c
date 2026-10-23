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
static int cur_buffer = 0;
static int first_flip = 1;
static int pad_initialized = 0;
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

        ioPadInit(7);
        pad_initialized = 1;
    }

    nsfb->ptr = (uint8_t *)fb[cur_buffer];
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

static int ps3_surface_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    (void)nsfb;
    (void)box;

    if (!first_flip) {
        while (gcmGetFlipStatus() != 0) usleep(200);
    }
    gcmResetFlipStatus();
    gcmSetFlip(ctx, cur_buffer);
    rsxFlushBuffer(ctx);
    gcmSetWaitFlip(ctx);

    cur_buffer ^= 1;
    first_flip = 0;

    /* Apuntar la superficie de NetSurf al buffer activo siguiente */
    nsfb->ptr = (uint8_t *)fb[cur_buffer];
    return 0;
}

static bool ps3_surface_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
    (void)nsfb;
    (void)timeout;
    sysUtilCheckCallback();

    if (!pad_initialized) return false;

    padInfo pinfo;
    padData pdata;
    ioPadGetInfo(&pinfo);

    if (pinfo.status[0]) {
        ioPadGetData(0, &pdata);

        int ax = pdata.ANA_L_H - 128;
        int ay = pdata.ANA_L_V - 128;

        if (abs(ax) > 15 || abs(ay) > 15) {
            virtual_mouse_x += ax / 8;
            virtual_mouse_y += ay / 8;

            if (virtual_mouse_x < 0) virtual_mouse_x = 0;
            if (virtual_mouse_x >= nsfb->width) virtual_mouse_x = nsfb->width - 1;
            if (virtual_mouse_y < 0) virtual_mouse_y = 0;
            if (virtual_mouse_y >= nsfb->height) virtual_mouse_y = nsfb->height - 1;

            event->type = NSFB_EVENT_MOVE_ABSOLUTE;
            event->value.vector.x = virtual_mouse_x;
            event->value.vector.y = virtual_mouse_y;
            event->value.vector.z = 0;
            return true;
        }

        if (pdata.BTN_CROSS) {
            event->type = NSFB_EVENT_KEY_DOWN;
            event->value.keycode = NSFB_KEY_MOUSE_1;
            return true;
        }
    }

    return false;
}

const nsfb_surface_rt_t ps3_rt_surface = {
    .type = NSFB_SURFACE_RAM,
    .defaults = ps3_surface_defaults,
    .initialise = ps3_surface_initialise,
    .finalise = ps3_surface_finalise,
    .update = ps3_surface_update,
    .input = ps3_surface_input,
};
