#ifndef EONSURF_MD_CLEAN_H
#define EONSURF_MD_CLEAN_H

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_CLEAN_LINKS 16

typedef struct {
    char label[128];
    char url[256];
    int16_t x, y, w, h; /* Bounding box en pantalla para colisiones */
} EonCleanLink;

/* 
 * Transforma Markdown crudo y HTML entities en texto estructurado editorial:
 * - Inserta saltos de línea dobles en #, ##, ###
 * - Pone viñetas ordenadas en listas (- )
 * - Limpia entidades comunes (&nbsp;, &amp;)
 * - Convierte [Texto](https://url) -> \x01Texto\x02 y guarda la URL en out_links
 */
static inline void eonsurf_format_markdown_and_links(const char *src, char *dst, size_t dst_max,
                                                     EonCleanLink *out_links, int *link_count, int max_links) {
    size_t di = 0;
    if (link_count) *link_count = 0;

    for (size_t si = 0; src[si] != '\0' && di + 8 < dst_max; ) {
        /* 0. Limpieza de entidades HTML simples */
        if (src[si] == '&' && strncmp(&src[si], "&nbsp;", 6) == 0) {
            dst[di++] = ' ';
            si += 6;
            continue;
        }
        if (src[si] == '&' && strncmp(&src[si], "&amp;", 5) == 0) {
            dst[di++] = '&';
            si += 5;
            continue;
        }

        /* 1. Detectar títulos Markdown (## o ###) en medio de una línea y bajar de renglón */
        if (si > 0 && (src[si - 1] == ' ' || src[si - 1] == '\n') && src[si] == '#' && (src[si + 1] == '#' || src[si + 1] == ' ')) {
            if (di > 0 && dst[di - 1] != '\n') dst[di++] = '\n';
            dst[di++] = '\n';
            while (src[si] == '#' || src[si] == ' ') si++;
            continue;
        }

        /* 2. Detectar ítems de lista (" - ") y poner viñeta en nueva línea */
        if (src[si] == ' ' && src[si + 1] == '-' && src[si + 2] == ' ') {
            if (di > 0 && dst[di - 1] != '\n') dst[di++] = '\n';
            dst[di++] = ' ';
            dst[di++] = '*';
            dst[di++] = ' ';
            si += 3;
            continue;
        }

        /* 3. Ocultar marcadores de negrita/cursiva (__ o **) */
        if ((src[si] == '_' && src[si + 1] == '_') || (src[si] == '*' && src[si + 1] == '*')) {
            si += 2;
            continue;
        }

        /* 4. Detectar enlaces Markdown: [Texto](URL) */
        if (src[si] == '[') {
            const char *close_bracket = strchr(&src[si + 1], ']');
            if (close_bracket && close_bracket[1] == '(') {
                const char *close_paren = strchr(close_bracket + 2, ')');
                if (close_paren && (close_paren - close_bracket) < 255) {
                    size_t label_len = (size_t)(close_bracket - (&src[si + 1]));
                    size_t url_len   = (size_t)(close_paren - (close_bracket + 2));

                    /* Guardar la URL en la tabla de links interactivos */
                    if (out_links && link_count && *link_count < max_links && label_len > 0 && url_len > 0) {
                        int idx = (*link_count)++;
                        size_t clen = label_len < 127 ? label_len : 127;
                        memcpy(out_links[idx].label, &src[si + 1], clen);
                        out_links[idx].label[clen] = '\0';

                        size_t ulen = url_len < 255 ? url_len : 255;
                        memcpy(out_links[idx].url, close_bracket + 2, ulen);
                        out_links[idx].url[ulen] = '\0';
                    }

                    /* Inyectar marca de color de link (\x01) + Texto + fin de color (\x02) */
                    dst[di++] = '\x01';
                    for (size_t k = 0; k < label_len && di + 4 < dst_max; k++) {
                        dst[di++] = src[si + 1 + k];
                    }
                    dst[di++] = '\x02';

                    /* Saltar toda la parte (https://...) para que no ensucie la pantalla */
                    si = (size_t)((close_paren + 1) - src);
                    continue;
                }
            }
        }

        dst[di++] = src[si++];
    }
    dst[di] = '\0';
}

#endif /* EONSURF_MD_CLEAN_H */
