/* EonSurf - Navegador Web Nativo para PS3 (PSL1GHT: RSX + NetSurf Engine Graphics).
 * Modern Reader Mode con tipografia HD 8x16 de NetSurf, lienzo web iluminado,
 * scroll interactivo y navegacion por hipertexto real. */
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

#include "font8x16.h"
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

static char current_url[512] = "https://es.wikipedia.org/wiki/PlayStation_3";
static char page_title[256] = "PlayStation 3 - Wikipedia, la enciclopedia libre";
static char page_body[8192] = "Bienvenido a EonSurf para PlayStation 3.\nUsa los marcadores superiores para cargar páginas web completas en vivo.";
static int net_initialized = 0;

/* Desplazamiento y Scroll */
static int scroll_y = 0;
static int max_scroll_y = 0;

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

/* Primitivas de renderizado grafico de alta fidelidad */
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

/* Renderizado con tipografia HD 8x16 de NetSurf con soporte mayúsculas y minúsculas reales */
static void text_hd(int x, int y, int s, const char *t, u32 c)
{
    for (; *t; t++, x += 9 * s) {
        uint8_t ch = (uint8_t)*t;
        if (ch > 127) ch = 32;
        for (int row = 0; row < 16; row++) {
            uint8_t row_bits = font8x16_data[ch][row];
            if (row_bits == 0) continue;
            for (int col = 0; col < 8; col++) {
                if (row_bits & (1 << (7 - col))) {
                    if (s == 1) {
                        int px = x + col;
                        int py = y + row;
                        if (px >= 0 && px < (int)W && py >= 0 && py < (int)H) {
                            fb[cur][py * W + px] = c;
                        }
                    } else {
                        rect(x + col * s, y + row * s, s, s, c);
                    }
                }
            }
        }
    }
}

/* Renderizado de parrafos con ajuste de linea y soporte de scroll vertical */
static int text_wrapped_hd(int start_x, int start_y, int scale, int max_width, int line_spacing, int clip_top, int clip_bottom, int scroll_offset, const char *t, u32 c)
{
    int cur_x = start_x;
    int cur_y = start_y - scroll_offset;
    int char_w = 9 * scale;
    int total_height = 0;
    const char *p = t;

    while (*p) {
        while (*p == ' ' || *p == '\n') {
            if (*p == '\n') {
                cur_x = start_x;
                cur_y += line_spacing;
            }
            p++;
        }
        if (!*p) break;

        const char *w_start = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        int w_len = p - w_start;
        int w_pix = w_len * char_w;

        if (cur_x + w_pix > start_x + max_width && cur_x > start_x) {
            cur_x = start_x;
            cur_y += line_spacing;
        }

        if (cur_y + 16 * scale >= clip_top && cur_y <= clip_bottom) {
            for (int i = 0; i < w_len; i++) {
                uint8_t ch = (uint8_t)w_start[i];
                if (ch > 127) ch = 32;
                for (int row = 0; row < 16; row++) {
                    uint8_t row_bits = font8x16_data[ch][row];
                    if (!row_bits) continue;
                    for (int col = 0; col < 8; col++) {
                        if (row_bits & (1 << (7 - col))) {
                            int px = cur_x + col * scale;
                            int py = cur_y + row * scale;
                            if (px >= start_x && px < start_x + max_width && py >= clip_top && py <= clip_bottom) {
                                fb[cur][py * W + px] = c;
                            }
                        }
                    }
                }
                cur_x += char_w;
            }
        } else {
            cur_x += w_len * char_w;
        }

        cur_x += char_w;
        if (cur_y + line_spacing - (start_y - scroll_offset) > total_height) {
            total_height = cur_y + line_spacing - (start_y - scroll_offset);
        }
    }

    return total_height;
}

/* Captura de respuesta HTTP via libcurl */
struct HttpMemoryBuffer {
    char *data;
    size_t size;
};

static size_t http_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    struct HttpMemoryBuffer *mem = (struct HttpMemoryBuffer *)userp;

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
    return total;
}

static void navigate_to(const char *input_url)
{
    char target_url[512] = "";

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

    snprintf(page_title, sizeof(page_title), "Cargando: %s", target_url);
    snprintf(page_body, sizeof(page_body), "Conectando al servidor web...");
    page_links_count = 0;
    scroll_y = 0;

    if (!net_initialized) {
        snprintf(page_title, sizeof(page_title), "Sin Conexion");
        snprintf(page_body, sizeof(page_body), "La red de PS3 no esta inicializada.");
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

            char clean_body[7800] = "";
            eonsurf_fsm_extract_text_and_links(chunk.data, chunk.size, clean_body, sizeof(clean_body), page_links, &page_links_count);

            if (strlen(clean_body) > 0) {
                snprintf(page_body, sizeof(page_body), "%s", clean_body);
            } else {
                snprintf(page_body, sizeof(page_body), "Pagina recibida (Tamano: %u bytes).", (unsigned int)chunk.size);
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
        int ax = 128, ay = 128;
        sysUtilCheckCallback();
        ioPadGetInfo(&pinfo);
        if (pinfo.status[0]) {
            ioPadGetData(0, &pdata);
            ax = pdata.ANA_L_H; ay = pdata.ANA_L_V;
            px += (ax - 128) / 16; py += (ay - 128) / 16;
            if (px < 0) px = 0;
            if (px > (int)W - 16) px = W - 16;
            if (py < 0) py = 0;
            if (py > (int)H - 16) py = H - 16;

            /* Scroll vertical con el Stick Derecho o Cruceta Arriba/Abajo */
            int ry = pdata.ANA_R_V - 128;
            if (abs(ry) > 16) {
                scroll_y += ry / 6;
            }
            if (pdata.BTN_UP) scroll_y -= 8;
            if (pdata.BTN_DOWN) scroll_y += 8;
            if (scroll_y < 0) scroll_y = 0;
            if (scroll_y > max_scroll_y) scroll_y = max_scroll_y;

            /* Teclado Sony OSK con Triangulo */
            if (pdata.BTN_TRIANGLE && !prev_btn_triangle) {
                char url_ingresada[512] = "";
                if (eonsurf_abrir_teclado("Buscar o ingresar URL", current_url, url_ingresada, sizeof(url_ingresada)) == 0) {
                    if (strlen(url_ingresada) > 0) {
                        navigate_to(url_ingresada);
                    }
                }
            }
            prev_btn_triangle = pdata.BTN_TRIANGLE;

            /* Clic con Cruz [X] */
            if (pdata.BTN_CROSS && !prev_btn_cross) {
                /* Barra de URL superior */
                if (py >= 10 && py <= 50) {
                    char url_ingresada[512] = "";
                    if (eonsurf_abrir_teclado("Buscar o ingresar URL", current_url, url_ingresada, sizeof(url_ingresada)) == 0) {
                        if (strlen(url_ingresada) > 0) {
                            navigate_to(url_ingresada);
                        }
                    }
                }
                /* Marcadores Rapidos */
                else if (py >= 65 && py <= 112) {
                    if (px >= 60 && px <= 330) {
                        navigate_to("https://es.wikipedia.org/wiki/PlayStation_3");
                    } else if (px >= 350 && px <= 620) {
                        navigate_to("https://lite.duckduckgo.com/lite/?q=playstation+3+homebrew");
                    } else if (px >= 640 && px <= 910) {
                        navigate_to("https://raw.githubusercontent.com/aldostools/webMAN-MOD/master/README.md");
                    }
                }
                /* Enlaces dinamicos en el panel de navegacion */
                else if (py >= H - 125 && py <= H - 45 && page_links_count > 0) {
                    for (size_t k = 0; k < page_links_count && k < 6; k++) {
                        int col = k % 3;
                        int row = k / 3;
                        int btn_x = 60 + col * 380;
                        int btn_y = (H - 120) + row * 34;
                        if (px >= btn_x && px <= btn_x + 360 && py >= btn_y && py <= btn_y + 30) {
                            navigate_to(page_links[k].url);
                            break;
                        }
                    }
                }
            }
            prev_btn_cross = pdata.BTN_CROSS;

            if (pdata.BTN_START && pdata.BTN_SELECT) {
                running = 0;
            }
        }

        /* 1. Fondo elegante de escritorio (#1E222D) */
        rect(0, 0, W, H, 0x001e222d);

        /* 2. Barra de herramientas superior moderna */
        rect(0, 0, W, 58, 0x00151821);
        rect(0, 58, W, 2, 0x003b82f6);

        rect_rounded(20, 14, 32, 32, 4, 0x002d3748);
        text_hd(28, 22, 1, "<", 0x00ffffff);

        rect_rounded(60, 14, 32, 32, 4, 0x002d3748);
        text_hd(68, 22, 1, ">", 0x00ffffff);

        /* Barra de Direcciones / Busqueda */
        int url_bar_w = W - 260;
        rect_rounded(105, 12, url_bar_w, 36, 6, 0x000f131a);
        rect(105, 46, url_bar_w, 2, 0x003b82f6);

        snprintf(buf, sizeof(buf), "%s", current_url);
        text_hd(120, 22, 1, buf, 0x0093c5fd);

        /* Indicador de red activa */
        rect_rounded(W - 140, 14, 120, 32, 4, net_initialized ? 0x00059669 : 0x004b5563);
        text_hd(W - 128, 22, 1, net_initialized ? "ONLINE" : "OFFLINE", 0x00ffffff);

        /* 3. Marcadores Rapidos */
        rect_rounded(60, 70, 270, 36, 5, 0x00242b38);
        text_hd(75, 80, 1, "1. Wikipedia (PS3)", 0x00fbbf24);

        rect_rounded(350, 70, 270, 36, 5, 0x00242b38);
        text_hd(365, 80, 1, "2. Buscar: PS3 Scene", 0x0060a5fa);

        rect_rounded(640, 70, 270, 36, 5, 0x00242b38);
        text_hd(655, 80, 1, "3. webMAN-MOD Portal", 0x0034d399);

        /* 4. LIENZO WEB MODERNO ("Reader Canvas" Blanco/Gris Editorial #FFFFFF) */
        int canvas_x = 50;
        int canvas_y = 120;
        int canvas_w = W - 100;
        int canvas_h = (page_links_count > 0) ? (H - 260) : (H - 175);

        /* Sombra exterior y cuerpo de la pagina web */
        rect_rounded(canvas_x - 3, canvas_y - 3, canvas_w + 6, canvas_h + 6, 8, 0x000f131a);
        rect_rounded(canvas_x, canvas_y, canvas_w, canvas_h, 6, 0x00f8fafc);

        /* Cabecera del articulo web */
        rect(canvas_x, canvas_y, canvas_w, 42, 0x00f1f5f9);
        rect(canvas_x, canvas_y + 42, canvas_w, 1, 0x00e2e8f0);
        text_hd(canvas_x + 20, canvas_y + 13, 1, page_title, 0x000f172a);

        /* Contenido web con tipografia HD 8x16 de NetSurf Zap (Color texto editorial: #1E293B) */
        int content_height = text_wrapped_hd(canvas_x + 24, canvas_y + 55, 1, canvas_w - 50, 22,
                                             canvas_y + 45, canvas_y + canvas_h - 15, scroll_y, page_body, 0x001e293b);
        
        /* Calcular limites de scroll */
        max_scroll_y = (content_height > canvas_h - 60) ? (content_height - (canvas_h - 60)) : 0;

        /* Barra de scroll vertical interactiva */
        if (max_scroll_y > 0) {
            int scrollbar_track_h = canvas_h - 60;
            int thumb_h = (scrollbar_track_h * (canvas_h - 60)) / content_height;
            if (thumb_h < 25) thumb_h = 25;
            int thumb_y = canvas_y + 50 + (scroll_y * (scrollbar_track_h - thumb_h)) / max_scroll_y;

            rect(canvas_x + canvas_w - 12, canvas_y + 50, 8, scrollbar_track_h, 0x00e2e8f0);
            rect_rounded(canvas_x + canvas_w - 12, thumb_y, 8, thumb_h, 4, 0x003b82f6);
        }

        /* 5. Panel de Enlaces Hipertextuales */
        if (page_links_count > 0) {
            rect_rounded(canvas_x, H - 130, canvas_w, 90, 6, 0x00151821);
            text_hd(canvas_x + 15, H - 124, 1, "ENLACES DISPONIBLES EN LA PAGINA (Haz clic para navegar):", 0x00fbbf24);

            for (size_t k = 0; k < page_links_count && k < 6; k++) {
                int col = k % 3;
                int row = k / 3;
                int btn_x = canvas_x + 15 + col * ((canvas_w - 40) / 3);
                int btn_y = (H - 102) + row * 32;
                int btn_w = ((canvas_w - 40) / 3) - 10;

                int hovered = (px >= btn_x && px <= btn_x + btn_w && py >= btn_y && py <= btn_y + 28);
                rect_rounded(btn_x, btn_y, btn_w, 28, 4, hovered ? 0x003b82f6 : 0x00242b38);
                text_hd(btn_x + 10, btn_y + 6, 1, page_links[k].text, hovered ? 0x00ffffff : 0x0093c5fd);
            }
        }

        /* 6. Barra de estado inferior */
        rect(0, H - 32, W, 32, 0x000f131a);
        text_hd(20, H - 22, 1, "[/_\\] Buscar / URL   |   [X] Clic   |   [R-Stick / D-Pad] Scroll   |   [SELECT+START] Salir", 0x0094a3b8);

        snprintf(buf, sizeof(buf), "Cursor: %d, %d", px, py);
        text_hd(W - 170, H - 22, 1, buf, 0x0060a5fa);

        /* 7. Cursor en forma de flecha estilizada */
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
                    u32 color = (col == 0 || row == col || row == 0) ? 0x002563eb : 0x00ffffff;
                    rect(px + col, py + row, 1, 1, color);
                }
            }
        }

        flip();
    }

    if (net_initialized) {
        netDeinitialize();
    }
    gcmSetWaitFlip(ctx);
    rsxFinish(ctx, 1);
    ioPadEnd();
    return 0;
}
