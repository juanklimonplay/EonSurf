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
    FSM_STATE_SKIP_BOILERPLATE, /* <nav>, <footer>, <aside> */
    FSM_STATE_TRUNCATED
} eon_fsm_state_t;

typedef struct {
    eon_fsm_state_t state;
    uint32_t node_count;
    int is_closing_tag;
    char tag_buf[16];
    uint8_t tag_len;
    char tail_window[12];
} __attribute__((aligned(128))) eon_fsm_ctx_t;

static inline void eonsurf_fsm_init(eon_fsm_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(eon_fsm_ctx_t));
    ctx->state = FSM_STATE_TEXT;
}

static inline void eonsurf_fsm_push_window(eon_fsm_ctx_t *ctx, char c)
{
    memmove(ctx->tail_window, ctx->tail_window + 1, 10);
    ctx->tail_window[10] = (char)tolower((unsigned char)c);
    ctx->tail_window[11] = '\0';
}

/* Filtra un bloque de red entrante de 128 KB en tiempo lineal O(N) */
static inline size_t eonsurf_fsm_filter_chunk(
    eon_fsm_ctx_t *ctx,
    const char *in_buf,
    size_t in_len,
    char *out_buf,
    size_t out_max
) {
    if (!ctx || !in_buf || !out_buf || out_max == 0) return 0;
    size_t out_len = 0;

    for (size_t i = 0; i < in_len; i++) {
        char c = in_buf[i];
        eonsurf_fsm_push_window(ctx, c);

        if (ctx->state == FSM_STATE_TRUNCATED) {
            break;
        }

        switch (ctx->state) {
            case FSM_STATE_TEXT:
                if (c == '<') {
                    ctx->state = FSM_STATE_TAG_OPEN;
                    ctx->is_closing_tag = 0;
                    ctx->tag_len = 0;
                } else {
                    if (out_len + 1 < out_max) out_buf[out_len++] = c;
                }
                break;

            case FSM_STATE_TAG_OPEN:
                if (c == '!') {
                    ctx->state = FSM_STATE_COMMENT;
                } else if (c == '/') {
                    ctx->is_closing_tag = 1;
                    ctx->state = FSM_STATE_TAG_NAME;
                } else if (isalpha((unsigned char)c)) {
                    ctx->state = FSM_STATE_TAG_NAME;
                    ctx->tag_buf[ctx->tag_len++] = (char)tolower((unsigned char)c);
                } else {
                    ctx->state = FSM_STATE_TEXT;
                    if (out_len + 1 < out_max) out_buf[out_len++] = c;
                }
                break;

            case FSM_STATE_TAG_NAME:
                if (isalnum((unsigned char)c)) {
                    if (ctx->tag_len < sizeof(ctx->tag_buf) - 1) {
                        ctx->tag_buf[ctx->tag_len++] = (char)tolower((unsigned char)c);
                    }
                } else {
                    ctx->tag_buf[ctx->tag_len] = '\0';

                    /* Descarte de scripts y estilos pesados */
                    if (!ctx->is_closing_tag && strcmp(ctx->tag_buf, "script") == 0) {
                        ctx->state = FSM_STATE_SKIP_SCRIPT;
                    } else if (!ctx->is_closing_tag && strcmp(ctx->tag_buf, "style") == 0) {
                        ctx->state = FSM_STATE_SKIP_STYLE;
                    } else if (!ctx->is_closing_tag && strcmp(ctx->tag_buf, "svg") == 0) {
                        ctx->state = FSM_STATE_SKIP_SVG;
                    } else if (!ctx->is_closing_tag && (strcmp(ctx->tag_buf, "nav") == 0 ||
                                                        strcmp(ctx->tag_buf, "footer") == 0 ||
                                                        strcmp(ctx->tag_buf, "aside") == 0)) {
                        ctx->state = FSM_STATE_SKIP_BOILERPLATE;
                    } else {
                        if (!ctx->is_closing_tag) {
                            ctx->node_count++;
                            if (ctx->node_count >= EONSURF_MAX_DOM_NODES) {
                                ctx->state = FSM_STATE_TRUNCATED;
                                const char *synth_close = "</body></html>";
                                size_t sc_len = strlen(synth_close);
                                if (out_len + sc_len < out_max) {
                                    memcpy(out_buf + out_len, synth_close, sc_len);
                                    out_len += sc_len;
                                }
                                break;
                            }
                        }
                        if (out_len + 1 < out_max) out_buf[out_len++] = '<';
                        if (ctx->is_closing_tag && out_len + 1 < out_max) out_buf[out_len++] = '/';
                        for (uint8_t k = 0; k < ctx->tag_len; k++) {
                            if (out_len + 1 < out_max) out_buf[out_len++] = ctx->tag_buf[k];
                        }
                        if (out_len + 1 < out_max) out_buf[out_len++] = c;
                        ctx->state = (c == '>') ? FSM_STATE_TEXT : FSM_STATE_TAG_ATTRS;
                    }
                }
                break;

            case FSM_STATE_TAG_ATTRS:
                if (c == '>') {
                    ctx->state = FSM_STATE_TEXT;
                }
                if (out_len + 1 < out_max) out_buf[out_len++] = c;
                break;

            case FSM_STATE_COMMENT:
                if (strstr(ctx->tail_window, "-->")) {
                    ctx->state = FSM_STATE_TEXT;
                }
                break;

            case FSM_STATE_SKIP_SCRIPT:
                if (strstr(ctx->tail_window, "</script>")) {
                    ctx->state = FSM_STATE_TEXT;
                }
                break;

            case FSM_STATE_SKIP_STYLE:
                if (strstr(ctx->tail_window, "</style>")) {
                    ctx->state = FSM_STATE_TEXT;
                }
                break;

            case FSM_STATE_SKIP_SVG:
                if (strstr(ctx->tail_window, "</svg>")) {
                    ctx->state = FSM_STATE_TEXT;
                }
                break;

            case FSM_STATE_SKIP_BOILERPLATE:
                if (strstr(ctx->tail_window, "</nav>") ||
                    strstr(ctx->tail_window, "</footer>") ||
                    strstr(ctx->tail_window, "</aside>")) {
                    ctx->state = FSM_STATE_TEXT;
                }
                break;

            default:
                ctx->state = FSM_STATE_TEXT;
                break;
        }
    }

    return out_len;
}

#endif /* EONSURF_FSM_H */
