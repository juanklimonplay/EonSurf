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
#include "eonsurf_fsm.h"

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
static char page_body[4096] = "Escribe cualquier termino o presiona los botones de marcadores superiores para navegar en vivo.";
static int net_initialized = 0;

/* Enlaces extraidos dinamicamente de la pagina actual */
static eon_page_link_t page_links[EONSURF_MAX_LINKS];
static size_t page_links_count = 0;

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

/* Renderizado con ajuste de linea por palabras (Word-Wrap) */
static void text_wrapped(int start_x, int start_y, int scale, int max_width, int line_spacing, int max_lines, const char *t, u32 c)
{
    int cur_x = start_x;
    int cur_y = start_y;
    int lines_drawn = 0;
    int char_w = 6 * scale;
    const char *p = t;

    while (*p && lines_drawn < max_lines) {
        /* Ignorar espacios iniciales de linea */
        while (*p == ' ') p++;
        if (!*p) break;

        /* Buscar la siguiente palabra */
        const char *w_start = p;
        while (*p && *p != ' ') p++;
        int w_len = p - w_start;
        int w_pix = w_len * char_w;

        /* Si la palabra no cabe en la linea actual, saltar a la siguiente linea */
        if (cur_x + w_pix > start_x + max_width && cur_x > start_x) {
            cur_x = start_x;
            cur_y += line_spacing;
            lines_drawn++;
            if (lines_drawn >= max_lines) break;
        }

        /* Dibujar palabra caracter por caracter */
        for (int i = 0; i < w_len; i++) {
            int ch = w_start[i];
            if (ch >= 'a' && ch <= 'z') ch -= 32;
            if (ch < 32 || ch > 90) ch = 32;
            for (int col = 0; col < 5; col++) {
                for (int row = 0; row < 7; row++) {
                    if (F[ch - 32][col] & (1 << row)) {
                        rect(cur_x + col * scale, cur_y + row * scale, scale, scale, c);
                    }
                }
            }
            cur_x += char_w;
        }

        /* Espacio despues de la palabra */
        cur_x += char_w;
    }
}

/* Estructura para capturar respuesta HTTP/HTTPS via libcurl */
struct HttpMemoryBuffer {
    char *data;
    size_t size;
};

static size_t http_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    struct HttpMemoryBuffer *mem = (struct HttpMemoryBuffer *)userp;

    /* Aceptamos hasta 256 KB en memoria XDR para el cuerpo de la página */
    if (mem->size < 256 * 1024) {
        size_t to_copy = total;
        if (mem->size + to_copy > 256 * 1024) {
            to_copy = (256 * 1024) - mem->size;
        }

        char *ptr = realloc(mem->data, mem->size + to_copy + 1);
        if (ptr) {
            mem->data = ptr;
            memcpy(&(mem->data[mem->size]), contents, to_copy);
            mem->size += to_copy;
            mem->data[mem->size] = 0;
        }
    }

    /* SIEMPRE devolver 'total' para que libcurl sepa que los bytes fueron procesados */
    return total;
}

/* Navegación y descarga web real por HTTPS */
static void navigate_to(const char *input_url)
{
    char target_url[512] = "";

    /* Si el usuario ingresa una busqueda (ej: "juegos ps3"), redirigir a DuckDuckGo Lite */
    if (strncmp(input_url, "http://", 7) != 0 && strncmp(input_url, "https://", 8) != 0) {
        char query_escaped[384] = "";
        size_t qidx = 0;
        for (size_t k = 0; input_url[k] != '\0' && qidx + 2 < sizeof(query_escaped); k++) {
            if (input_url[k] == ' ') query_escaped[qidx++] = '+';
            else query_escaped[qidx++] = input_url[k];
        }
        query_escaped[qidx] = '\0';
        snprintf(target_url, sizeof(target_url), "https://lite.duckduckgo.com/lite/?q=%s", query_escaped);
    } else {
        strncpy(target_url, input_url, sizeof(target_url) - 1);
        target_url[sizeof(target_url) - 1] = '\0';
    }

    strncpy(current_url, target_url, sizeof(current_url) - 1);
    current_url[sizeof(current_url) - 1] = '\0';

    snprintf(page_title, sizeof(page_title), "Conectando: %s", target_url);
    snprintf(page_body, sizeof(page_body), "Descargando contenido web seguro mediante libcurl + mbedTLS...");
    page_links_count = 0;

    if (!net_initialized) {
        snprintf(page_title, sizeof(page_title), "Modo Sin Conexion: %s", target_url);
        snprintf(page_body, sizeof(page_body), "La red de PS3 no esta inicializada. Conecta la consola a Internet.");
        return;
    }

    CURL *curl = curl_easy_init();
    if (curl) {
        struct HttpMemoryBuffer chunk = {0};
        chunk.data = malloc(1);
        chunk.size = 0;

        curl_easy_setopt(curl, CURLOPT_URL, target_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK && chunk.size > 0) {
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            snprintf(page_title, sizeof(page_title), "HTTP %ld: %s", response_code, target_url);

            /* Filtrar con el automata FSM eliminando JS, estilos y extrayendo enlaces reales */
            char clean_body[3800] = "";
            eonsurf_fsm_extract_text_and_links(chunk.data, chunk.size, clean_body, sizeof(clean_body), page_links, &page_links_count);

            if (strlen(clean_body) > 0) {
                snprintf(page_body, sizeof(page_body), "%s", clean_body);
            } else {
                snprintf(page_body, sizeof(page_body), "Pagina recibida (Tamano: %u bytes). Contenido sin texto directo.", (unsigned int)chunk.size);
            }
        } else {
            snprintf(page_title, sizeof(page_title), "Error de conexion (%d)", (int)res);
            snprintf(page_body, sizeof(page_body), "No se pudo obtener respuesta de %s: %s", target_url, curl_easy_strerror(res));
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
                if (eonsurf_abrir_teclado("Buscar o ingresar URL", current_url, url_ingresada, sizeof(url_ingresada)) == 0) {
                    if (strlen(url_ingresada) > 0) {
                        navigate_to(url_ingresada);
                    }
                }
            }
            prev_btn_triangle = pdata.BTN_TRIANGLE;

            /* Clic con Cruz (Enter/K) en la barra de URL, marcadores o links */
            if (pdata.BTN_CROSS && !prev_btn_cross) {
                /* Clic en Barra de URL / Busqueda */
                if (py >= 10 && py <= 50) {
                    char url_ingresada[512] = "";
                    if (eonsurf_abrir_teclado("Buscar o ingresar URL", current_url, url_ingresada, sizeof(url_ingresada)) == 0) {
                        if (strlen(url_ingresada) > 0) {
                            navigate_to(url_ingresada);
                        }
                    }
                }
                /* Clic en Marcadores Superiores */
                else if (py >= 65 && py <= 115) {
                    if (px >= 60 && px <= 330) {
                        navigate_to("https://es.wikipedia.org/wiki/PlayStation_3");
                    } else if (px >= 350 && px <= 620) {
                        navigate_to("https://lite.duckduckgo.com/lite/?q=playstation+3+homebrew");
                    } else if (px >= 640 && px <= 910) {
                        navigate_to("https://raw.githubusercontent.com/aldostools/webMAN-MOD/master/README.md");
                    }
                }
                /* Clic en Enlaces dinamicos descubiertos en la pagina */
                else if (py >= H - 130 && py <= H - 50 && page_links_count > 0) {
                    for (size_t k = 0; k < page_links_count && k < 6; k++) {
                        int col = k % 3;
                        int row = k / 3;
                        int btn_x = 60 + col * 380;
                        int btn_y = (H - 125) + row * 38;
                        if (px >= btn_x && px <= btn_x + 360 && py >= btn_y && py <= btn_y + 34) {
                            navigate_to(page_links[k].url);
                            break;
                        }
                    }
                }
            }
            prev_btn_cross = pdata.BTN_CROSS;

            /* Solo salir si se pulsa SELECT + START simultaneamente */
            if (pdata.BTN_START && pdata.BTN_SELECT) {
                running = 0;
            }
        }

        /* 1. Fondo elegante oscuro */
        rect(0, 0, W, H, 0x000b132b);

        /* 2. Barra de herramientas superior moderna */
        rect(0, 0, W, 58, 0x001c2541);
        rect(0, 58, W, 2, 0x0048cae4);

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

        /* 4. Marcadores Rapidos */
        rect_rounded(60, 72, 270, 38, 5, 0x001c2541);
        text(75, 84, 2, "1. Wikipedia (PS3)", 0x00ffd166);

        rect_rounded(350, 72, 270, 38, 5, 0x001c2541);
        text(365, 84, 2, "2. Buscar: PS3 Scene", 0x0048cae4);

        rect_rounded(640, 72, 270, 38, 5, 0x001c2541);
        text(655, 84, 2, "3. webMAN-MOD Portal", 0x0006d6a0);

        /* 5. Area de contenido web (Tarjeta visual estilo navegador moderno) */
        int content_h = (page_links_count > 0) ? (H - 265) : (H - 180);
        rect_rounded(40, 125, W - 80, content_h, 8, 0x001c2541);
        rect_rounded(40, 125, W - 80, 42, 8, 0x0024325a);
        text(60, 137, 2, page_title, 0x00ffffff);

        /* Cuerpo del contenido web renderizado con ajuste de linea multilineal */
        text_wrapped(60, 180, 2, W - 120, 22, 11, page_body, 0x0090e0ef);

        /* 6. Panel de Navegacion por Enlaces Clicables de la Web */
        if (page_links_count > 0) {
            rect_rounded(40, H - 135, W - 80, 95, 8, 0x00162032);
            text(55, H - 130, 1, "ENLACES DISPONIBLES EN LA PAGINA (Haz clic con [X] para navegar):", 0x00ffd166);

            for (size_t k = 0; k < page_links_count && k < 6; k++) {
                int col = k % 3;
                int row = k / 3;
                int btn_x = 60 + col * 380;
                int btn_y = (H - 110) + row * 34;

                /* Resaltar botón si el cursor está encima */
                int hovered = (px >= btn_x && px <= btn_x + 360 && py >= btn_y && py <= btn_y + 30);
                rect_rounded(btn_x, btn_y, 360, 30, 4, hovered ? 0x0000b4d8 : 0x0024325a);
                text(btn_x + 10, btn_y + 8, 2, page_links[k].text, hovered ? 0x000b132b : 0x00caf0f8);
            }
        }

        /* 7. Barra de estado inferior */
        rect(0, H - 35, W, 35, 0x000f172a);
        text(20, H - 24, 2, "[/_\\] Buscar / URL  |  [X] Clic en Enlace  |  [SELECT+START] Salir", 0x00cbd5e1);

        snprintf(buf, sizeof(buf), "Cursor: %d, %d", px, py);
        text(W - 180, H - 24, 2, buf, 0x0038bdf8);

        /* 8. Cursor en forma de flecha estilizada */
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
