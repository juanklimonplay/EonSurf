/* EonSurf - hola mundo nativo para PS3 (PSL1GHT: RSX + libio). Sin SDL.
 * Dibuja texto en el framebuffer y muestra el estado del DualShock 3.
 * Basado en el patron de los ejemplos de PSL1GHT (samples/graphics/blitting). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <io/pad.h>
#include <rsx/rsx.h>

#include "font5x7.h"

#define CB_SIZE   0x100000
#define HOST_SIZE (32 * 1024 * 1024)

SYS_PROCESS_PARAM(1001, 0x100000);

static gcmContextData *ctx;
static u32 W, H, pitch;
static u32 *fb[2];
static u32 fb_off[2];
static int cur, first = 1, running = 1;

static void on_sysutil(u64 status, u64 param, void *ud)
{
    (void)param; (void)ud;
    if (status == SYSUTIL_EXIT_GAME) running = 0;
}

static void init_screen(void)
{
    void *host = memalign(1024 * 1024, HOST_SIZE);
    printf("EonSurf: host=%p\n", host);
    rsxInit(&ctx, CB_SIZE, HOST_SIZE, host);
    printf("EonSurf: rsxInit ctx=%p\n", (void *)ctx);

    videoState st; videoResolution res; videoConfiguration vc;
    videoGetState(0, 0, &st);
    videoGetResolution(st.displayMode.resolution, &res);
    memset(&vc, 0, sizeof vc);
    vc.resolution = st.displayMode.resolution;
    vc.format = VIDEO_BUFFER_FORMAT_XRGB;
    vc.pitch = res.width * sizeof(u32);
    videoConfigure(0, &vc, NULL, 0);
    videoGetState(0, 0, &st);
    gcmSetFlipMode(GCM_FLIP_VSYNC);

    W = res.width; H = res.height; pitch = W * sizeof(u32);
    printf("EonSurf: video %ux%u\n", W, H);
    for (int i = 0; i < 2; i++) {
        fb[i] = rsxMemalign(64, H * pitch);
        rsxAddressToOffset(fb[i], &fb_off[i]);
        gcmSetDisplayBuffer(i, fb_off[i], pitch, W, H);
        printf("EonSurf: fb[%d]=%p off=%u\n", i, (void *)fb[i], fb_off[i]);
    }
}

static void flip(void)
{
    if (!first) { while (gcmGetFlipStatus() != 0) usleep(200); }
    gcmResetFlipStatus();
    gcmSetFlip(ctx, cur);
    rsxFlushBuffer(ctx);
    gcmSetWaitFlip(ctx);
    cur ^= 1;
    first = 0;
}

static void rect(int x, int y, int w, int h, u32 c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)W) w = W - x;
    if (y + h > (int)H) h = H - y;
    for (int j = 0; j < h; j++) {
        u32 *p = fb[cur] + (y + j) * W + x;
        for (int i = 0; i < w; i++) p[i] = c;
    }
}

static void text(int x, int y, int s, const char *t, u32 c)
{
    for (; *t; t++, x += 6 * s) {
        int ch = *t; if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch < 32 || ch > 90) ch = 32;
        for (int i = 0; i < 5; i++)
            for (int j = 0; j < 7; j++)
                if (F[ch - 32][i] & (1 << j)) rect(x + i * s, y + j * s, s, s, c);
    }
}

int main(void)
{
    padInfo pinfo; padData pdata;
    int px = 640, py = 460;
    char buf[128];

    printf("EonSurf: hola mundo nativo arrancando\n");
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, on_sysutil, NULL);
    init_screen();
    ioPadInit(7);
    printf("EonSurf: init listo\n");

    while (running) {
        int ax = 128, ay = 128, have = 0;
        sysUtilCheckCallback();
        ioPadGetInfo(&pinfo);
        if (pinfo.status[0]) {
            ioPadGetData(0, &pdata);
            have = 1; ax = pdata.ANA_L_H; ay = pdata.ANA_L_V;
            px += (ax - 128) / 16; py += (ay - 128) / 16;
            if (px < 0) px = 0;
            if (px > (int)W - 16) px = W - 16;
            if (py < 0) py = 0;
            if (py > (int)H - 16) py = H - 16;
        }
        rect(0, 0, W, H, 0x00102030);
        text(60, 60, 8, "EonSurf", 0x00ffffff);
        text(60, 150, 4, "Hola mundo - PS3 nativo", 0x00ffffff);
        snprintf(buf, sizeof buf, "Mando: %s", have ? "SI" : "NO");
        text(60, 230, 3, buf, 0x00ffffff);
        snprintf(buf, sizeof buf, "Palanca izq: %d %d", ax, ay);
        text(60, 280, 3, buf, 0x00ffffff);
        if (have) {
            snprintf(buf, sizeof buf, "X:%d O:%d TRI:%d CUA:%d L1:%d R1:%d L2:%d R2:%d START:%d",
                     pdata.BTN_CROSS, pdata.BTN_CIRCLE, pdata.BTN_TRIANGLE, pdata.BTN_SQUARE,
                     pdata.BTN_L1, pdata.BTN_R1, pdata.BTN_L2, pdata.BTN_R2, pdata.BTN_START);
            text(60, 330, 2, buf, 0x00ffffff);
        }
        rect(px, py, 16, 16, 0x00ffb400);
        flip();
    }
    printf("EonSurf: saliendo\n");
    gcmSetWaitFlip(ctx);
    rsxFinish(ctx, 1);
    ioPadEnd();
    return 0;
}
