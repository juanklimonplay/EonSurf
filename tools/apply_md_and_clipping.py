with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Agregar #include "eonsurf_md_clean.h"
inc_target = '#include "eonsurf_layout.h"'
inc_replace = '''#include "eonsurf_layout.h"
#include "eonsurf_md_clean.h"'''
if inc_target in text:
    text = text.replace(inc_target, inc_replace, 1)

# 2. Agregar variables para enlaces limpios
var_target = 'static eon_page_link_t page_links[EONSURF_MAX_LINKS];'
var_replace = '''static eon_page_link_t page_links[EONSURF_MAX_LINKS];
static EonCleanLink md_links[MAX_CLEAN_LINKS];
static int md_links_count = 0;'''
if var_target in text:
    text = text.replace(var_target, var_replace, 1)

# 3. Función draw_text_clipped para la Infobox
helper_target = '/* Primitivas de renderizado grafico de alta fidelidad */'
helper_replace = '''/* Dibuja texto con recorte estricto para evitar desbordes en tarjetas */
static void draw_text_clipped(int x, int y, int s, const char *text, u32 c, int max_right_x) {
    int cur_x = x;
    int char_w = 9 * s;
    const char *p = text;

    while (*p) {
        if (cur_x + (char_w * 3) >= max_right_x && *(p + 1) != '\\0') {
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
            single[len] = '\\0';
            text_hd(cur_x, y, s, single, c);
        }
        cur_x += char_w;
        p = next_p;
    }
}

/* Primitivas de renderizado grafico de alta fidelidad */'''
if helper_target in text:
    text = text.replace(helper_target, helper_replace, 1)

# 4. Actualizar text_wrapped_hd para procesar \x01 y \x02 (Azul link / Negro texto)
old_wrap_code = '''/* Renderizado de párrafos UTF-8 con ajuste de línea y soporte de scroll vertical */
static int text_wrapped_hd(int start_x, int start_y, int scale, int max_width, int line_spacing, int clip_top, int clip_bottom, int scroll_offset, const char *t, u32 c)
{
    int cur_x = start_x;
    int cur_y = start_y - scroll_offset;
    int char_w = 9 * scale;
    int total_height = 0;
    const char *p = t;

    while (*p) {
        while (*p == ' ' || *p == '\\n') {
            if (*p == '\\n') {
                cur_x = start_x;
                cur_y += line_spacing;
            }
            p++;
        }
        if (!*p) break;

        /* Medir longitud de la palabra en caracteres visuales UTF-8 */
        const char *w_start = p;
        int word_chars = 0;
        const char *temp_p = p;
        while (*temp_p && *temp_p != ' ' && *temp_p != '\\n') {
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
                                fb[cur][py * W + px] = c;
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
}'''

new_wrap_code = '''/* Renderizado de párrafos UTF-8 con soporte de enlaces coloreados (\\x01 / \\x02) y scroll */
static int text_wrapped_hd(int start_x, int start_y, int scale, int max_width, int line_spacing, int clip_top, int clip_bottom, int scroll_offset, const char *t, u32 default_color)
{
    int cur_x = start_x;
    int cur_y = start_y - scroll_offset;
    int char_w = 9 * scale;
    int total_height = 0;
    const char *p = t;
    u32 current_color = default_color;

    while (*p) {
        while (*p == ' ' || *p == '\\n' || *p == '\\x01' || *p == '\\x02') {
            if (*p == '\\n') {
                cur_x = start_x;
                cur_y += line_spacing;
            } else if (*p == '\\x01') {
                current_color = 0x002563eb; /* Azul enlace vibrante */
            } else if (*p == '\\x02') {
                current_color = default_color; /* Color de texto normal */
            }
            p++;
        }
        if (!*p) break;

        /* Medir longitud de la palabra ignorando marcas de control */
        const char *w_start = p;
        int word_chars = 0;
        const char *temp_p = p;
        while (*temp_p && *temp_p != ' ' && *temp_p != '\\n') {
            if (*temp_p == '\\x01' || *temp_p == '\\x02') {
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
                if (*p == '\\x01') {
                    current_color = 0x002563eb;
                    p++;
                    continue;
                }
                if (*p == '\\x02') {
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
}'''
if old_wrap_code in text:
    text = text.replace(old_wrap_code, new_wrap_code, 1)

# 5. Formatear con eonsurf_format_markdown_and_links en navigate_to
old_nav_code = '''            char clean_body[7800] = "";
            eonsurf_fsm_extract_text_and_links(chunk.data, chunk.size, clean_body, sizeof(clean_body), page_links, &page_links_count);

            if (strlen(clean_body) > 0) {
                snprintf(page_body, sizeof(page_body), "%s", clean_body);
            } else {
                snprintf(page_body, sizeof(page_body), "Página recibida (Tamaño: %u bytes).", (unsigned int)chunk.size);
            }'''

new_nav_code = '''            char raw_extracted[7800] = "";
            eonsurf_fsm_extract_text_and_links(chunk.data, chunk.size, raw_extracted, sizeof(raw_extracted), page_links, &page_links_count);

            if (strlen(raw_extracted) > 0) {
                /* Formatear Markdown limpio inyectando saltos en ## y marcas de enlace */
                eonsurf_format_markdown_and_links(raw_extracted, page_body, sizeof(page_body), md_links, &md_links_count, MAX_CLEAN_LINKS);
            } else {
                snprintf(page_body, sizeof(page_body), "Página recibida (Tamaño: %u bytes).", (unsigned int)chunk.size);
            }'''
if old_nav_code in text:
    text = text.replace(old_nav_code, new_nav_code, 1)

# 6. Coordenadas del Header sin colisión
old_hdr = '''        /* Barra de Direcciones / Búsqueda con píldora y borde de brillo cian */
        int url_bar_w = W - 390;
        draw_rounded_pill(fb[cur], W, 112, 11, url_bar_w, 38, 8, 0x00030712, 0x000284c7);

        snprintf(buf, sizeof(buf), "%s", current_url);
        text_hd(126, 22, 1, buf, 0x0038bdf8);

        /* Badge oficial Cell B.E. • 60 FPS */
        draw_rounded_pill(fb[cur], W, W - 264, 13, 148, 34, 6, 0x000f172a, 0x0006b6d4);
        text_hd(W - 252, 22, 1, "Cell B.E. | 60fps", 0x0038bdf8);

        /* Indicador de red activa Online/Offline */
        draw_rounded_pill(fb[cur], W, W - 104, 13, 84, 34, 6, net_initialized ? 0x00064e3b : 0x00450a0a, net_initialized ? 0x0010b981 : 0x00ef4444);
        text_hd(W - 90, 22, 1, net_initialized ? "ONLINE" : "OFFLINE", 0x00ffffff);'''

new_hdr = '''        /* Barra de Direcciones / Búsqueda con píldora y borde de brillo cian */
        int url_bar_w = W - 440;
        draw_rounded_pill(fb[cur], W, 112, 11, url_bar_w, 38, 8, 0x00030712, 0x000284c7);

        snprintf(buf, sizeof(buf), "%s", current_url);
        draw_text_clipped(126, 22, 1, buf, 0x0038bdf8, 112 + url_bar_w - 15);

        /* Badge oficial Cell B.E. | 60 FPS (Desplazado a la izquierda sin solapamiento) */
        draw_rounded_pill(fb[cur], W, W - 310, 13, 175, 34, 6, 0x000f172a, 0x0006b6d4);
        text_hd(W - 298, 22, 1, "Cell B.E. | 60fps", 0x0038bdf8);

        /* Indicador de red activa Online/Offline */
        draw_rounded_pill(fb[cur], W, W - 120, 13, 98, 34, 6, net_initialized ? 0x00064e3b : 0x00450a0a, net_initialized ? 0x0010b981 : 0x00ef4444);
        text_hd(W - 104, 22, 1, net_initialized ? "ONLINE" : "OFFLINE", 0x00ffffff);'''
if old_hdr in text:
    text = text.replace(old_hdr, new_hdr, 1)

# 7. Clipping estricto en la tabla derecha
old_infobox_rows = '''            char val_buf[26];
            strncpy(val_buf, g_layout.info_rows[r].value, sizeof(val_buf) - 1);
            val_buf[sizeof(val_buf) - 1] = '\\0';
            text_hd(col_right_x + 18, row_y + 3, 1, g_layout.info_rows[r].key, 0x000f172a);
            text_hd(col_right_x + 115, row_y + 3, 1, val_buf, 0x002563eb);'''

new_infobox_rows = '''            text_hd(col_right_x + 18, row_y + 3, 1, g_layout.info_rows[r].key, 0x000f172a);
            draw_text_clipped(col_right_x + 115, row_y + 3, 1, g_layout.info_rows[r].value, 0x002563eb, col_right_x + col_right_w - 16);'''
if old_infobox_rows in text:
    text = text.replace(old_infobox_rows, new_infobox_rows, 1)

with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
    f.write(text)

print("Integrated Markdown Formatter, URL clipping, Header spacing and Link coloring!")
