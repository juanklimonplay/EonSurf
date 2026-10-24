#ifndef EONSURF_FSM_H
#define EONSURF_FSM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define EONSURF_MAX_DOM_NODES 3000

/* Extrae texto plano continuo y limpio en tiempo O(N) descartando scripts, estilos, comentarios y tags */
static inline size_t eonsurf_fsm_extract_text(
    const char *in_buf,
    size_t in_len,
    char *out_buf,
    size_t out_max
) {
    if (!in_buf || !out_buf || out_max == 0) return 0;

    size_t out_len = 0;
    size_t i = 0;
    int in_tag = 0;
    int in_quote = 0;
    int in_comment = 0;
    char skip_tag[16] = "";
    char tail_window[16] = "";
    char tag_name[32] = "";
    uint8_t tag_len = 0;
    int in_tag_name = 0;

    while (i < in_len && out_len + 1 < out_max) {
        char c = in_buf[i];

        /* Mantener ventana deslizante para detectar cierres de tags especiales o comentarios */
        memmove(tail_window, tail_window + 1, sizeof(tail_window) - 2);
        tail_window[sizeof(tail_window) - 2] = (char)tolower((unsigned char)c);
        tail_window[sizeof(tail_window) - 1] = '\0';

        /* 1. Descartar comentarios <!-- ... --> */
        if (in_comment) {
            if (strstr(tail_window, "-->") != NULL) {
                in_comment = 0;
            }
            i++;
            continue;
        }

        /* 2. Descartar bloques completos como <script>...</script>, <style>...</style>, etc. */
        if (skip_tag[0] != '\0') {
            char close_pattern[24];
            snprintf(close_pattern, sizeof(close_pattern), "</%s>", skip_tag);
            if (strstr(tail_window, close_pattern) != NULL) {
                skip_tag[0] = '\0';
            }
            i++;
            continue;
        }

        /* 3. Modo texto libre (fuera de etiquetas HTML) */
        if (!in_tag) {
            if (c == '<') {
                /* Detectar si es un comentario <!-- */
                if (i + 3 < in_len && in_buf[i+1] == '!' && in_buf[i+2] == '-' && in_buf[i+3] == '-') {
                    in_comment = 1;
                    i += 4;
                    continue;
                }
                /* Detectar si es un doctype <!DOCTYPE ... > */
                if (i + 8 < in_len && tolower((unsigned char)in_buf[i+1]) == '!' &&
                    tolower((unsigned char)in_buf[i+2]) == 'd' &&
                    tolower((unsigned char)in_buf[i+3]) == 'o' &&
                    tolower((unsigned char)in_buf[i+4]) == 'c' &&
                    tolower((unsigned char)in_buf[i+5]) == 't' &&
                    tolower((unsigned char)in_buf[i+6]) == 'y' &&
                    tolower((unsigned char)in_buf[i+7]) == 'p' &&
                    tolower((unsigned char)in_buf[i+8]) == 'e') {
                    while (i < in_len && in_buf[i] != '>') i++;
                    if (i < in_len) i++;
                    continue;
                }

                in_tag = 1;
                tag_len = 0;
                tag_name[0] = '\0';
                in_tag_name = 1;
                in_quote = 0;
            } else {
                if (c == '\r' || c == '\n' || c == '\t') c = ' ';
                if (c == ' ' && out_len > 0 && out_buf[out_len - 1] == ' ') {
                    /* Evitar acumular espacios repetidos */
                } else if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
                    out_buf[out_len++] = c;
                }
            }
        } else {
            /* 4. Modo dentro de etiqueta HTML < ... > */
            if (in_tag_name) {
                if (isalpha((unsigned char)c) || (c == '/' && tag_len == 0)) {
                    if (tag_len < sizeof(tag_name) - 1) {
                        tag_name[tag_len++] = (char)tolower((unsigned char)c);
                        tag_name[tag_len] = '\0';
                    }
                } else {
                    in_tag_name = 0;
                    const char *t = (tag_name[0] == '/') ? &tag_name[1] : tag_name;
                    if (tag_name[0] != '/') {
                        if (strcmp(t, "script") == 0 ||
                            strcmp(t, "style") == 0 ||
                            strcmp(t, "svg") == 0 ||
                            strcmp(t, "header") == 0 ||
                            strcmp(t, "nav") == 0 ||
                            strcmp(t, "footer") == 0 ||
                            strcmp(t, "aside") == 0) {
                            strncpy(skip_tag, t, sizeof(skip_tag) - 1);
                            skip_tag[sizeof(skip_tag) - 1] = '\0';
                            in_tag = 0;
                            i++;
                            continue;
                        }
                    }
                }
            }

            if (in_tag) {
                if (in_quote != 0) {
                    if (c == in_quote) {
                        in_quote = 0;
                    }
                } else {
                    if (c == '\"' || c == '\'') {
                        in_quote = c;
                    } else if (c == '>') {
                        in_tag = 0;
                        if (out_len > 0 && out_buf[out_len - 1] != ' ') {
                            out_buf[out_len++] = ' ';
                        }
                    }
                }
            }
        }

        i++;
    }

    out_buf[out_len] = '\0';
    return out_len;
}

#endif /* EONSURF_FSM_H */
