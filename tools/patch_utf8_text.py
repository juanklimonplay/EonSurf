with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    text = f.read()

old_text_hd = '''/* Renderizado con tipografia HD 8x16 de NetSurf con soporte mayúsculas y minúsculas reales */
static void text_hd(int x, int y, int s, const char *t, u32 c)
{
    for (; *t; t++, x += 9 * s) {
        uint8_t ch = (uint8_t)*t;
        if (ch > 127) ch = 32;
        for (int row = 0; row < 16; row++) {
            uint8_t row_bits = font8x16_data[ch][row];
            if (row_bits == 0) continue;
            for (int col = 0; col < 8; col++) {
                if (row_bits & (1 << (7 - col))) {
                    if (s == 1) {
                        int px = x + col;
                        int py = y + row;
                        if (px >= 0 && px < (int)W && py >= 0 && py < (int)H) {
                            fb[cur][py * W + px] = c;
                        }
                    } else {
                        rect(x + col * s, y + row * s, s, s, c);
                    }
                }
            }
        }
    }
}

/* Renderizado de parrafos con ajuste de linea y soporte de scroll vertical */
static int text_wrapped_hd(int start_x, int start_y, int scale, int max_width, int line_spacing, int clip_top, int clip_bottom, int scroll_offset, const char *t, u32 c)
{
    int cur_x = start_x;
    int cur_y = start_y - scroll_offset;
    int char_w = 9 * scale;
    int total_height = 0;
    const char *p = t;

    while (*p) {
        while (*p == ' ' || *p == '\n') {
            if (*p == '\n') {
                cur_x = start_x;
                cur_y += line_spacing;
            }
            p++;
        }
        if (!*p) break;

        const char *w_start = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        int w_len = p - w_start;
        int w_pix = w_len * char_w;

        if (cur_x + w_pix > start_x + max_width && cur_x > start_x) {
            cur_x = start_x;
            cur_y += line_spacing;
        }

        if (cur_y + 16 * scale >= clip_top && cur_y <= clip_bottom) {
            for (int i = 0; i < w_len; i++) {
                uint8_t ch = (uint8_t)w_start[i];
                if (ch > 127) ch = 32;
                for (int row = 0; row < 16; row++) {
                    uint8_t row_bits = font8x16_data[ch][row];
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
            cur_x += w_len * char_w;
        }

        cur_x += char_w;
        if (cur_y + line_spacing - (start_y - scroll_offset) > total_height) {
            total_height = cur_y + line_spacing - (start_y - scroll_offset);
        }
    }

    return total_height;
}'''

new_text_hd = '''/* Renderizado con tipografía HD 8x16 de NetSurf con decodificación UTF-8 real (tildes, Ñ, símbolos) */
static void text_hd(int x, int y, int s, const char *t, u32 c)
{
    const char *p = t;
    while (*p) {
        uint32_t cp = utf8_next_codepoint(&p);
        if (cp == 0) break;
        const uint8_t *glyph = font8x16_get_glyph(cp);

        for (int row = 0; row < 16; row++) {
            uint8_t row_bits = glyph[row];
            if (row_bits == 0) continue;
            for (int col = 0; col < 8; col++) {
                if (row_bits & (1 << (7 - col))) {
                    if (s == 1) {
                        int px = x + col;
                        int py = y + row;
                        if (px >= 0 && px < (int)W && py >= 0 && py < (int)H) {
                            fb[cur][py * W + px] = c;
                        }
                    } else {
                        rect(x + col * s, y + row * s, s, s, c);
                    }
                }
            }
        }
        x += 9 * s;
    }
}

/* Renderizado de párrafos UTF-8 con ajuste de línea y soporte de scroll vertical */
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

if old_text_hd in text:
    text = text.replace(old_text_hd, new_text_hd, 1)
    with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
        f.write(text)
    print("Replaced text_hd with UTF-8 renderer")
else:
    print("old_text_hd not found")
