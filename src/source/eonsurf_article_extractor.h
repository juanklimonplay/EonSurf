#ifndef EONSURF_ARTICLE_EXTRACTOR_H
#define EONSURF_ARTICLE_EXTRACTOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/*
 * Decodificador de entidades HTML comunes (&quot;, &lt;, &gt;, &amp;, &nbsp;, &#NNN;)
 */
static inline size_t eonsurf_decode_html_entity(const char *src, char *out_char) {
    if (!src || *src != '&') return 0;

    if (strncmp(src, "&quot;", 6) == 0) { *out_char = '"'; return 6; }
    if (strncmp(src, "&apos;", 6) == 0) { *out_char = '\''; return 6; }
    if (strncmp(src, "&amp;", 5) == 0)  { *out_char = '&'; return 5; }
    if (strncmp(src, "&lt;", 4) == 0)   { *out_char = '<'; return 4; }
    if (strncmp(src, "&gt;", 4) == 0)   { *out_char = '>'; return 4; }
    if (strncmp(src, "&nbsp;", 6) == 0) { *out_char = ' '; return 6; }
    if (strncmp(src, "&bull;", 6) == 0) { *out_char = '-'; return 6; }
    if (strncmp(src, "&mdash;", 7) == 0) { *out_char = '-'; return 7; }
    if (strncmp(src, "&ndash;", 7) == 0) { *out_char = '-'; return 7; }

    /* Entidades numéricas: &#160; o &#xA0; */
    if (src[1] == '#') {
        const char *sc = strchr(src, ';');
        if (sc && (sc - src) <= 8) {
            long val = 0;
            if (src[2] == 'x' || src[2] == 'X') {
                val = strtol(src + 3, NULL, 16);
            } else {
                val = strtol(src + 2, NULL, 10);
            }
            if (val == 160) { *out_char = ' '; return (size_t)(sc - src + 1); }
            if (val >= 32 && val <= 126) { *out_char = (char)val; return (size_t)(sc - src + 1); }
            /* Para caracteres latinos comunes en ISO-8859-1 mapeados a UTF-8/ASCII */
            if (val == 225 || val == 193) { *out_char = 'a'; return (size_t)(sc - src + 1); }
            if (val == 233 || val == 201) { *out_char = 'e'; return (size_t)(sc - src + 1); }
            if (val == 237 || val == 205) { *out_char = 'i'; return (size_t)(sc - src + 1); }
            if (val == 243 || val == 211) { *out_char = 'o'; return (size_t)(sc - src + 1); }
            if (val == 250 || val == 218) { *out_char = 'u'; return (size_t)(sc - src + 1); }
            if (val == 241 || val == 209) { *out_char = 'n'; return (size_t)(sc - src + 1); }
            *out_char = ' ';
            return (size_t)(sc - src + 1);
        }
    }

    return 0;
}

/*
 * Extrae texto de artículos web limpios preservando hipervínculos estructurados:
 * Convierte etiquetas HTML <a href="...">Texto</a> en Markdown [Texto](URL)
 * para que el motor de maquetación los pinte en cian (#38BDF8) interactivos.
 * Elimina completamente metadatos JSON (data-mw, etc.) y tablas/infoboxes del cuerpo principal.
 */
static inline int eonsurf_extract_clean_article(const char *html_data, size_t html_len,
                                               char *out_title, size_t max_title,
                                               char *out_body, size_t max_body) {
    if (!html_data || html_len == 0 || !out_body || max_body == 0) return 0;

    /* 1. Extraer etiqueta <title>...</title> */
    const char *title_start = strstr(html_data, "<title>");
    if (!title_start) title_start = strstr(html_data, "<TITLE>");
    if (title_start) {
        title_start += 7;
        const char *title_end = strstr(title_start, "</title>");
        if (!title_end) title_end = strstr(title_start, "</TITLE>");
        if (title_end && title_end > title_start) {
            size_t di = 0;
            for (const char *tp = title_start; tp < title_end && di + 1 < max_title; tp++) {
                char dec_c = 0;
                size_t ent_len = eonsurf_decode_html_entity(tp, &dec_c);
                if (ent_len > 0) {
                    out_title[di++] = dec_c;
                    tp += (ent_len - 1);
                    continue;
                }
                unsigned char c = (unsigned char)*tp;
                if (c == '\r' || c == '\n' || c == '\t') {
                    if (di > 0 && out_title[di - 1] != ' ') out_title[di++] = ' ';
                } else if (c >= 32 && c <= 126) {
                    out_title[di++] = (char)c;
                } else if (c >= 192) {
                    out_title[di++] = (char)c; /* Permitir UTF-8 */
                }
            }
            while (di > 0 && out_title[di - 1] == ' ') di--;
            out_title[di] = '\0';
        }
    }

    /* 2. Buscar el contenedor principal del artículo */
    const char *body_ptr = html_data;
    const char *main_candidates[] = {
        "mw-parser-output",
        "mw-content-text",
        "<main",
        "<article",
        "role=\"main\"",
        "<body"
    };

    for (int k = 0; k < 6; k++) {
        const char *found = strstr(html_data, main_candidates[k]);
        if (found) { body_ptr = found; break; }
    }

    /*
     * 3. Si encontramos la etiqueta del contenedor, avanzar hasta el final de la etiqueta '>'
     * para no parsear atributos internos de <div class="mw-parser-output" ...>
     */
    const char *first_tag_close = strchr(body_ptr, '>');
    if (first_tag_close) {
        body_ptr = first_tag_close + 1;
    }

    /*
     * 4. En páginas de Wikipedia / MediaWiki, antes del texto real suele haber estilos,
     * metadatos y una ficha o infobox (<table class="infobox"...).
     * Si detectamos <table dentro de los primeros 30.000 bytes de mw-parser-output,
     * saltamos completamente la tabla de infobox hasta su cierre </table>.
     */
    const char *check_table = strstr(body_ptr, "<table");
    if (!check_table) check_table = strstr(body_ptr, "<TABLE");
    if (check_table && (check_table - body_ptr) < 30000) {
        int table_depth = 0;
        const char *scan = check_table;
        while (*scan && (scan - check_table) < 200000) {
            if (strncasecmp(scan, "<table", 6) == 0) {
                table_depth++;
                scan += 6;
            } else if (strncasecmp(scan, "</table>", 8) == 0) {
                table_depth--;
                scan += 8;
                if (table_depth <= 0) {
                    body_ptr = scan;
                    break;
                }
            } else {
                scan++;
            }
        }
    }

    /* 5. Extraer párrafos limpios e hipervínculos como [Label](URL) */
    size_t out_idx = 0;
    const char *p = body_ptr;
    int table_nest = 0;

    while (*p && out_idx + 64 < max_body) {
        if (*p == '<') {
            /* Descartar bloques de scripts, estilos, audio, video, etc. */
            if (strncasecmp(p, "<script", 7) == 0) {
                const char *close_tag = strstr(p, "</script>");
                if (!close_tag) close_tag = strstr(p, "</SCRIPT>");
                if (close_tag) { p = close_tag + 9; continue; } else break;
            }
            if (strncasecmp(p, "<style", 6) == 0) {
                const char *close_tag = strstr(p, "</style>");
                if (!close_tag) close_tag = strstr(p, "</STYLE>");
                if (close_tag) { p = close_tag + 8; continue; } else break;
            }
            if (strncasecmp(p, "<table", 6) == 0) {
                table_nest++;
                /* Avanzar hasta el cierre '>' de la apertura de table */
                while (*p && *p != '>') p++;
                if (*p == '>') p++;
                continue;
            }
            if (strncasecmp(p, "</table>", 8) == 0) {
                if (table_nest > 0) table_nest--;
                p += 8;
                continue;
            }
            if (strncasecmp(p, "<nav", 4) == 0) {
                const char *close_tag = strstr(p, "</nav>");
                if (!close_tag) close_tag = strstr(p, "</NAV>");
                if (close_tag) { p = close_tag + 6; continue; } else break;
            }
            if (strncasecmp(p, "<header", 7) == 0) {
                const char *close_tag = strstr(p, "</header>");
                if (!close_tag) close_tag = strstr(p, "</HEADER>");
                if (close_tag) { p = close_tag + 9; continue; } else break;
            }
            if (strncasecmp(p, "<footer", 7) == 0) {
                const char *close_tag = strstr(p, "</footer>");
                if (!close_tag) close_tag = strstr(p, "</FOOTER>");
                if (close_tag) { p = close_tag + 9; continue; } else break;
            }
            if (strncasecmp(p, "<figure", 7) == 0) {
                const char *close_tag = strstr(p, "</figure>");
                if (!close_tag) close_tag = strstr(p, "</FIGURE>");
                if (close_tag) { p = close_tag + 9; continue; } else break;
            }

            /* Si estamos dentro de una tabla cualquiera, no dumpear texto desordenado al cuerpo */
            if (table_nest > 0) {
                p++;
                continue;
            }

            /* Detectar saltos de párrafo y encabezados */
            if (strncasecmp(p, "</p>", 4) == 0 || strncasecmp(p, "<br", 3) == 0) {
                if (out_idx > 0 && out_body[out_idx - 1] != '\n') {
                    out_body[out_idx++] = '\n';
                    out_body[out_idx++] = '\n';
                }
            } else if (strncasecmp(p, "<h1", 3) == 0) {
                if (out_idx > 0 && out_body[out_idx - 1] != '\n') out_body[out_idx++] = '\n';
                out_idx += snprintf(&out_body[out_idx], max_body - out_idx, "\n# ");
            } else if (strncasecmp(p, "<h2", 3) == 0) {
                if (out_idx > 0 && out_body[out_idx - 1] != '\n') out_body[out_idx++] = '\n';
                out_idx += snprintf(&out_body[out_idx], max_body - out_idx, "\n## ");
            } else if (strncasecmp(p, "<h3", 3) == 0) {
                if (out_idx > 0 && out_body[out_idx - 1] != '\n') out_body[out_idx++] = '\n';
                out_idx += snprintf(&out_body[out_idx], max_body - out_idx, "\n### ");
            } else if (strncasecmp(p, "<li", 3) == 0) {
                if (out_idx > 0 && out_body[out_idx - 1] != '\n') out_body[out_idx++] = '\n';
                out_idx += snprintf(&out_body[out_idx], max_body - out_idx, "* ");
            }

            /* Detectar etiquetas de enlace: <a href="...">Texto</a> */
            if (strncasecmp(p, "<a ", 3) == 0) {
                /*
                 * CUIDADO: La etiqueta <a> puede contener atributos con comillas como data-mw='...'
                 * Buscar '>' que realmente cierra la etiqueta <a> sin ser parte de un atributo.
                 */
                const char *scan_a = p + 3;
                const char *href_pos = NULL;
                int in_q = 0;
                char q_char = 0;
                while (*scan_a) {
                    if (!in_q && (*scan_a == '"' || *scan_a == '\'')) {
                        in_q = 1;
                        q_char = *scan_a;
                    } else if (in_q && *scan_a == q_char) {
                        in_q = 0;
                    } else if (!in_q && *scan_a == '>') {
                        break;
                    }
                    if (!in_q && strncmp(scan_a, "href=", 5) == 0) {
                        href_pos = scan_a;
                    }
                    scan_a++;
                }

                const char *tag_end = (*scan_a == '>') ? scan_a : NULL;

                if (href_pos && tag_end && href_pos < tag_end) {
                    char quote = href_pos[5];
                    const char *url_start = href_pos + 6;
                    const char *url_end = strchr(url_start, quote);
                    const char *a_close = strstr(tag_end, "</a>");
                    if (!a_close) a_close = strstr(tag_end, "</A>");

                    if (url_end && a_close && (url_end - url_start) < 256 && (a_close - tag_end) < 140 && a_close > tag_end) {
                        /* Extraer URL completa (resolver relativas a Wikipedia si corresponde) */
                        char target_url[256] = "";
                        size_t ulen = (size_t)(url_end - url_start);
                        if (url_start[0] == '/' && url_start[1] == 'w') {
                            snprintf(target_url, sizeof(target_url), "https://es.wikipedia.org%.*s", (int)ulen, url_start);
                        } else if (url_start[0] == '/' && url_start[1] == '/') {
                            snprintf(target_url, sizeof(target_url), "https:%.*s", (int)ulen, url_start);
                        } else if (strncmp(url_start, "http", 4) == 0) {
                            snprintf(target_url, sizeof(target_url), "%.*s", (int)ulen, url_start);
                        }

                        /* Extraer texto/etiqueta del enlace limpia y decodificar entidades */
                        char label[128] = "";
                        size_t l_idx = 0;
                        for (const char *lp = tag_end + 1; lp < a_close && l_idx + 1 < sizeof(label); lp++) {
                            if (*lp == '<') {
                                while (*lp && *lp != '>') lp++;
                                if (!*lp) break;
                                continue;
                            }
                            char dec_c = 0;
                            size_t ent_len = eonsurf_decode_html_entity(lp, &dec_c);
                            if (ent_len > 0) {
                                label[l_idx++] = dec_c;
                                lp += (ent_len - 1);
                                continue;
                            }
                            if (*lp != '\r' && *lp != '\n' && *lp != '\t') {
                                label[l_idx++] = *lp;
                            }
                        }
                        label[l_idx] = '\0';

                        /* Si es un link web válido y texto no vacío, escribir en formato Markdown [Texto](URL) */
                        if (strlen(target_url) > 0 && strlen(label) > 0 &&
                            strstr(target_url, "redlink=1") == NULL &&
                            strstr(target_url, "action=edit") == NULL) {
                            size_t needed = strlen(label) + strlen(target_url) + 5;
                            if (out_idx + needed < max_body) {
                                out_idx += snprintf(&out_body[out_idx], max_body - out_idx, "[%s](%s)", label, target_url);
                                p = a_close + 4;
                                continue;
                            }
                        }
                    }
                }
            }

            /*
             * Saltar hasta el final real de la etiqueta HTML '>'.
             * IMPORTANTE: Si la etiqueta contiene cadenas con comillas como data-mw='{"...":">"}',
             * no detenerse en un '>' que esté dentro de comillas simples o dobles!
             */
            int in_quote = 0;
            char q = 0;
            while (*p) {
                if (!in_quote && (*p == '"' || *p == '\'')) {
                    in_quote = 1;
                    q = *p;
                } else if (in_quote && *p == q) {
                    in_quote = 0;
                } else if (!in_quote && *p == '>') {
                    p++; /* Consumir el '>' */
                    break;
                }
                p++;
            }
            continue;
        }

        /* Si estamos dentro de una tabla secundaria, ignorar texto plano */
        if (table_nest > 0) {
            p++;
            continue;
        }

        /* Texto visible */
        char dec_c = 0;
        size_t ent_len = eonsurf_decode_html_entity(p, &dec_c);
        if (ent_len > 0) {
            if (dec_c == ' ') {
                if (out_idx > 0 && out_body[out_idx - 1] != ' ' && out_body[out_idx - 1] != '\n') {
                    out_body[out_idx++] = ' ';
                }
            } else {
                out_body[out_idx++] = dec_c;
            }
            p += ent_len;
            continue;
        }

        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            if (out_idx > 0 && out_body[out_idx - 1] != ' ' && out_body[out_idx - 1] != '\n') {
                out_body[out_idx++] = ' ';
            }
        } else {
            out_body[out_idx++] = *p;
        }
        p++;
    }

    out_body[out_idx] = '\0';
    return (out_idx > 50);
}

#endif /* EONSURF_ARTICLE_EXTRACTOR_H */
