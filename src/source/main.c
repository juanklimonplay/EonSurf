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
#include "eonsurf_osk.h"
#include "eonsurf_fsm.h"
#include "eonsurf_layout.h"
#include "eonsurf_md_clean.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "eonsurf_interactive_media.h"
#include "eonsurf_dynamic_web.h"
#include "eonsurf_history.h"
#include "eonsurf_downloader.h"
#include "eonsurf_tabs.h"
#include "eonsurf_cursor.h"
#include "eonsurf_article_extractor.h"
#include "eonsurf_modern_ui.h"
#include "eonsurf_net_bypass.h"
#include "eonsurf_ps3_ergonomics.h"
#include <sys/time.h>

float g_measured_fps = 60.0f;
int g_ram_used_mb = 48;
int g_settings_menu_open = 0;
int g_sidebar_visible = 0; /* Barra lateral oculta por defecto para pantalla completa */
int g_dock_visible = 1;    /* Barra dock inferior de controles */
static uint64_t g_last_time_usec = 0;
static int g_fps_frame_count = 0;

#define CB_SIZE   0x100000
#define HOST_SIZE (32 * 1024 * 1024)

SYS_PROCESS_PARAM(1001, 0x100000);

static gcmContextData *ctx;
static u32 W, H, pitch;
static u32 *fb[2];
static u32 fb_off[2];
static int cur, first = 1, running = 1;

static char current_url[512] = "eonsurf://portal";
static char page_title[256] = "Portal EonSurf";
static char page_body[8192] = "Bienvenido al motor nativo Dark Glass corriendo sobre el silicio Cell B.E. a 60 FPS.\n\nPresiona X sobre cualquier enlace para navegar con aceleracion por hardware RSX.";
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
    
    /* Inicializar estado inicial en la pila de historial con el portal EonSurf */
    eonsurf_tabs_init();
    eonsurf_format_markdown_and_links(EONSURF_PORTAL_MARKDOWN, current_url);
    strncpy(page_body, EONSURF_PORTAL_MARKDOWN, sizeof(page_body) - 1);
    eonsurf_history_push(current_url, page_body, 0, 200, strlen(page_body));

    /* Filas iniciales dinámicas para Ficha Técnica */
    g_info_row_count = 5;
    strncpy(g_infobox_header, "Ficha Tecnica", sizeof(g_infobox_header) - 1);
    strncpy(g_info_rows[0].key, "Fabricante", sizeof(g_info_rows[0].key) - 1);
    strncpy(g_info_rows[0].val, "Sony Computer Ent.", sizeof(g_info_rows[0].val) - 1);

    strncpy(g_info_rows[1].key, "Lanzamiento", sizeof(g_info_rows[1].key) - 1);
    strncpy(g_info_rows[1].val, "11 de nov. de 2006", sizeof(g_info_rows[1].val) - 1);

    strncpy(g_info_rows[2].key, "CPU", sizeof(g_info_rows[2].key) - 1);
    strncpy(g_info_rows[2].val, "Cell B.E. 3.2GHz", sizeof(g_info_rows[2].val) - 1);

    strncpy(g_info_rows[3].key, "GPU", sizeof(g_info_rows[3].key) - 1);
    strncpy(g_info_rows[3].val, "RSX 500MHz VRAM", sizeof(g_info_rows[3].val) - 1);

    strncpy(g_info_rows[4].key, "Ventas", sizeof(g_info_rows[4].key) - 1);
    strncpy(g_info_rows[4].val, "87.4M unidades", sizeof(g_info_rows[4].val) - 1);

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
    eonsurf_osk_handle_sysutil_event(status);
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

static inline void eonsurf_draw_dynamic_infobox_rows(int col_right_x, int table_start_y, int col_right_w, int canvas_y, int canvas_h);

static inline void eonsurf_draw_dynamic_infobox_rows(int col_right_x, int table_start_y, int col_right_w, int canvas_y, int canvas_h) {
    for (int r = 0; r < g_info_row_count; r++) {
        int row_y = table_start_y + (r * 26);
        if (row_y + 24 > canvas_y + canvas_h) break;

        if (r % 2 == 1) {
            rect(col_right_x + 14, row_y - 2, col_right_w - 28, 24, 0x00f8fafc);
        }
        text_hd(col_right_x + 18, row_y + 3, 1, g_info_rows[r].key, 0x000f172a);
        draw_text_clipped(col_right_x + 115, row_y + 3, 1, g_info_rows[r].val, 0x002563eb, col_right_x + col_right_w - 16);
    }
}


static inline void eonsurf_draw_download_osd(uint32_t *fb, int pitch) {
    if (g_dl.active == 0) return;

    if (g_dl.active == 2 || g_dl.active == -1) {
        if (--g_dl.status_timer <= 0) {
            g_dl.active = 0;
            return;
        }
    }

    const int box_x = 340, box_y = 585, box_w = 600, box_h = 68;
    uint32_t border_col = (g_dl.active == 2) ? 0x0000e676 : ((g_dl.active == -1) ? 0x00ff3d00 : 0x0000e5ff);

    /* Fondo oscuro Titanium con borde neon segun estado */
    draw_rounded_pill(fb, pitch, box_x, box_y, box_w, box_h, 10, 0x00101820, border_col);

    char line1[128];
    if (g_dl.active == 1) {
        snprintf(line1, sizeof(line1), "DESCARGANDO: %s (%d%%)", g_dl.filename, g_dl.percent);
    } else if (g_dl.active == 2) {
        snprintf(line1, sizeof(line1), "LISTO EN XMB: %s", g_dl.dest_path);
    } else {
        snprintf(line1, sizeof(line1), "ERROR AL DESCARGAR: %s", g_dl.filename);
    }
    draw_text_clipped(box_x + 16, box_y + 12, 1, line1, 0x00ffffff, box_x + box_w - 110);

    /* Riel de la barra de progreso */
    int bar_x = box_x + 16, bar_y = box_y + 38, bar_w = box_w - 32, bar_h = 16;
    draw_rounded_pill(fb, pitch, bar_x, bar_y, bar_w, bar_h, 6, 0x0022303c, 0x003a4a52);

    /* Relleno proporcional de la barra */
    int fill_w = (bar_w * g_dl.percent) / 100;
    if (fill_w > 8) {
        draw_rounded_pill(fb, pitch, bar_x, bar_y, fill_w, bar_h, 6, border_col, border_col);
    }

    /* Indicador de Megabytes descargados a la derecha */
    char mb_label[48];
    snprintf(mb_label, sizeof(mb_label), "%.2f MB", (double)g_dl.bytes_now / (1024.0 * 1024.0));
    draw_text_clipped(box_x + box_w - 100, box_y + 12, 1, mb_label, 0x0000e5ff, box_x + box_w - 12);
}

#include "eonsurf_mockup_engine.h"

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
    eonsurf_history_save_current_scroll(scroll_y);

    eonsurf_build_search_url_fixed(input_url, target_url, sizeof(target_url));

    strncpy(current_url, target_url, sizeof(current_url) - 1);
    current_url[sizeof(current_url) - 1] = '\0';

    snprintf(page_title, sizeof(page_title), "Cargando: %s", target_url);
    snprintf(page_body, sizeof(page_body), "Conectando al servidor web...");
    page_links_count = 0;
    scroll_y = 0;

    /* 1. Intercepcion local instantanea de eonsurf://portal y eonsurf://home (Cero Red) */
    if (strncmp(target_url, "eonsurf://", 10) == 0) {
        snprintf(page_title, sizeof(page_title), "Portal EonSurf");
        eonsurf_format_markdown_and_links(EONSURF_PORTAL_MARKDOWN, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
        eonsurf_update_dynamic_infobox("eonsurf://core", NULL, 0, 200, g_link_count);
        strncpy(g_tabs[g_current_tab].url, current_url, sizeof(g_tabs[g_current_tab].url) - 1);
        strncpy(g_tabs[g_current_tab].title, page_title, sizeof(g_tabs[g_current_tab].title) - 1);
        strncpy(g_tabs[g_current_tab].body, page_body, sizeof(g_tabs[g_current_tab].body) - 1);
        g_tabs[g_current_tab].scroll_y = 0;
        g_tabs[g_current_tab].http_code = 200;
        eonsurf_history_push(current_url, page_body, 0, 200, strlen(page_body));
        return;
    }

    if (!net_initialized) {
        snprintf(page_title, sizeof(page_title), "Sin Conexion");
        snprintf(page_body, sizeof(page_body), "La red de PS3 no esta inicializada.");
        return;
    }

    g_net_loading = 1;
    CURL *curl = curl_easy_init();
    if (curl) {
        struct HttpMemoryBuffer chunk = {0};
        chunk.data = malloc(1);
        chunk.size = 0;

        curl_easy_setopt(curl, CURLOPT_URL, target_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        struct curl_slist *stealth_hdrs = eonsurf_apply_stealth_browser_headers(curl);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);
        if (stealth_hdrs) {
            curl_slist_free_all(stealth_hdrs);
        }

        if (res == CURLE_OK && chunk.size > 0) {
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            /* Fallback anti-captcha automatico si DuckDuckGo tira HTTP 202 o desafio */
            if (eonsurf_is_captcha_blocked((int)response_code, chunk.data)) {
                char fallback_url[512] = "";
                eonsurf_build_fallback_wiki_search(target_url, fallback_url, sizeof(fallback_url));
                if (chunk.data) free(chunk.data);
                curl_easy_cleanup(curl);
                g_net_loading = 0;
                navigate_to(fallback_url);
                return;
            }

            snprintf(page_title, sizeof(page_title), "HTTP %ld: %s", response_code, target_url);

            static char raw_extracted[16384];
            int parsed_ok = 0;

            /* Si es una pagina de resultados de DuckDuckGo Lite, extraer snippets y titulos a Markdown */
            if (strstr(target_url, "duckduckgo.com")) {
                static char search_md[16384];
                if (eonsurf_parse_duckduckgo_results(chunk.data, search_md, sizeof(search_md))) {
                    eonsurf_format_markdown_and_links(search_md, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
                    parsed_ok = 1;
                }
            }

            if (!parsed_ok) {
                /* Intentar extracción con el nuevo motor de lectura de artículos web limpios */
                static char extracted_title[128];
                static char extracted_body[16384];
                static char cleaned_body[16384];
                if (eonsurf_extract_clean_article(chunk.data, chunk.size, extracted_title, sizeof(extracted_title), extracted_body, sizeof(extracted_body))) {
                    if (strlen(extracted_title) > 0) {
                        strncpy(page_title, extracted_title, sizeof(page_title) - 1);
                    }
                    eonsurf_strip_wiki_garbage(extracted_body, cleaned_body, sizeof(cleaned_body));
                    eonsurf_format_markdown_and_links(cleaned_body, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
                    parsed_ok = 1;
                }
            }

            if (!parsed_ok) {
                eonsurf_fsm_extract_text_and_links(chunk.data, chunk.size, raw_extracted, sizeof(raw_extracted), page_links, &page_links_count);
                if (strlen(raw_extracted) > 0) {
                    eonsurf_format_markdown_and_links(raw_extracted, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
                } else {
                    snprintf(page_body, sizeof(page_body), "Pagina recibida (%u bytes).", (unsigned int)chunk.size);
                }
            }

            /* Extraer primera imagen de la pagina para el Infobox */
            char detected_img_url[256] = "";
            if (eonsurf_find_first_image_url(chunk.data, detected_img_url, sizeof(detected_img_url))) {
                if (eonsurf_fetch_and_decode_infobox_image(detected_img_url)) {
                    g_layout.infobox_rgba_tex = g_infobox_tex;
                    g_layout.img_w = g_infobox_w;
                    g_layout.img_h = g_infobox_h;
                }
            }

            /* Actualizar Infobox 100% dinamico en tiempo real (Wikipedia o Telemetria Red) */
            eonsurf_update_dynamic_infobox(target_url, chunk.data, chunk.size, (int)response_code, g_link_count);
            if (strlen(g_infobox_header) > 0) {
                strncpy(g_layout.title, g_infobox_header, sizeof(g_layout.title) - 1);
            }
            strncpy(g_layout.subtitle, target_url, sizeof(g_layout.subtitle) - 1);

            /* Actualizar pestaña actual con los nuevos datos */
            strncpy(g_tabs[g_current_tab].url, current_url, sizeof(g_tabs[g_current_tab].url) - 1);
            strncpy(g_tabs[g_current_tab].title, page_title, sizeof(g_tabs[g_current_tab].title) - 1);
            strncpy(g_tabs[g_current_tab].body, page_body, sizeof(g_tabs[g_current_tab].body) - 1);
            g_tabs[g_current_tab].scroll_y = 0;
            g_tabs[g_current_tab].http_code = (int)response_code;

            /* Guardar página en la pila de historial con snapshot en RAM */
            eonsurf_history_push(current_url, page_body, 0, (int)response_code, chunk.size);
        } else {
            snprintf(page_title, sizeof(page_title), "Error de conexion (%d)", (int)res);
            snprintf(page_body, sizeof(page_body), "No se pudo obtener respuesta de %s: %s", target_url, curl_easy_strerror(res));
        }

        if (chunk.data) free(chunk.data);
        curl_easy_cleanup(curl);
    }
    g_net_loading = 0;
}

int main(void)
{
    padInfo pinfo; padData pdata;
    int px = 640, py = 460;
    char buf[128];
    int prev_btn_triangle = 0;
    int prev_btn_cross = 0;
    int prev_btn_circle = 0;
    int prev_btn_left = 0;
    int prev_btn_right = 0;

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, on_sysutil, NULL);
    init_screen();
    init_default_layout();
    ioPadInit(7);

    if (netInitialize() == 0) {
        net_initialized = 1;
    }

    while (running) {
        int ax = 128, ay = 128;
        /* Medición real de FPS cada 30 frames */
        g_fps_frame_count++;
        if (g_fps_frame_count >= 30) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            uint64_t now_usec = (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
            if (g_last_time_usec > 0 && now_usec > g_last_time_usec) {
                uint64_t elapsed = now_usec - g_last_time_usec;
                g_measured_fps = (float)(30.0 * 1000000.0 / (double)elapsed);
            }
            g_last_time_usec = now_usec;
            g_fps_frame_count = 0;
        }
        sysUtilCheckCallback();

        /* Comprobar si el usuario confirmo texto en el teclado Sony OSK */
        char osk_user_text[256];
        if (eonsurf_osk_poll_result(osk_user_text, sizeof(osk_user_text))) {
            navigate_to(osk_user_text);
        }
        ioPadGetInfo(&pinfo);
        if (pinfo.status[0]) {
            ioPadGetData(0, &pdata);
            ax = pdata.ANA_L_H; ay = pdata.ANA_L_V;
            static int prev_dpad_up = 0;
            static int prev_dpad_down = 0;
            static int prev_btn_l3 = 0;

            int dpad_up_edge = (pdata.BTN_UP && !prev_dpad_up);
            int dpad_down_edge = (pdata.BTN_DOWN && !prev_dpad_down);
            int l3_edge = (pdata.BTN_L3 && !prev_btn_l3);

            prev_dpad_up = pdata.BTN_UP;
            prev_dpad_down = pdata.BTN_DOWN;
            prev_btn_l3 = pdata.BTN_L3;

            /* Actualización ergonómica nativa PS3 (aceleración, imán de links, L3 zoom) */
            EonCursorMode mode = eonsurf_update_ps3_ergonomics(&px, &py, ax, ay,
                                                               dpad_up_edge, dpad_down_edge,
                                                               l3_edge, 0, g_links, g_link_count);
            if (mode == CURSOR_HAND_LINK) g_cursor_dynamic_mode = CURSOR_MODE_HAND_LINK;
            else if (mode == CURSOR_IBEAM_URL) g_cursor_dynamic_mode = CURSOR_MODE_IBEAM_TEXT;
            else g_cursor_dynamic_mode = CURSOR_MODE_NORMAL;

            /* Scroll vertical: Stick Derecho (R3) + Fallback teclado PC / L1 / R1 */
            int ry = pdata.ANA_R_V - 128;
            if (abs(ry) > 16) {
                scroll_y += ry / 6;
            }
            if (pdata.BTN_L1) scroll_y -= 12;
            if (pdata.BTN_R1) scroll_y += 12;
            if (pdata.BTN_L2) scroll_y -= 20;
            if (pdata.BTN_R2) scroll_y += 20;
            if (scroll_y < 0) scroll_y = 0;
            if (scroll_y > max_scroll_y) scroll_y = max_scroll_y;

            /* Teclado Sony OSK con Triangulo */
            if (pdata.BTN_TRIANGLE && !prev_btn_triangle) {
                if (!eonsurf_osk_is_active()) {
                    eonsurf_osk_open("EonSurf: Escribi una busqueda o URL", current_url);
                }
            }
            prev_btn_triangle = pdata.BTN_TRIANGLE;

            /* 1. Volver Atras: Boton Circulo (O) o D-Pad Izquierda */
            if ((pdata.BTN_CIRCLE && !prev_btn_circle) || (pdata.BTN_LEFT && !prev_btn_left)) {
                int restored_scroll = 0;
                int restored_code = 200;
                size_t restored_dl = 0;
                if (eonsurf_history_go_back(scroll_y, current_url, page_body, &restored_scroll, &restored_code, &restored_dl)) {
                    scroll_y = restored_scroll;
                    snprintf(page_title, sizeof(page_title), "%s", current_url);
                    /* Re-extraer enlaces y telemetria al instante sin usar red */
                    eonsurf_format_markdown_and_links(page_body, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
                    eonsurf_update_dynamic_infobox(current_url, page_body, restored_dl, restored_code, g_link_count);
                }
            }
            prev_btn_circle = pdata.BTN_CIRCLE;
            prev_btn_left = pdata.BTN_LEFT;

            /* 2. Ir Adelante: D-Pad Derecha */
            if (pdata.BTN_RIGHT && !prev_btn_right) {
                int restored_scroll = 0;
                int restored_code = 200;
                size_t restored_dl = 0;
                if (eonsurf_history_go_forward(scroll_y, current_url, page_body, &restored_scroll, &restored_code, &restored_dl)) {
                    scroll_y = restored_scroll;
                    snprintf(page_title, sizeof(page_title), "%s", current_url);
                    eonsurf_format_markdown_and_links(page_body, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
                    eonsurf_update_dynamic_infobox(current_url, page_body, restored_dl, restored_code, g_link_count);
                }
            }
            prev_btn_right = pdata.BTN_RIGHT;

            /* 3. Clic con Cruz [X] */
            if (pdata.BTN_CROSS && !prev_btn_cross) {
                /* A. Clic sobre pestañas superiores o botón '+' */
                /* Clic en Emblema Hexagonal [ES] para abrir Configuraciones */
                if (px >= 1180 && px <= 1235 && py >= 58 && py <= 100) {
                    g_settings_menu_open = !g_settings_menu_open;
                }
                /* Clic dentro de las opciones del Centro de Control EonSurf si está abierto */
                else if (g_settings_menu_open && px >= 820 && px <= 1250 && py >= 105 && py <= 500) {
                    if (px >= 835 && px <= 1235 && py >= 175 && py <= 209) {
                        /* Opción 1: Alternar barra lateral */
                        g_sidebar_visible = !g_sidebar_visible;
                    } else if (px >= 835 && px <= 1235 && py >= 215 && py <= 249) {
                        /* Opción 2: Alternar dock inferior */
                        g_dock_visible = !g_dock_visible;
                    } else if (px >= 835 && px <= 1235 && py >= 255 && py <= 289) {
                        /* Opción 3: Alternar estilo de puntero */
                        g_cursor_style = (g_cursor_style + 1) % 3;
                    } else if (px >= 835 && px <= 1235 && py >= 295 && py <= 329) {
                        /* Opción 4: Resetear zoom y scroll */
                        scroll_y = 0;
                    } else if (px >= 835 && px <= 1235 && py >= 335 && py <= 369) {
                        /* Opción 5: Ir al Portal de Inicio */
                        navigate_to("eonsurf://portal");
                        g_settings_menu_open = 0;
                    } else if (py >= 435 && py <= 470) {
                        g_settings_menu_open = 0;
                    }
                }

                int tab_click = eonsurf_tabs_check_click(px, py);
                if (tab_click >= 0 && tab_click < MAX_TABS) {
                    /* Guardar estado de la pestaña actual antes de cambiar */
                    strncpy(g_tabs[g_current_tab].url, current_url, sizeof(g_tabs[g_current_tab].url) - 1);
                    strncpy(g_tabs[g_current_tab].title, page_title, sizeof(g_tabs[g_current_tab].title) - 1);
                    strncpy(g_tabs[g_current_tab].body, page_body, sizeof(g_tabs[g_current_tab].body) - 1);
                    g_tabs[g_current_tab].scroll_y = scroll_y;
                    
                    eonsurf_tab_select(tab_click);
                    strncpy(current_url, g_tabs[g_current_tab].url, sizeof(current_url) - 1);
                    strncpy(page_title, g_tabs[g_current_tab].title, sizeof(page_title) - 1);
                    strncpy(page_body, g_tabs[g_current_tab].body, sizeof(page_body) - 1);
                    scroll_y = g_tabs[g_current_tab].scroll_y;
                } else if (tab_click >= 20 && tab_click < 20 + MAX_TABS) {
                    /* Cerrar pestaña */
                    eonsurf_tab_close(tab_click - 20);
                    strncpy(current_url, g_tabs[g_current_tab].url, sizeof(current_url) - 1);
                    strncpy(page_title, g_tabs[g_current_tab].title, sizeof(page_title) - 1);
                    strncpy(page_body, g_tabs[g_current_tab].body, sizeof(page_body) - 1);
                    scroll_y = g_tabs[g_current_tab].scroll_y;
                } else if (tab_click == 99) {
                    /* Abrir nueva pestaña con Speed Dial */
                    eonsurf_tab_open_new("eonsurf://portal", "Portal EonSurf");
                    strncpy(current_url, g_tabs[g_current_tab].url, sizeof(current_url) - 1);
                    strncpy(page_title, g_tabs[g_current_tab].title, sizeof(page_title) - 1);
                    strncpy(page_body, g_tabs[g_current_tab].body, sizeof(page_body) - 1);
                    scroll_y = 0;
                }
                /* A. Clic en botones '←', '→' o '↻' de la barra superior */
                int nav_click = eonsurf_check_nav_buttons_click(px, py);
                if (nav_click == -1) {
                    /* Clic con cursor en '←' (Back) */
                    int restored_scroll = 0, restored_code = 200; size_t restored_dl = 0;
                    if (eonsurf_history_go_back(scroll_y, current_url, page_body, &restored_scroll, &restored_code, &restored_dl)) {
                        scroll_y = restored_scroll;
                        snprintf(page_title, sizeof(page_title), "%s", current_url);
                        eonsurf_format_markdown_and_links(page_body, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
                        eonsurf_update_dynamic_infobox(current_url, page_body, restored_dl, restored_code, g_link_count);
                    }
                } else if (nav_click == 1) {
                    /* Clic con cursor en '→' (Forward) */
                    int restored_scroll = 0, restored_code = 200; size_t restored_dl = 0;
                    if (eonsurf_history_go_forward(scroll_y, current_url, page_body, &restored_scroll, &restored_code, &restored_dl)) {
                        scroll_y = restored_scroll;
                        snprintf(page_title, sizeof(page_title), "%s", current_url);
                        eonsurf_format_markdown_and_links(page_body, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
                        eonsurf_update_dynamic_infobox(current_url, page_body, restored_dl, restored_code, g_link_count);
                    }
                } else if (nav_click == 2) {
                    /* Clic con cursor en '↻' (Reload) */
                    navigate_to(current_url);
                }
                /* B. Clic sobre enlace en el texto */
                else {
                    const char *clicked_link = eonsurf_check_link_click(px, py);
                    if (clicked_link) {
                        char link_copy[512] = "";
                        strncpy(link_copy, clicked_link, sizeof(link_copy) - 1);
                        /* Interceptor de descargas directas a disco (.pkg, .zip, .iso, .rar, .7z) */
                        if (eonsurf_is_downloadable_file(link_copy)) {
                            eonsurf_start_direct_download(link_copy);
                        } else {
                            navigate_to(link_copy);
                        }
                    }
                    /* Barra de URL superior (Pill neón en Y: 58-100, X: 182-1152) */
                    else if (py >= 58 && py <= 100 && px >= 182 && px <= 1152) {
                        if (!eonsurf_osk_is_active()) {
                            eonsurf_osk_open("EonSurf: Ingresar URL o Busqueda Web", current_url);
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
            }
            prev_btn_cross = pdata.BTN_CROSS;

            /* Botón START: Alterna Centro de Control EonSurf */
            if (pdata.BTN_START && !pdata.BTN_SELECT) {
                static int prev_btn_start = 0;
                if (!prev_btn_start) {
                    g_settings_menu_open = !g_settings_menu_open;
                }
                prev_btn_start = 1;
            } else {
                static int prev_btn_start = 0;
                prev_btn_start = 0;
            }

            /* Botón CUADRADO: Alternar barra lateral (Infobox / Ficha) */
            {
                static int prev_btn_sq = 0;
                if (pdata.BTN_SQUARE) {
                    if (!prev_btn_sq) g_sidebar_visible = !g_sidebar_visible;
                    prev_btn_sq = 1;
                } else {
                    prev_btn_sq = 0;
                }
            }

            /* Botón SELECT: Alternar barra dock inferior */
            {
                static int prev_btn_sel = 0;
                if (pdata.BTN_SELECT && !pdata.BTN_START) {
                    if (!prev_btn_sel) g_dock_visible = !g_dock_visible;
                    prev_btn_sel = 1;
                } else {
                    prev_btn_sel = 0;
                }
            }

            if (pdata.BTN_START && pdata.BTN_SELECT) {
                running = 0;
            }
        }

        /* ====================================================================
         * MOTOR DE RENDERIZADO COMPLETO DEL MOCKUP V2 (60 FPS NATIVO)
         * - Ondas de seda XMB + Spotlight radial plateado
         * - Pestañas curvas estilo Chrome con glow cian y logo [W]
         * - Barra URL con resplandor neón difuso y candado verde HTTPS
         * - Título H1 a 34px y tipografía TrueType con contorno negro (stroke)
         * - Infobox lateral con foto y telemetría dinámica
         * - Dock inferior translúcido con los 5 controles DualShock 3
         * - Cursor neón grande con glow turquesa
         * ==================================================================== */
        eonsurf_render_complete_mockup_frame(fb[cur], W, current_url, page_title, page_body,
                                             px, py, scroll_y, &max_scroll_y);

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
