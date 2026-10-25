#ifndef EONSURF_ARTICLE_EXTRACTOR_H
#define EONSURF_ARTICLE_EXTRACTOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/*
 * Extrae texto de artículos web preservando hipervínculos estructurados:
 * Convierte etiquetas HTML <a href="...">Texto</a> en Markdown [Texto](URL)
 * para que el motor de maquetación los pinte en cian (#38BDF8) interactivos.
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
        "id=\"mw-content-text\"",
        "class=\"mw-parser-output\"",
        "<main",
        "<article",
        "role=\"main\"",
        "<body"
    };

    for (int k = 0; k < 6; k++) {
        const char *found = strstr(html_data, main_candidates[k]);
        if (found) { body_ptr = found; break; }
    }

    /* 3. Extraer párrafos limpios e hipervínculos como [Label](URL) */
    size_t out_idx = 0;
    const char *p = body_ptr;
    int in_skip_block = 0;
    char skip_name[16] = "";

    while (*p && out_idx + 64 < max_body) {
        if (*p == '<') {
            /* Descartar bloques completos */
            if (strncasecmp(p, "<script", 7) == 0) { in_skip_block = 1; strcpy(skip_name, "script"); }
            else if (strncasecmp(p, "<style", 6) == 0) { in_skip_block = 1; strcpy(skip_name, "style"); }
            else if (strncasecmp(p, "<table", 6) == 0) { in_skip_block = 1; strcpy(skip_name, "table"); }
            else if (strncasecmp(p, "<nav", 4) == 0) { in_skip_block = 1; strcpy(skip_name, "nav"); }
            else if (strncasecmp(p, "<header", 7) == 0) { in_skip_block = 1; strcpy(skip_name, "header"); }
            else if (strncasecmp(p, "<footer", 7) == 0) { in_skip_block = 1; strcpy(skip_name, "footer"); }

            /* Si estamos en bloque a descartar, avanzar hasta cierre </tag> */
            if (in_skip_block) {
                char close_str[24];
                snprintf(close_str, sizeof(close_str), "</%s>", skip_name);
                const char *close_tag = strstr(p, close_str);
                if (close_tag) {
                    p = close_tag + strlen(close_str);
                    in_skip_block = 0;
                    skip_name[0] = '\0';
                    continue;
                } else {
                    break;
                }
            }

            /* Detectar saltos de párrafo */
            if (strncasecmp(p, "</p>", 4) == 0 || strncasecmp(p, "<br", 3) == 0 || strncasecmp(p, "</h2>", 5) == 0) {
                if (out_idx > 0 && out_body[out_idx - 1] != '\n') {
                    out_body[out_idx++] = '\n';
                    out_body[out_idx++] = '\n';
                }
            }

            /* Detectar etiquetas de enlace: <a href="...">Texto</a> */
            if (strncasecmp(p, "<a ", 3) == 0) {
                const char *tag_end = strchr(p, '>');
                const char *href_pos = strstr(p, "href=\"");
                if (!href_pos || (tag_end && href_pos > tag_end)) {
                    href_pos = strstr(p, "href='");
                }

                if (href_pos && tag_end && href_pos < tag_end) {
                    char quote = href_pos[5];
                    const char *url_start = href_pos + 6;
                    const char *url_end = strchr(url_start, quote);
                    const char *a_close = strstr(tag_end, "</a>");
                    if (!a_close) a_close = strstr(tag_end, "</A>");

                    if (url_end && a_close && (url_end - url_start) < 256 && (a_close - tag_end) < 120 && a_close > tag_end) {
                        /* Extraer URL completa (resolver relativas de Wikipedia a https://es.wikipedia.org) */
                        char target_url[256] = "";
                        size_t ulen = (size_t)(url_end - url_start);
                        if (url_start[0] == '/' && url_start[1] == 'w') {
                            snprintf(target_url, sizeof(target_url), "https://es.wikipedia.org%.*s", (int)ulen, url_start);
                        } else if (url_start[0] == '/' && url_start[1] == '/') {
                            snprintf(target_url, sizeof(target_url), "https:%.*s", (int)ulen, url_start);
                        } else if (strncmp(url_start, "http", 4) == 0) {
                            snprintf(target_url, sizeof(target_url), "%.*s", (int)ulen, url_start);
                        }

                        /* Extraer texto/etiqueta del enlace limpia */
                        char label[128] = "";
                        size_t l_idx = 0;
                        for (const char *lp = tag_end + 1; lp < a_close && l_idx + 1 < sizeof(label); lp++) {
                            if (*lp == '<') {
                                while (*lp && *lp != '>') lp++;
                                if (!*lp) break;
                                continue;
                            }
                            if (*lp != '\r' && *lp != '\n' && *lp != '\t') {
                                label[l_idx++] = *lp;
                            }
                        }
                        label[l_idx] = '\0';

                        /* Si es un link web válido y texto no vacío, escribir en formato Markdown [Texto](URL) */
                        if (strlen(target_url) > 0 && strlen(label) > 1 &&
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

            /* Avanzar hasta el cierre '>' de la etiqueta actual */
            while (*p && *p != '>') p++;
            if (*p == '>') p++;
            continue;
        }

        /* Texto visible */
        if (!in_skip_block) {
            if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
                if (out_idx > 0 && out_body[out_idx - 1] != ' ' && out_body[out_idx - 1] != '\n') {
                    out_body[out_idx++] = ' ';
                }
            } else {
                out_body[out_idx++] = *p;
            }
        }
        p++;
    }

    out_body[out_idx] = '\0';
    return (out_idx > 50);
}

#endif /* EONSURF_ARTICLE_EXTRACTOR_H */
