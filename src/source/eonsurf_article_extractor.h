#ifndef EONSURF_ARTICLE_EXTRACTOR_H
#define EONSURF_ARTICLE_EXTRACTOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/*
 * Extrae texto de artículos web eliminando código innecesario
 * Salta automáticamente barras laterales, menús de navegación y tablas pesadas
 * para dejar un texto de lectura limpio de alta calidad.
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
            size_t tlen = (size_t)(title_end - title_start);
            if (tlen >= max_title) tlen = max_title - 1;
            strncpy(out_title, title_start, tlen);
            out_title[tlen] = '\0';
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

    /* 3. Extraer párrafos limpios ignorando scripts, estilos y tablas de metadatos */
    size_t out_idx = 0;
    const char *p = body_ptr;
    int in_tag = 0;
    int in_skip_block = 0;
    char skip_name[16] = "";

    while (*p && out_idx + 16 < max_body) {
        if (!in_tag) {
            if (*p == '<') {
                in_tag = 1;
                /* Comprobar si entramos a un bloque que se debe descartar por completo */
                if (strncasecmp(p, "<script", 7) == 0) { in_skip_block = 1; strcpy(skip_name, "script"); }
                else if (strncasecmp(p, "<style", 6) == 0) { in_skip_block = 1; strcpy(skip_name, "style"); }
                else if (strncasecmp(p, "<table", 6) == 0) { in_skip_block = 1; strcpy(skip_name, "table"); }
                else if (strncasecmp(p, "<nav", 4) == 0) { in_skip_block = 1; strcpy(skip_name, "nav"); }
                else if (strncasecmp(p, "<header", 7) == 0) { in_skip_block = 1; strcpy(skip_name, "header"); }
                else if (strncasecmp(p, "<footer", 7) == 0) { in_skip_block = 1; strcpy(skip_name, "footer"); }

                /* Detectar saltos de párrafo en el texto */
                if (strncasecmp(p, "</p>", 4) == 0 || strncasecmp(p, "<br", 3) == 0 || strncasecmp(p, "</h2>", 5) == 0) {
                    if (out_idx > 0 && out_body[out_idx - 1] != '\n') {
                        out_body[out_idx++] = '\n';
                        out_body[out_idx++] = '\n';
                    }
                }
            } else {
                if (!in_skip_block) {
                    /* Filtrar espacios repetidos */
                    if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
                        if (out_idx > 0 && out_body[out_idx - 1] != ' ' && out_body[out_idx - 1] != '\n') {
                            out_body[out_idx++] = ' ';
                        }
                    } else {
                        out_body[out_idx++] = *p;
                    }
                }
            }
        } else {
            if (*p == '>') {
                in_tag = 0;
            } else if (in_skip_block) {
                char close_str[24];
                snprintf(close_str, sizeof(close_str), "</%s>", skip_name);
                if (strncasecmp(p, close_str, strlen(close_str)) == 0) {
                    in_skip_block = 0;
                    skip_name[0] = '\0';
                }
            }
        }
        p++;
    }

    out_body[out_idx] = '\0';
    return (out_idx > 50);
}

#endif /* EONSURF_ARTICLE_EXTRACTOR_H */
