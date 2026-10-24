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
#include "eonsurf_layout.h"
#include "eonsurf_md_clean.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"
#include "eonsurf_interactive_media.h"

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
static char page_body[8192] = "PlayStation 3 (oficialmente abreviada como PS3) es la tercera videoconsola del modelo PlayStation de Sony Computer Entertainment.\n\nForma parte de las videoconsolas de séptima generación y compitió contra la Xbox 360 de Microsoft y la Wii de Nintendo. Su procesador central es el Cell Broadband Engine diseñado conjuntamente por Sony, Toshiba e IBM.";
static int net_initialized = 0;

/* Motor de Maquetación Estructurada 70/30 (eonsurf_layout.h) */
static EonPageLayout g_layout;

/* Desplazamiento y Scroll */
static int scroll_y = 0;
static int max_scroll_y = 0;

/* Enlaces extraidos dinamicamente de la pagina actual */
static eon_page_link_t page_links[EONSURF_MAX_LINKS];

static size_t page_links_count = 0;

/* Decodificación de imágenes vía stb_image para el RSX en Big-Endian */
static uint32_t* eonsurf_decode_image_argb(const uint8_t *buffer, int len, int *out_w, int *out_h) {
    int w, h, channels;
    uint8_t *raw = stbi_load_from_memory(buffer, len, &w, &h, &channels, 4);
    if (!raw) return NULL;

    uint32_t *argb = (uint32_t *)malloc(w * h * sizeof(uint32_t));
    if (!argb) { stbi_image_free(raw); return NULL; }

    /* Empaquetado directo a 0xAARRGGBB nativo del RSX en Big-Endian */
    for (int i = 0; i < w * h; i++) {
        uint8_t r = raw[i * 4 + 0];
        uint8_t g = raw[i * 4 + 1];
        uint8_t b = raw[i * 4 + 2];
        uint8_t a = raw[i * 4 + 3];
        argb[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }

    stbi_image_free(raw);
    *out_w = w;
    *out_h = h;
    return argb;
}

/* Inicializa la infobox y genera la textura del thumbnail de la PS3 */
static void init_default_layout(void) {
    memset(&g_layout, 0, sizeof(EonPageLayout));
    strncpy(g_layout.title, "PlayStation 3", sizeof(g_layout.title) - 1);
    strncpy(g_layout.subtitle, "Consola de videojuegos de Sony", sizeof(g_layout.subtitle) - 1);
    
    g_layout.info_row_count = 5;
    strncpy(g_layout.info_rows[0].key, "Fabricante", sizeof(g_layout.info_rows[0].key) - 1);
    strncpy(g_layout.info_rows[0].value, "Sony Computer Ent.", sizeof(g_layout.info_rows[0].value) - 1);

    strncpy(g_layout.info_rows[1].key, "Lanzamiento", sizeof(g_layout.info_rows[1].key) - 1);
    strncpy(g_layout.info_rows[1].value, "11 de nov. de 2006", sizeof(g_layout.info_rows[1].value) - 1);

    strncpy(g_layout.info_rows[2].key, "CPU", sizeof(g_layout.info_rows[2].key) - 1);
    strncpy(g_layout.info_rows[2].value, "Cell Broadband Engine 3.2GHz", sizeof(g_layout.info_rows[2].value) - 1);

    strncpy(g_layout.info_rows[3].key, "GPU", sizeof(g_layout.info_rows[3].key) - 1);
    strncpy(g_layout.info_rows[3].value, "RSX 'Reality Synthesizer' 500MHz", sizeof(g_layout.info_rows[3].value) - 1);

    strncpy(g_layout.info_rows[4].key, "Ventas", sizeof(g_layout.info_rows[4].key) - 1);
    strncpy(g_layout.info_rows[4].value, "87.4 millones de unidades", sizeof(g_layout.info_rows[4].value) - 1);

    /* Generar textura de 120x80 para la miniatura fotográfica de la consola */
    g_layout.img_w = 120;
    g_layout.img_h = 80;
    g_layout.infobox_rgba_tex = (uint32_t *)malloc(120 * 80 * sizeof(uint32_t));
    if (g_layout.infobox_rgba_tex) {
        for (int y = 0; y < 80; y++) {
            for (int x = 0; x < 120; x++) {
                uint32_t color;
                /* Silueta de la PS3 Fat con acabado piano black y reflejo cian */
                if (y >= 25 && y <= 55 && x >= 15 && x <= 105) {
                    if (y == 25 || y == 55 || x == 15 || x == 105) {
                        color = 0xFF38BDF8; /* Contorno cromado de la bahía */
                    } else if (y == 40 && x >= 30 && x <= 90) {
                        color = 0xFFE2E8F0; /* Ranura Slot-in de Blu-Ray */
                    } else {
                        color = 0xFF0F172A; /* Piano black brillante */
                    }
                } else {
                    /* Gradiente de fondo del estudio fotográfico */
                    uint8_t grad = (uint8_t)(15 + (y * 20) / 80);
                    color = 0xFF000000 | (grad << 16) | ((grad + 5) << 8) | (grad + 15);
                }
                g_layout.infobox_rgba_tex[y * 120 + x] = color;
            }
        }
    }
}

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

/* Renderizado con tipografía HD 8x16 de NetSurf con decodificación UTF-8 real (tildes, Ñ, símbolos) */
static void text_hd(int x, int y, int s, const char *t, u32 c)
{
    const char *p = t;
    while (*p) {
        uint32_t cp = utf8_next_codepoint(&p);
        if (cp == 0) break;
        const uint8_t *glyph = font8x16_get_glyph(cp);

        for (int row = 0; row < 16; row++) {
            uint8_t row_bits = glyph[row];
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
        x += 9 * s;
    }
}

/* Renderizado de párrafos UTF-8 con soporte de enlaces coloreados (\x01 / \x02) y scroll */
/* Dibuja texto con recorte estricto para evitar desbordes en tarjetas */
static void draw_text_clipped(int x, int y, int s, const char *text, u32 c, int max_right_x) {
    int cur_x = x;
    int char_w = 9 * s;
    const char *p = text;

    while (*p) {
        if (cur_x + (char_w * 3) >= max_right_x && *(p + 1) != '\0') {
            text_hd(cur_x, y, s, "...", c);
            break;
        }
        char single[8] = "";
        const char *next_p = p;
        uint32_t cp = utf8_next_codepoint(&next_p);
        if (cp == 0) break;
        size_t len = next_p - p;
        if (len < sizeof(single)) {
            memcpy(single, p, len);
            single[len] = '\0';
            text_hd(cur_x, y, s, single, c);
        }
        cur_x += char_w;
        p = next_p;
    }
}

static int text_wrapped_hd(int start_x, int start_y, int scale, int max_width, int line_spacing, int clip_top, int clip_bottom, int scroll_offset, const char *t, u32 default_color)
{
    int cur_x = start_x;
    int cur_y = start_y - scroll_offset;
    int char_w = 9 * scale;
    int total_height = 0;
    const char *p = t;
    u32 current_color = default_color;

    while (*p) {
        while (*p == ' ' || *p == '\n' || *p == '\x01' || *p == '\x02') {
            if (*p == '\n') {
                cur_x = start_x;
                cur_y += line_spacing;
            } else if (*p == '\x01') {
                current_color = 0x002563eb; /* Azul enlace vibrante */
            } else if (*p == '\x02') {
                current_color = default_color; /* Color de texto normal */
            }
            p++;
        }
        if (!*p) break;

        /* Medir longitud de la palabra ignorando marcas de control */
        /* const char *w_start = p; */
        int word_chars = 0;
        const char *temp_p = p;
        while (*temp_p && *temp_p != ' ' && *temp_p != '\n') {
            if (*temp_p == '\x01' || *temp_p == '\x02') {
                temp_p++;
                continue;
            }
            utf8_next_codepoint(&temp_p);
            word_chars++;
        }
        int w_pix = word_chars * char_w;

        if (cur_x + w_pix > start_x + max_width && cur_x > start_x) {
            cur_x = start_x;
            cur_y += line_spacing;
        }

        /* Renderizar palabra */
        if (cur_y + 16 * scale >= clip_top && cur_y <= clip_bottom) {
            while (p < temp_p) {
                if (*p == '\x01') {
                    current_color = 0x002563eb;
                    p++;
                    continue;
                }
                if (*p == '\x02') {
                    current_color = default_color;
                    p++;
                    continue;
                }
                uint32_t cp = utf8_next_codepoint(&p);
                const uint8_t *glyph = font8x16_get_glyph(cp);

                for (int row = 0; row < 16; row++) {
                    uint8_t row_bits = glyph[row];
                    if (!row_bits) continue;
                    for (int col = 0; col < 8; col++) {
                        if (row_bits & (1 << (7 - col))) {
                            int px = cur_x + col * scale;
                            int py = cur_y + row * scale;
                            if (px >= start_x && px < start_x + max_width && py >= clip_top && py <= clip_bottom) {
                                fb[cur][py * W + px] = current_color;
                            }
                        }
                    }
                }
                cur_x += char_w;
            }
        } else {
            p = temp_p;
            cur_x += w_pix;
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

            char raw_extracted[7800] = "";
            eonsurf_fsm_extract_text_and_links(chunk.data, chunk.size, raw_extracted, sizeof(raw_extracted), page_links, &page_links_count);

            if (strlen(raw_extracted) > 0) {
                /* Formatear Markdown limpio inyectando saltos en ## y marcas de enlace */
                eonsurf_format_markdown_and_links(raw_extracted, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
                
                /* Extraer la primera imagen de la página y descargarla para el Infobox */
                char detected_img_url[256] = "";
                if (eonsurf_find_first_image_url(chunk.data, detected_img_url, sizeof(detected_img_url))) {
                    /* Descargar con timeout corto y convertir a ARGB nativo */
                    if (eonsurf_fetch_and_decode_infobox_image(detected_img_url)) {
                        /* Sincronizar textura con el layout */
                        g_layout.infobox_rgba_tex = g_infobox_tex;
                        g_layout.img_w = g_infobox_w;
                        g_layout.img_h = g_infobox_h;
                    }
                }
            } else {
                snprintf(page_body, sizeof(page_body), "Página recibida (Tamaño: %u bytes).", (unsigned int)chunk.size);
            }

            /* Actualizar título del documento en la Infobox 70/30 */
            if (strstr(target_url, "PlayStation_3") || strstr(target_url, "playstation")) {
                strncpy(g_layout.title, "PlayStation 3", sizeof(g_layout.title) - 1);
                strncpy(g_layout.subtitle, "Consola de videojuegos de Sony", sizeof(g_layout.subtitle) - 1);
            } else if (strstr(target_url, "duckduckgo")) {
                strncpy(g_layout.title, "DuckDuckGo Search", sizeof(g_layout.title) - 1);
                strncpy(g_layout.subtitle, "Resultados web en vivo", sizeof(g_layout.subtitle) - 1);
            } else {
                strncpy(g_layout.title, "Web Document", sizeof(g_layout.title) - 1);
                strncpy(g_layout.subtitle, target_url, sizeof(g_layout.subtitle) - 1);
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
    init_default_layout();
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
                /* 1. Verificar si hizo clic sobre un enlace en el texto */
                const char *clicked_link = eonsurf_check_link_click(px, py);
                if (clicked_link) {
                    navigate_to(clicked_link);
                }
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

        /* 2. Barra de herramientas superior moderna Titanium Dark */
        rect(0, 0, W, 58, 0x000b0f17);
        rect(0, 58, W, 2, 0x000284c7);

        /* Botones Atrás / Adelante con píldora brillante */
        draw_rounded_pill(fb[cur], W, 20, 12, 36, 36, 6, 0x001e293b, 0x0038bdf8);
        text_hd(32, 22, 1, "<", 0x00ffffff);

        draw_rounded_pill(fb[cur], W, 64, 12, 36, 36, 6, 0x001e293b, 0x0038bdf8);
        text_hd(76, 22, 1, ">", 0x00ffffff);

        /* Barra de Direcciones / Búsqueda con píldora y borde de brillo cian */
        int url_bar_w = W - 440;
        draw_rounded_pill(fb[cur], W, 112, 11, url_bar_w, 38, 8, 0x00030712, 0x000284c7);

        snprintf(buf, sizeof(buf), "%s", current_url);
        draw_text_clipped(126, 22, 1, buf, 0x0038bdf8, 112 + url_bar_w - 15);

        /* Badge oficial Cell B.E. | 60 FPS (Desplazado a la izquierda sin solapamiento) */
        draw_rounded_pill(fb[cur], W, W - 310, 13, 175, 34, 6, 0x000f172a, 0x0006b6d4);
        text_hd(W - 298, 22, 1, "Cell B.E. | 60fps", 0x0038bdf8);

        /* Indicador de red activa Online/Offline */
        draw_rounded_pill(fb[cur], W, W - 120, 13, 98, 34, 6, net_initialized ? 0x00064e3b : 0x00450a0a, net_initialized ? 0x0010b981 : 0x00ef4444);
        text_hd(W - 104, 22, 1, net_initialized ? "ONLINE" : "OFFLINE", 0x00ffffff);

        /* 3. Marcadores Rapidos */
        rect_rounded(60, 70, 270, 36, 5, 0x00242b38);
        text_hd(75, 80, 1, "1. Wikipedia (PS3)", 0x00fbbf24);

        rect_rounded(350, 70, 270, 36, 5, 0x00242b38);
        text_hd(365, 80, 1, "2. Buscar: PS3 Scene", 0x0060a5fa);

        rect_rounded(640, 70, 270, 36, 5, 0x00242b38);
        text_hd(655, 80, 1, "3. webMAN-MOD Portal", 0x0034d399);

        /* 4. MOTOR DE MAQUETACIÓN ESTRUCTURADA 70/30 (Estilo EonSurf UI Mockup) */
        int canvas_x = 40;
        int canvas_y = 120;
        int canvas_w = W - 80;
        int canvas_h = (page_links_count > 0) ? (H - 260) : (H - 175);

        /* Columna Izquierda: 68% del ancho para el Artículo y Texto */
        int col_left_w = (canvas_w * 68) / 100;
        /* Columna Derecha: 30% del ancho para la Tarjeta Infobox */
        int col_right_x = canvas_x + col_left_w + 20;
        int col_right_w = canvas_w - col_left_w - 20;

        /* Sombra y cuerpo del artículo izquierdo */
        rect_rounded(canvas_x - 3, canvas_y - 3, col_left_w + 6, canvas_h + 6, 8, 0x000f131a);
        rect_rounded(canvas_x, canvas_y, col_left_w, canvas_h, 6, 0x00f8fafc);

        /* Cabecera del artículo web */
        rect(canvas_x, canvas_y, col_left_w, 42, 0x00f1f5f9);
        rect(canvas_x, canvas_y + 42, col_left_w, 1, 0x00e2e8f0);
        text_hd(canvas_x + 20, canvas_y + 13, 1, page_title, 0x000f172a);

        /* Contenido del artículo con tipografía Zap 8x16 en la columna izquierda */
        int content_height = text_wrapped_hd(canvas_x + 24, canvas_y + 55, 1, col_left_w - 48, 22,
                                             canvas_y + 45, canvas_y + canvas_h - 15, scroll_y, page_body, 0x001e293b);
        max_scroll_y = (content_height > canvas_h - 60) ? (content_height - (canvas_h - 60)) : 0;

        /* Barra de scroll vertical de la columna izquierda */
        if (max_scroll_y > 0) {
            int scrollbar_track_h = canvas_h - 60;
            int thumb_h = (scrollbar_track_h * (canvas_h - 60)) / content_height;
            if (thumb_h < 25) thumb_h = 25;
            int thumb_y = canvas_y + 50 + (scroll_y * (scrollbar_track_h - thumb_h)) / max_scroll_y;

            rect(canvas_x + col_left_w - 10, canvas_y + 50, 6, scrollbar_track_h, 0x00e2e8f0);
            rect_rounded(canvas_x + col_left_w - 10, thumb_y, 6, thumb_h, 3, 0x003b82f6);
        }

        /* --- COLUMNA DERECHA: TARJETA INFOBOX WIKIPEDIA (30%) --- */
        rect_rounded(col_right_x - 3, canvas_y - 3, col_right_w + 6, canvas_h + 6, 8, 0x000f131a);
        rect_rounded(col_right_x, canvas_y, col_right_w, canvas_h, 6, 0x00ffffff);

        /* Cabecera de la Infobox */
        rect(col_right_x, canvas_y, col_right_w, 38, 0x000ea5e9);
        text_hd(col_right_x + 16, canvas_y + 11, 1, g_layout.title, 0x00ffffff);

        /* Área de imagen fotográfica de la Infobox con textura nativa */
        int img_box_y = canvas_y + 48;
        int img_box_h = 130;
        rect_rounded(col_right_x + 14, img_box_y, col_right_w - 28, img_box_h, 6, 0x000b0f17);

        /* Si tenemos textura en g_layout.infobox_rgba_tex, dibujarla escalada con el blitter de punto fijo */
        if (g_layout.infobox_rgba_tex && g_layout.img_w > 0 && g_layout.img_h > 0) {
            int box_x = col_right_x + 18;
            int box_y = img_box_y + 4;
            int box_w = col_right_w - 36;
            int box_h = img_box_h - 8;
            eonsurf_blit_infobox_image(fb[cur], W, g_layout.infobox_rgba_tex, g_layout.img_w, g_layout.img_h,
                                       box_x, box_y, box_w, box_h);
        } else {
            rect(col_right_x + 20, img_box_y + 6, col_right_w - 40, img_box_h - 12, 0x001e293b);
            text_hd(col_right_x + (col_right_w / 2) - 64, img_box_y + 48, 1, "[ FOTO CONSOLA ]", 0x0038bdf8);
        }
        char sub_buf[32];
        strncpy(sub_buf, g_layout.subtitle, sizeof(sub_buf) - 1);
        sub_buf[sizeof(sub_buf) - 1] = '\0';
        text_hd(col_right_x + 20, img_box_y + img_box_h + 8, 1, sub_buf, 0x0064748b);

        /* Tabla de especificaciones clave-valor */
        int table_start_y = img_box_y + img_box_h + 32;
        rect(col_right_x + 14, table_start_y - 4, col_right_w - 28, 1, 0x00cbd5e1);

        for (uint32_t r = 0; r < g_layout.info_row_count; r++) {
            int row_y = table_start_y + (r * 26);
            if (row_y + 24 > canvas_y + canvas_h) break;

            if (r % 2 == 1) {
                rect(col_right_x + 14, row_y - 2, col_right_w - 28, 24, 0x00f8fafc);
            }
            text_hd(col_right_x + 18, row_y + 3, 1, g_layout.info_rows[r].key, 0x000f172a);
            draw_text_clipped(col_right_x + 115, row_y + 3, 1, g_layout.info_rows[r].value, 0x002563eb, col_right_x + col_right_w - 16);
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

        /* 6. Barra de estado inferior translúcida con botones PS3 estilizados */
        rect(0, H - 36, W, 36, 0x000b0f17);
        rect(0, H - 36, W, 1, 0x001e293b);

        /* Iconos de botones estilo PlayStation */
        text_hd(24, H - 24, 1, "/_\\ Buscar / URL", 0x0010b981); /* Triángulo Verde */
        text_hd(190, H - 24, 1, "X Abrir Enlace", 0x0038bdf8);   /* Cruz Azul */
        text_hd(350, H - 24, 1, "O Volver Atras", 0x00f43f5e);   /* Círculo Rojo */
        text_hd(510, H - 24, 1, "[R-Stick / Pad] Desplazar", 0x0094a3b8);
        text_hd(770, H - 24, 1, "[SELECT+START] Salir", 0x0064748b);

        /* Coordenadas en píldora compacta */
        draw_rounded_pill(fb[cur], W, W - 180, H - 30, 160, 24, 4, 0x00030712, 0x001e293b);
        snprintf(buf, sizeof(buf), "RSX: %d, %d", px, py);
        text_hd(W - 165, H - 23, 1, buf, 0x0038bdf8);

        /* 7. Puntero de alta visibilidad Titanio/Cian con borde de sombra negro */
        /* Matriz de 16x16: 0=transparente, 1=borde negro exterior, 2=interior cian brillante, 3=nucleo blanco */
        static const uint8_t cursor_map[16][16] = {
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
            {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
            {1,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
            {1,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0},
            {1,3,3,2,1,0,0,0,0,0,0,0,0,0,0,0},
            {1,3,3,2,2,1,0,0,0,0,0,0,0,0,0,0},
            {1,3,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
            {1,3,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
            {1,3,2,2,2,2,2,2,1,0,0,0,0,0,0,0},
            {1,3,2,2,2,2,2,2,2,1,0,0,0,0,0,0},
            {1,3,2,2,2,2,1,1,1,1,1,0,0,0,0,0},
            {1,3,2,1,2,2,1,0,0,0,0,0,0,0,0,0},
            {1,1,1,0,1,2,2,1,0,0,0,0,0,0,0,0},
            {1,0,0,0,1,2,2,1,0,0,0,0,0,0,0,0},
            {0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,0},
            {0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0}
        };
        for (int row = 0; row < 16; row++) {
            for (int col = 0; col < 16; col++) {
                uint8_t p = cursor_map[row][col];
                if (p == 1) {
                    rect(px + col, py + row, 1, 1, 0x0005080f);
                } else if (p == 2) {
                    rect(px + col, py + row, 1, 1, 0x0006b6d4);
                } else if (p == 3) {
                    rect(px + col, py + row, 1, 1, 0x00ffffff);
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
