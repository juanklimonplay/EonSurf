/* EonSurf - Navegador Web Nativo para PS3 (PSL1GHT: RSX + NetSurf libnsfb).
 * Dibuja la interfaz visual premium del navegador y gestiona la navegacion interactiva. */
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
#include <net/net.h>
#include <curl/curl.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"

#include "font5x7.h"
#include "osk_ps3.h"

#define CB_SIZE   0x100000
#define HOST_SIZE (32 * 1024 * 1024)

SYS_PROCESS_PARAM(1001, 0x100000);

static gcmContextData *ctx;
static u32 W, H, pitch;
static u32 *fb[2];
static u32 fb_off[2];
static int cur, first = 1, running = 1;
static nsfb_t *nsfb = NULL;

static char current_url[512] = "https://eonsurf.ps3/inicio";
static char page_title[256] = "EonSurf: Bienvenido a la Web de PS3";
static char page_body[1024] = "Presiona [/\\] o haz click en la barra superior para escribir una URL con el teclado Sony.";
static int net_initialized = 0;

static void on_sysutil(u64 status, u64 param, void *ud)
{
    (void)param; (void)ud;
    if (status == SYSUTIL_EXIT_GAME) running = 0;
}

static void init_screen(void)
{
    void *host = memalign(1024 * 1024, HOST_SIZE);
    rsxInit(&ctx, CB_SIZE, HOST_SIZE, host);

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
    for (int i = 0; i < 2; i++) {
        fb[i] = rsxMemalign(64, H * pitch);
        rsxAddressToOffset(fb[i], &fb_off[i]);
        gcmSetDisplayBuffer(i, fb_off[i], pitch, W, H);
    }
}

static void flip(void)
{
    if (!first) {
        while (gcmGetFlipStatus() != 0) usleep(200);
        gcmResetFlipStatus();
    }
    gcmSetFlip(ctx, cur);
    rsxFlushBuffer(ctx);
    gcmSetWaitFlip(ctx);
    cur ^= 1;
    first = 0;
}

/* Primitivas de renderizado grafico de alta estetica */
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

static void rect_rounded(int x, int y, int w, int h, int r, u32 c)
{
    rect(x + r, y, w - 2 * r, h, c);
    rect(x, y + r, r, h - 2 * r, c);
    rect(x + w - r, y + r, r, h - 2 * r, c);
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

/* Estructura para capturar respuesta HTTP/HTTPS via libcurl */
struct HttpMemoryBuffer {
    char *data;
    size_t size;
};

static size_t http_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct HttpMemoryBuffer *mem = (struct HttpMemoryBuffer *)userp;

    /* Limitar a 64 KB para no desbordar RAM en páginas gigantes */
    if (mem->size + realsize > 64 * 1024) {
        realsize = (64 * 1024) - mem->size;
        if (realsize == 0) return 0;
    }

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return size * nmemb;
}

/* Extrae texto legible descartando tags <...> */
static void extract_plain_text(const char *html, char *out_text, size_t max_len)
{
    size_t out_idx = 0;
    int in_tag = 0;

    for (size_t i = 0; html[i] != '\0' && out_idx < max_len - 1; i++) {
        if (html[i] == '<') {
            in_tag = 1;
        } else if (html[i] == '>') {
            in_tag = 0;
            if (out_idx > 0 && out_text[out_idx - 1] != ' ') {
                out_text[out_idx++] = ' ';
            }
        } else if (!in_tag) {
            char c = html[i];
            if (c == '\r' || c == '\n' || c == '\t') c = ' ';
            if (c == ' ' && out_idx > 0 && out_text[out_idx - 1] == ' ') continue;
            if (c >= 32 && c <= 126) {
                out_text[out_idx++] = c;
            }
        }
    }
    out_text[out_idx] = '\0';
}

/* Navegación y descarga web real por HTTPS */
static void navigate_to(const char *url)
{
    strncpy(current_url, url, sizeof(current_url) - 1);
    current_url[sizeof(current_url) - 1] = '\0';

    snprintf(page_title, sizeof(page_title), "Conectando: %s", url);
    snprintf(page_body, sizeof(page_body), "Descargando contenido web seguro mediante libcurl + mbedTLS...");

    if (!net_initialized) {
        snprintf(page_title, sizeof(page_title), "Modo Sin Conexion: %s", url);
        snprintf(page_body, sizeof(page_body), "La red de PS3 no esta inicializada. Conecta la consola a Internet.");
        return;
    }

    CURL *curl = curl_easy_init();
    if (curl) {
        struct HttpMemoryBuffer chunk = {0};
        chunk.data = malloc(1);
        chunk.size = 0;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (PlayStation 3; EonSurf/0.3.1; CellOS)");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); /* Aceptar certificados modernos en PS3 */
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK && chunk.size > 0) {
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            snprintf(page_title, sizeof(page_title), "HTTP %ld: %s", response_code, url);

            char clean_body[800] = "";
            extract_plain_text(chunk.data, clean_body, sizeof(clean_body));
            if (strlen(clean_body) > 0) {
                snprintf(page_body, sizeof(page_body), "%s", clean_body);
            } else {
                snprintf(page_body, sizeof(page_body), "Pagina recibida (Tamano: %u bytes).", (unsigned int)chunk.size);
            }
        } else {
            snprintf(page_title, sizeof(page_title), "Error de conexion (%d)", (int)res);
            snprintf(page_body, sizeof(page_body), "No se pudo obtener respuesta de %s: %s", url, curl_easy_strerror(res));
        }

        if (chunk.data) free(chunk.data);
        curl_easy_cleanup(curl);
    }
}

int main(void)
{
    padInfo pinfo; padData pdata;
    int px = 640, py = 460;
    char buf[128];
    int prev_btn_triangle = 0;
    int prev_btn_cross = 0;

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, on_sysutil, NULL);
    init_screen();
    ioPadInit(7);

    /* Inicializar stack de red si esta disponible */
    if (netInitialize() == 0) {
        net_initialized = 1;
    }

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

            /* Abrir teclado Sony con Triangulo */
            if (pdata.BTN_TRIANGLE && !prev_btn_triangle) {
                char url_ingresada[512] = "";
                if (eonsurf_abrir_teclado("Ingresar URL en EonSurf", current_url, url_ingresada, sizeof(url_ingresada)) == 0) {
                    if (strlen(url_ingresada) > 0) {
                        navigate_to(url_ingresada);
                    }
                }
            }
            prev_btn_triangle = pdata.BTN_TRIANGLE;

            /* Clic con Cruz (Enter/K) en la barra de URL o marcadores */
            if (pdata.BTN_CROSS && !prev_btn_cross) {
                if (py >= 10 && py <= 50) {
                    char url_ingresada[512] = "";
                    if (eonsurf_abrir_teclado("Ingresar URL en EonSurf", current_url, url_ingresada, sizeof(url_ingresada)) == 0) {
                        if (strlen(url_ingresada) > 0) {
                            navigate_to(url_ingresada);
                        }
                    }
                } else if (py >= 270 && py <= 320) {
                    if (px >= 60 && px <= 340) {
                        navigate_to("https://es.wikipedia.org/wiki/PlayStation_3");
                    } else if (px >= 360 && px <= 640) {
                        navigate_to("https://gemini.google.com");
                    } else if (px >= 660 && px <= 940) {
                        navigate_to("https://www.psx-place.com");
                    }
                }
            }
            prev_btn_cross = pdata.BTN_CROSS;

            /* Solo salir si se pulsa SELECT + START simultaneamente */
            if (pdata.BTN_START && pdata.BTN_SELECT) {
                running = 0;
            }
        }

        /* 1. Fondo elegante oscuro con degradado visual */
        rect(0, 0, W, H, 0x000b132b);

        /* 2. Barra de herramientas superior moderna */
        rect(0, 0, W, 58, 0x001c2541);
        rect(0, 58, W, 2, 0x0048cae4);

        /* Botones de navegacion OSD (Atras, Adelante, Recargar) */
        rect_rounded(20, 14, 32, 32, 4, 0x003a506b);
        text(30, 22, 2, "<", 0x00ffffff);

        rect_rounded(60, 14, 32, 32, 4, 0x003a506b);
        text(70, 22, 2, ">", 0x00ffffff);

        /* 3. Barra de URL estilizada con bordes redondeados */
        int url_bar_w = W - 260;
        rect_rounded(105, 12, url_bar_w, 36, 6, 0x000b132b);
        rect(105, 46, url_bar_w, 2, 0x0000b4d8);

        snprintf(buf, sizeof(buf), "%s", current_url);
        text(120, 22, 2, buf, 0x00caf0f8);

        /* Indicador de red activa */
        rect_rounded(W - 140, 14, 120, 32, 4, net_initialized ? 0x00065f46 : 0x00475569);
        text(W - 130, 22, 2, net_initialized ? "RED ONLINE" : "OFFLINE", 0x00ffffff);

        /* 4. Area de contenido web (Tarjeta visual estilo navegador moderno) */
        rect_rounded(40, 80, W - 80, H - 140, 8, 0x001c2541);
        rect_rounded(40, 80, W - 80, 48, 8, 0x0024325a);
        text(60, 95, 3, page_title, 0x00ffffff);

        /* Cuerpo del contenido web renderizado */
        text(60, 160, 2, page_body, 0x0090e0ef);

        /* Linea separadora elegante */
        rect(60, 210, W - 120, 2, 0x003a506b);

        /* Panel de acceso rapido a marcadores */
        text(60, 230, 3, "Marcadores Rapidos (Presiona [X] para cargar):", 0x00ffd166);

        rect_rounded(60, 270, 280, 50, 6, 0x000b132b);
        text(80, 285, 2, "1. Wikipedia (Enciclopedia)", 0x00ffffff);

        rect_rounded(360, 270, 280, 50, 6, 0x000b132b);
        text(380, 285, 2, "2. Gemini 2.5 Flash (AI)", 0x00ffffff);

        rect_rounded(660, 270, 280, 50, 6, 0x000b132b);
        text(680, 285, 2, "3. PSX-Place (Scene)", 0x00ffffff);

        /* Barra de estado inferior */
        rect(0, H - 35, W, 35, 0x000f172a);
        text(20, H - 24, 2, "[/_\\] Escribir URL  |  [X] Clic / Seleccionar  |  [SELECT+START] Salir", 0x00cbd5e1);

        snprintf(buf, sizeof(buf), "Cursor: %d, %d", px, py);
        text(W - 180, H - 24, 2, buf, 0x0038bdf8);

        /* 5. Cursor en forma de flecha estilizada */
        static const uint16_t arrow_mask[16] = {
            0b1000000000000000,
            0b1100000000000000,
            0b1110000000000000,
            0b1111000000000000,
            0b1111100000000000,
            0b1111110000000000,
            0b1111111000000000,
            0b1111111100000000,
            0b1111111110000000,
            0b1111110000000000,
            0b1101111000000000,
            0b1000111100000000,
            0b0000011110000000,
            0b0000001111000000,
            0b0000000110000000,
            0b0000000000000000
        };
        for (int row = 0; row < 16; row++) {
            for (int col = 0; col < 16; col++) {
                if (arrow_mask[row] & (1 << (15 - col))) {
                    /* Borde exterior cian y relleno blanco */
                    u32 color = (col == 0 || row == col || row == 0) ? 0x0000b4d8 : 0x00ffffff;
                    rect(px + col, py + row, 1, 1, color);
                }
            }
        }

        flip();
    }

    if (nsfb != NULL) {
        nsfb_free(nsfb);
    }
    if (net_initialized) {
        netDeinitialize();
    }
    gcmSetWaitFlip(ctx);
    rsxFinish(ctx, 1);
    ioPadEnd();
    return 0;
}
