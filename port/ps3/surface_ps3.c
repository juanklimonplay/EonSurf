/*
 * EonSurf - Superficie nativa libnsfb para PlayStation 3 (RSX Framebuffer)
 * Conecta el motor de dibujo de NetSurf directo con la memoria de pantalla de la PS3.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <malloc.h>
#include <ppu-types.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <io/pad.h>
#include <rsx/rsx.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_plot_util.h"
#include "libnsfb_event.h"

#include "nsfb.h"
#include "surface.h"
#include "plot.h"
#include "cursor.h"

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

/* Mando: se lee a ritmo fijo para que la velocidad del cursor y del scroll
 * no dependa de cuantas vueltas da el bucle de NetSurf. */
#define PAD_POLL_MS      16     /* ~60 lecturas por segundo */
#define STICK_DEADZONE   20     /* de 0..127 */
#define CURSOR_MAX_SPEED 22.0f  /* px por lectura con el stick a fondo */
#define SCROLL_MAX_RATE  0.35f  /* eventos de rueda por lectura (~21/s, 100 px c/u) */

static unsigned long long next_poll_ms = 0;
static float cursor_frac_x = 0, cursor_frac_y = 0;
static float scroll_acc = 0;

/* Cola corta: una lectura puede generar clic + movimiento + scroll */
static nsfb_event_t evq[8];
static int evq_head = 0, evq_len = 0;

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

/* Cursor por software (como el driver SDL de libnsfb): se borra antes de que
 * NetSurf dibuje encima (claim) y se vuelve a pintar al terminar (update). */
static int ps3_surface_claim(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    struct nsfb_cursor_s *cursor = nsfb->cursor;

    if ((cursor != NULL) && (cursor->plotted == true) &&
        nsfb_plot_bbox_intersect(box, &cursor->loc)) {
        nsfb_cursor_clear(nsfb, cursor);
    }
    return 0;
}

static int ps3_surface_cursor(nsfb_t *nsfb, struct nsfb_cursor_s *cursor)
{
    if ((cursor != NULL) && (cursor->plotted == true)) {
        nsfb_cursor_clear(nsfb, cursor);
        nsfb_cursor_plot(nsfb, cursor);
        dirty = 1;
        ps3_present();
    }
    return 0;
}

static int ps3_surface_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    struct nsfb_cursor_s *cursor = nsfb->cursor;

    (void)box;
    if ((cursor != NULL) && (cursor->plotted == false))
        nsfb_cursor_plot(nsfb, cursor);
    dirty = 1;
    ps3_present();
    return 0;
}

static unsigned long long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
}

static void evq_push(enum nsfb_event_type_e type, int code, int x, int y)
{
    if (evq_len == (int)(sizeof(evq) / sizeof(evq[0])))
        return;
    nsfb_event_t *e = &evq[(evq_head + evq_len) % (sizeof(evq) / sizeof(evq[0]))];
    e->type = type;
    if (type == NSFB_EVENT_MOVE_ABSOLUTE) {
        e->value.vector.x = x;
        e->value.vector.y = y;
        e->value.vector.z = 0;
    } else {
        e->value.keycode = code;
    }
    evq_len++;
}

/* Inclinacion del stick (0..255, centro 128) a -1..1 con zona muerta y curva
 * cuadratica: poco recorrido = movimiento fino, a fondo = rapido. */
static float stick_curve(int raw)
{
    int v = raw - 128;
    int a = abs(v);
    if (a <= STICK_DEADZONE)
        return 0.0f;
    float n = (float)(a - STICK_DEADZONE) / (float)(127 - STICK_DEADZONE);
    if (n > 1.0f) n = 1.0f;
    n = n * n;
    return v < 0 ? -n : n;
}

static void ps3_poll_pad(nsfb_t *nsfb)
{
    padInfo pinfo;
    padData pdata;

    ioPadGetInfo(&pinfo);
    if (!pinfo.status[0])
        return;

    /* ioPadGetData solo trae datos cuando el mando cambio (len > 0);
     * si no, seguimos con el ultimo estado conocido. */
    ioPadGetData(0, &pdata);
    if (pdata.len > 0) {
        last_pad = pdata;
        have_pad = 1;
    }
    if (!have_pad)
        return;

    /* Cruz: un clic por pulsacion (DOWN al apretar, UP al soltar) */
    if (last_pad.BTN_CROSS != cross_down) {
        cross_down = last_pad.BTN_CROSS;
        evq_push(cross_down ? NSFB_EVENT_KEY_DOWN : NSFB_EVENT_KEY_UP,
                 NSFB_KEY_MOUSE_1, 0, 0);
    }

    /* Stick izquierdo: cursor. Solo hay evento si se movio de verdad: fbtk_event
     * junta movimientos en un bucle hasta que input devuelve false, asi que un
     * stick descentrado contra el borde lo colgaba. */
    cursor_frac_x += stick_curve(last_pad.ANA_L_H) * CURSOR_MAX_SPEED;
    cursor_frac_y += stick_curve(last_pad.ANA_L_V) * CURSOR_MAX_SPEED;
    int dx = (int)cursor_frac_x, dy = (int)cursor_frac_y;
    cursor_frac_x -= dx;
    cursor_frac_y -= dy;
    if (dx || dy) {
        int nx = virtual_mouse_x + dx, ny = virtual_mouse_y + dy;
        if (nx < 0) nx = 0;
        if (nx >= nsfb->width) nx = nsfb->width - 1;
        if (ny < 0) ny = 0;
        if (ny >= nsfb->height) ny = nsfb->height - 1;
        if (nx != virtual_mouse_x || ny != virtual_mouse_y) {
            virtual_mouse_x = nx;
            virtual_mouse_y = ny;
            evq_push(NSFB_EVENT_MOVE_ABSOLUTE, 0, nx, ny);
        } else {
            cursor_frac_x = cursor_frac_y = 0;
        }
    }

    /* Stick derecho (vertical): rueda del mouse, mas eventos cuanto mas inclinado */
    float sy = stick_curve(last_pad.ANA_R_V);
    if (sy == 0.0f) {
        scroll_acc = 0;
    } else {
        scroll_acc += sy * SCROLL_MAX_RATE;
        if (scroll_acc >= 1.0f || scroll_acc <= -1.0f) {
            int code = scroll_acc > 0 ? NSFB_KEY_MOUSE_5 : NSFB_KEY_MOUSE_4;
            scroll_acc += scroll_acc > 0 ? -1.0f : 1.0f;
            evq_push(NSFB_EVENT_KEY_DOWN, code, 0, 0);
            evq_push(NSFB_EVENT_KEY_UP, code, 0, 0);
        }
    }
}

static bool ps3_surface_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
    sysUtilCheckCallback();
    ps3_present();

    if (!pad_initialized) return false;

    if (evq_len == 0) {
        unsigned long long now = now_ms();

        /* Si NetSurf no tiene nada urgente, dormir hasta la proxima lectura del
         * mando (o hasta su timeout) en vez de girar al 100% de CPU. */
        if (timeout != 0 && now < next_poll_ms) {
            unsigned long long wait = next_poll_ms - now;
            if (timeout > 0 && (unsigned long long)timeout < wait)
                wait = timeout;
            usleep(wait * 1000);
            now = now_ms();
        }
        if (now < next_poll_ms)
            return false;
        next_poll_ms = now + PAD_POLL_MS;
        ps3_poll_pad(nsfb);
        if (evq_len == 0)
            return false;
    }

    *event = evq[evq_head];
    evq_head = (evq_head + 1) % (sizeof(evq) / sizeof(evq[0]));
    evq_len--;
    return true;
}

static nsfb_surface_rtns_t ps3_rt_surface = {
    .defaults = ps3_surface_defaults,
    .initialise = ps3_surface_initialise,
    .finalise = ps3_surface_finalise,
    .geometry = ps3_surface_geometry,
    .parameters = NULL,
    .input = ps3_surface_input,
    .claim = ps3_surface_claim,
    .update = ps3_surface_update,
    .cursor = ps3_surface_cursor,
};

/* NSFB_SURFACE_PS3 lo agrega tweak-sources.sh al enum de libnsfb.h. Antes habia
 * un #ifndef que, al ser un enum y no una macro, registraba esta superficie como
 * RAM: las imagenes y miniaturas recibian la pantalla real y al liberarlas se
 * apagaban el RSX y el mando. */
NSFB_SURFACE_DEF(ps3, NSFB_SURFACE_PS3, &ps3_rt_surface)
