#ifndef EONSURF_FSM_H
#define EONSURF_FSM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define EONSURF_MAX_DOM_NODES 3000

typedef enum {
    FSM_STATE_TEXT = 0,
    FSM_STATE_TAG_OPEN,
    FSM_STATE_TAG_NAME,
    FSM_STATE_TAG_ATTRS,
    FSM_STATE_COMMENT,
    FSM_STATE_SKIP_SCRIPT,
    FSM_STATE_SKIP_STYLE,
    FSM_STATE_SKIP_SVG,
    FSM_STATE_SKIP_BOILERPLATE, /* <nav>, <footer>, <aside>, <header> */
    FSM_STATE_TRUNCATED
} eon_fsm_state_t;

typedef struct {
    eon_fsm_state_t state;
    uint32_t node_count;
    int is_closing_tag;
    char tag_buf[32];
    uint8_t tag_len;
    char tail_window[16];
    char in_quote;
} __attribute__((aligned(128))) eon_fsm_ctx_t;

static inline void eonsurf_fsm_init(eon_fsm_ctx_t *ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(eon_fsm_ctx_t));
    ctx->state = FSM_STATE_TEXT;
}

static inline void eonsurf_fsm_push_window(eon_fsm_ctx_t *ctx, char c)
{
    memmove(ctx->tail_window, ctx->tail_window + 1, sizeof(ctx->tail_window) - 2);
    ctx->tail_window[sizeof(ctx->tail_window) - 2] = (char)tolower((unsigned char)c);
    ctx->tail_window[sizeof(ctx->tail_window) - 1] = '\0';
}

/* Extrae texto plano continuo y limpio en tiempo O(N) descartando scripts, estilos y etiquetas */
static inline size_t eonsurf_fsm_extract_text(
    const char *in_buf,
    size_t in_len,
    char *out_buf,
    size_t out_max
) {
    if (!in_buf || !out_buf || out_max == 0) return 0;
    
    eon_fsm_ctx_t ctx;
    eonsurf_fsm_init(&ctx);

    size_t out_len = 0;
    char skip_tag[16] = "";

    for (size_t i = 0; i < in_len && out_len + 1 < out_max; i++) {
        char c = in_buf[i];
        eonsurf_fsm_push_window(&ctx, c);

        /* Si estamos descartando un bloque especial (script, style, svg, header, nav, footer, aside) */
        if (skip_tag[0] != '\0') {
            char close_pattern[24];
            snprintf(close_pattern, sizeof(close_pattern), "</%s>", skip_tag);
            if (strstr(ctx.tail_window, close_pattern) != NULL) {
                skip_tag[0] = '\0';
                ctx.state = FSM_STATE_TEXT;
            }
            continue;
        }

        switch (ctx.state) {
            case FSM_STATE_TEXT:
                if (c == '<') {
                    ctx.state = FSM_STATE_TAG_OPEN;
                    ctx.is_closing_tag = 0;
                    ctx.tag_len = 0;
                    ctx.in_quote = 0;
                } else {
                    if (c == '\r' || c == '\n' || c == '\t') c = ' ';
                    if (c == ' ' && out_len > 0 && out_buf[out_len - 1] == ' ') {
                        /* Evitar acumular multiples espacios consecutivos */
                        continue;
                    }
                    if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
                        out_buf[out_len++] = c;
                    }
                }
                break;

            case FSM_STATE_TAG_OPEN:
                if (c == '!') {
                    ctx.state = FSM_STATE_COMMENT;
                } else if (c == '/') {
                    ctx.is_closing_tag = 1;
                    ctx.state = FSM_STATE_TAG_NAME;
                } else if (isalpha((unsigned char)c)) {
                    ctx.state = FSM_STATE_TAG_NAME;
                    ctx.tag_buf[ctx.tag_len++] = (char)tolower((unsigned char)c);
                } else {
                    ctx.state = FSM_STATE_TEXT;
                }
                break;

            case FSM_STATE_TAG_NAME:
                if (isalnum((unsigned char)c) || c == '-' || c == '_') {
                    if (ctx.tag_len < sizeof(ctx.tag_buf) - 1) {
                        ctx.tag_buf[ctx.tag_len++] = (char)tolower((unsigned char)c);
                    }
                } else {
                    ctx.tag_buf[ctx.tag_len] = '\0';

                    /* Detectar bloques que deben descartarse completamente con su contenido interior */
                    if (!ctx.is_closing_tag) {
                        if (strcmp(ctx.tag_buf, "script") == 0 ||
                            strcmp(ctx.tag_buf, "style") == 0 ||
                            strcmp(ctx.tag_buf, "svg") == 0 ||
                            strcmp(ctx.tag_buf, "header") == 0 ||
                            strcmp(ctx.tag_buf, "nav") == 0 ||
                            strcmp(ctx.tag_buf, "footer") == 0 ||
                            strcmp(ctx.tag_buf, "aside") == 0) {
                            strncpy(skip_tag, ctx.tag_buf, sizeof(skip_tag) - 1);
                            skip_tag[sizeof(skip_tag) - 1] = '\0';
                            ctx.state = FSM_STATE_TEXT;
                            break;
                        }
                    }

                    if (c == '>') {
                        ctx.state = FSM_STATE_TEXT;
                        if (out_len > 0 && out_buf[out_len - 1] != ' ') {
                            out_buf[out_len++] = ' ';
                        }
                    } else {
                        ctx.state = FSM_STATE_TAG_ATTRS;
                        if (c == '\"' || c == '\'') ctx.in_quote = c;
                    }
                }
                break;

            case FSM_STATE_TAG_ATTRS:
                if (ctx.in_quote != 0) {
                    if (c == ctx.in_quote) {
                        ctx.in_quote = 0;
                    }
                } else {
                    if (c == '\"' || c == '\'') {
                        ctx.in_quote = c;
                    } else if (c == '>') {
                        ctx.state = FSM_STATE_TEXT;
                        if (out_len > 0 && out_buf[out_len - 1] != ' ') {
                            out_buf[out_len++] = ' ';
                        }
                    }
                }
                break;

            case FSM_STATE_COMMENT:
                if (strstr(ctx.tail_window, "-->") != NULL) {
                    ctx.state = FSM_STATE_TEXT;
                }
                break;

            default:
                ctx.state = FSM_STATE_TEXT;
                break;
        }
    }

    out_buf[out_len] = '\0';
    return out_len;
}

#endif /* EONSURF_FSM_H */
