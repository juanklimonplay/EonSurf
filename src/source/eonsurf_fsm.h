#ifndef EONSURF_FSM_H
#define EONSURF_FSM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define EONSURF_MAX_DOM_NODES 3000
#define EONSURF_MAX_LINKS 6

typedef struct {
    char text[48];
    char url[256];
    int active;
} eon_page_link_t;

/* Extrae texto plano continuo y limpio en tiempo O(N) y captura hasta EONSURF_MAX_LINKS enlaces reales */
static inline size_t eonsurf_fsm_extract_text_and_links(
    const char *in_buf,
    size_t in_len,
    char *out_buf,
    size_t out_max,
    eon_page_link_t *links_out,
    size_t *links_count_out
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

    /* Detección de links <a href="...">texto</a> */
    char cur_link_url[256] = "";
    int capturing_link_text = 0;
    char cur_link_text[48] = "";
    uint8_t link_text_len = 0;
    size_t num_links = 0;

    if (links_out && links_count_out) {
        memset(links_out, 0, sizeof(eon_page_link_t) * EONSURF_MAX_LINKS);
        *links_count_out = 0;
    }

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
                /* Si estábamos capturando texto de enlace y encontramos un tag de cierre </a> */
                if (capturing_link_text && i + 3 < in_len && in_buf[i+1] == '/' && tolower((unsigned char)in_buf[i+2]) == 'a' && in_buf[i+3] == '>') {
                    if (links_out && num_links < EONSURF_MAX_LINKS && link_text_len >= 3) {
                        /* Guardar enlace si es significativo y no duplicado */
                        int dup = 0;
                        for (size_t k = 0; k < num_links; k++) {
                            if (strcmp(links_out[k].url, cur_link_url) == 0) { dup = 1; break; }
                        }
                        if (!dup && strstr(cur_link_url, "Archivo:") == NULL && strstr(cur_link_url, "Special:") == NULL) {
                            strncpy(links_out[num_links].text, cur_link_text, sizeof(links_out[num_links].text) - 1);
                            strncpy(links_out[num_links].url, cur_link_url, sizeof(links_out[num_links].url) - 1);
                            links_out[num_links].active = 1;
                            num_links++;
                        }
                    }
                    capturing_link_text = 0;
                    cur_link_url[0] = '\0';
                    cur_link_text[0] = '\0';
                    link_text_len = 0;
                }

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
                    /* Si estamos dentro de un link, acumular su texto para el botón */
                    if (capturing_link_text && link_text_len + 1 < sizeof(cur_link_text)) {
                        cur_link_text[link_text_len++] = c;
                        cur_link_text[link_text_len] = '\0';
                    }
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

            /* Detectar atributos href="https://..." en etiquetas <a ...> */
            if (in_tag && strcmp(tag_name, "a") == 0 && !capturing_link_text && num_links < EONSURF_MAX_LINKS) {
                if (i + 5 < in_len && tolower((unsigned char)in_buf[i]) == 'h' &&
                    tolower((unsigned char)in_buf[i+1]) == 'r' &&
                    tolower((unsigned char)in_buf[i+2]) == 'e' &&
                    tolower((unsigned char)in_buf[i+3]) == 'f' &&
                    in_buf[i+4] == '=') {
                    char quote_char = in_buf[i+5];
                    if (quote_char == '\"' || quote_char == '\'') {
                        size_t start_url = i + 6;
                        size_t end_url = start_url;
                        while (end_url < in_len && in_buf[end_url] != quote_char && (end_url - start_url) < sizeof(cur_link_url) - 1) {
                            end_url++;
                        }
                        size_t url_size = end_url - start_url;
                        if (url_size > 7 && strncmp(&in_buf[start_url], "http", 4) == 0) {
                            memcpy(cur_link_url, &in_buf[start_url], url_size);
                            cur_link_url[url_size] = '\0';
                            capturing_link_text = 1;
                            link_text_len = 0;
                            cur_link_text[0] = '\0';
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
    if (links_count_out) *links_count_out = num_links;
    return out_len;
}

#endif /* EONSURF_FSM_H */
