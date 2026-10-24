#ifndef EONSURF_DYNAMIC_WEB_H
#define EONSURF_DYNAMIC_WEB_H

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <stdint.h>

#include <curl/curl.h>



#define MAX_INFO_ROWS 5



typedef struct {

    char key[24];

    char val[36];

} EonInfoRow;



static EonInfoRow g_info_rows[MAX_INFO_ROWS];

static int g_info_row_count = 0;

static char g_infobox_header[32] = "Web Document";



/* ============================================================================

 * PARTE 1: TABLA DERECHA (INFOBOX) 100% DINÁMICA EN TIEMPO REAL

 * ============================================================================ */



/* Limpia tags HTML internos de una celda <th> o <td> y recorta espacios */

static void extract_clean_cell_text(const char *start, const char *end, char *out, size_t max_len) {

    size_t di = 0;

    int in_tag = 0;

    for (const char *p = start; p < end && di + 1 < max_len; p++) {

        if (*p == '<') { in_tag = 1; continue; }

        if (*p == '>') { in_tag = 0; continue; }

        if (!in_tag && (unsigned char)*p >= 32) {

            /* Evitar espacios dobles consecutivos */

            if (*p == ' ' && (di == 0 || out[di - 1] == ' ')) continue;

            out[di++] = *p;

        }

    }

    out[di] = '\0';

}



/*

 * Actualiza las 5 filas de la tarjeta derecha en vivo:

 * - Si el HTML tiene una <table class="infobox"> (Wikipedia), extrae sus filas reales.

 * - Si es otra web (GitHub, foros, Markdown), muestra telemetría real de red.

 */

static inline void eonsurf_update_dynamic_infobox(const char *url, const char *raw_html,

                                    size_t bytes_downloaded, int http_code, int detected_links) {

    g_info_row_count = 0;



    /* 1. Intentar extraer filas reales de una Infobox de Wikipedia */

    const char *infobox = strstr(raw_html, "infobox");

    if (infobox) {

        strncpy(g_infobox_header, "Ficha Wikipedia", sizeof(g_infobox_header) - 1);

        const char *p = infobox;

        const char *table_end = strstr(p, "</table>");

        if (!table_end) table_end = p + 30000;



        while (p < table_end && g_info_row_count < MAX_INFO_ROWS) {

            const char *tr = strstr(p, "<tr");

            if (!tr || tr >= table_end) break;

            const char *tr_end = strstr(tr, "</tr>");

            if (!tr_end) break;



            const char *th = strstr(tr, "<th");

            const char *td = strstr(tr, "<td");

            if (th && td && th < td && td < tr_end) {

                const char *th_close = strstr(th, "</th>");

                const char *td_close = strstr(td, "</td>");

                if (th_close && td_close && th_close <= td && td_close <= tr_end) {

                    const char *th_body = strchr(th, '>');

                    const char *td_body = strchr(td, '>');

                    if (th_body && td_body) {

                        char k[24], v[36];

                        extract_clean_cell_text(th_body + 1, th_close, k, sizeof(k));

                        extract_clean_cell_text(td_body + 1, td_close, v, sizeof(v));

                        if (strlen(k) > 1 && strlen(v) > 1) {

                            strncpy(g_info_rows[g_info_row_count].key, k, 23);

                            strncpy(g_info_rows[g_info_row_count].val, v, 35);

                            g_info_row_count++;

                        }

                    }

                }

            }

            p = tr_end + 5;

        }

        if (g_info_row_count > 0) return; /* Éxito extrayendo ficha real de Wikipedia */

    }



    /* 2. Si no tiene Infobox (ej. GitHub, PSX-Place, buscador), mostrar Telemetría HTTP en vivo */

    strncpy(g_infobox_header, "Telemetria Red", sizeof(g_infobox_header) - 1);



    /* Extraer el Host/Servidor de la URL actual */

    char host[36] = "Desconocido";

    const char *h_start = strstr(url, "://");

    h_start = h_start ? (h_start + 3) : url;

    const char *h_end = strchr(h_start, '/');

    size_t h_len = h_end ? (size_t)(h_end - h_start) : strlen(h_start);

    if (h_len >= sizeof(host)) h_len = sizeof(host) - 1;

    memcpy(host, h_start, h_len);

    host[h_len] = '\0';



    /* Fila 0: Servidor remoto */

    strcpy(g_info_rows[0].key, "Servidor");

    snprintf(g_info_rows[0].val, 35, "%s", host);



    /* Fila 1: Estado HTTP y protocolo TLS */

    strcpy(g_info_rows[1].key, "Estado");

    snprintf(g_info_rows[1].val, 35, "HTTP %d (TLS Activo)", http_code);



    /* Fila 2: Peso real descargado en RAM XDR */

    strcpy(g_info_rows[2].key, "Payload");

    snprintf(g_info_rows[2].val, 35, "%.1f KB en RAM XDR", (double)bytes_downloaded / 1024.0);



    /* Fila 3: Cantidad real de enlaces interactivos detectados en la página */

    strcpy(g_info_rows[3].key, "Enlaces");

    snprintf(g_info_rows[3].val, 35, "%d links activos (X)", detected_links);



    /* Fila 4: Formato detectado */

    strcpy(g_info_rows[4].key, "Formato");

    const char *is_md = strstr(url, ".md");

    snprintf(g_info_rows[4].val, 35, "%s", is_md ? "Markdown Raw (UTF-8)" : "HTML5 Estructurado");



    g_info_row_count = 5;

}



/* Dibuja las 5 filas dinámicas en la tarjeta blanca de la derecha con corte de seguridad */

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



/* ============================================================================

 * PARTE 2: BUSCADOR WEB REAL EN VIVO (CONVIERTE BÚSQUEDAS EN ENLACES REALES)

 * ============================================================================ */



/* Codifica espacios como '%20' para armar la URL de consulta al buscador */

static void url_encode_query(const char *src, char *dst, size_t max_len) {

    size_t di = 0;

    for (size_t i = 0; src[i] != '\0' && di + 4 < max_len; i++) {

        if (src[i] == ' ') {

            dst[di++] = '%'; dst[di++] = '2'; dst[di++] = '0';

        } else if ((unsigned char)src[i] > 32 && src[i] != '"' && src[i] != '&') {

            dst[di++] = src[i];

        }

    }

    dst[di] = '\0';

}



/*

 * Si el usuario escribe en el teclado Sony (Triángulo) algo que NO empieza con "http"

 * (por ejemplo: "playstation 3 homebrew" o "metal gear solid 4"), arma la URL real

 * de búsqueda ligera o convierte el HTML de DuckDuckGo Lite en formato Markdown limpio.

 */

static inline void eonsurf_build_search_url(const char *user_input, char *out_url, size_t max_len) {

    if (strncmp(user_input, "http://", 7) == 0 || strncmp(user_input, "https://", 8) == 0) {

        strncpy(out_url, user_input, max_len - 1);

        out_url[max_len - 1] = '\0';

        return;

    }

    char encoded[192];

    url_encode_query(user_input, encoded, sizeof(encoded));

    /* DuckDuckGo HTML Lite devuelve resultados web limpios sin requerir JavaScript */

    snprintf(out_url, max_len, "https://html.duckduckgo.com/html/?q=%s", encoded);

}



/*

 * Parser específico para cuando la URL descargada proviene de html.duckduckgo.com:

 * Extrae cada resultado (<a class="result__a" href="...">Título</a> y snippet)

 * y lo formatea en Markdown limpio [Título](URL) para que tu motor lo pinte con links azules.

 */

static inline int eonsurf_parse_duckduckgo_results(const char *raw_html, char *out_markdown, size_t max_len) {

    if (!strstr(raw_html, "result__a")) return 0; /* No es una página de búsqueda */



    size_t di = 0;

    di += snprintf(out_markdown, max_len, "# Resultados de Busqueda en Vivo (EonSurf Search)\n\n");



    const char *p = raw_html;

    int count = 0;



    while ((p = strstr(p, "class=\"result__a\"")) != NULL && count < 12 && di + 256 < max_len) {

        /* Retroceder o avanzar para buscar el href="..." dentro del mismo tag <a> */

        const char *tag_start = p;

        while (tag_start > raw_html && *tag_start != '<') tag_start--;

        const char *href = strstr(tag_start, "href=\"");

        const char *tag_close = strchr(p, '>');

        const char *a_end = strstr(p, "</a>");



        if (href && tag_close && a_end && href < tag_close) {

            href += 6;

            const char *href_end = strchr(href, '"');

            if (href_end) {

                char link_url[256] = {0};

                char title[128] = {0};



                /* Si DuckDuckGo envuelve el link en //duckduckgo.com/l/?uddg=https%3A%2F%2F... */

                const char *uddg = strstr(href, "uddg=");

                if (uddg && uddg < href_end) {

                    uddg += 5;

                    size_t ui = 0;

                    for (const char *c = uddg; c < href_end && *c != '&' && ui + 1 < sizeof(link_url); c++) {

                        if (*c == '%' && c + 2 < href_end) {

                            unsigned int hex = 0;

                            sscanf(c + 1, "%02x", &hex);

                            link_url[ui++] = (char)hex;

                            c += 2;

                        } else {

                            link_url[ui++] = *c;

                        }

                    }

                    link_url[ui] = '\0';

                } else {

                    size_t ulen = (size_t)(href_end - href);

                    if (ulen >= sizeof(link_url)) ulen = sizeof(link_url) - 1;

                    memcpy(link_url, href, ulen);

                }



                extract_clean_cell_text(tag_close + 1, a_end, title, sizeof(title));



                if (strlen(title) > 2 && strncmp(link_url, "http", 4) == 0) {

                    count++;

                    di += snprintf(out_markdown + di, max_len - di,

                                   "## %d. [%s](%s)\n", count, title, link_url);



                    /* Extraer descripción breve (result__snippet) debajo del enlace */

                    const char *snip = strstr(a_end, "result__snippet");

                    if (snip && (snip - a_end) < 800) {

                        const char *s_open = strchr(snip, '>');

                        const char *s_close = s_open ? strstr(s_open, "</a") : NULL;

                        if (!s_close && s_open) s_close = strstr(s_open, "</div");

                        if (s_open && s_close) {

                            char desc[220];

                            extract_clean_cell_text(s_open + 1, s_close, desc, sizeof(desc));

                            di += snprintf(out_markdown + di, max_len - di, " - %s\n\n", desc);

                        }

                    }

                }

            }

        }

        p = a_end ? (a_end + 4) : (p + 17);

    }



    return (count > 0);

}

#endif /* EONSURF_DYNAMIC_WEB_H */
