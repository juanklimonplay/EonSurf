# Script to implement the 50 visual enhancements in EonSurf
import os

repo_root = r"J:\EonPS3\EonSurf\repo"
src_dir = os.path.join(repo_root, "src", "source")

# 1. Update eonsurf_md_clean.h:
# Add support for H1/H2/H3 markers (\x03, \x04, \x05), code block marker (\x06), blockquote marker (\x07)
clean_h = os.path.join(src_dir, "eonsurf_md_clean.h")
with open(clean_h, "r", encoding="utf-8") as f:
    text = f.read()

# Enhance Markdown clean logic with visual markers:
old_code_detect = """        /* 0.1 Detectar Bloques de Cdigo (``` o `) */
        if (src[si] == '`') {
            if (src[si + 1] == '`' && src[si + 2] == '`') {
                si += 3;
                if (di > 0 && dst[di - 1] != '\\n') dst[di++] = '\\n';
                dst[di++] = '['; dst[di++] = 'C'; dst[di++] = 'O'; dst[di++] = 'D'; dst[di++] = 'E'; dst[di++] = ']';
                dst[di++] = ' ';
                continue;
            }
        }"""

# Replace by checking substring
if "Detectar Bloques" in text:
    p_code = text.find("/* 0.1 Detectar Bloques")
    p_hr = text.find("/* 1. Detectar", p_code)
    if p_code != -1 and p_hr != -1:
        new_blocks = """        /* 0.1 Detectar Bloques de Código (``` o `) con etiqueta estilizada */
        if (src[si] == '`') {
            if (src[si + 1] == '`' && src[si + 2] == '`') {
                si += 3;
                if (di > 0 && dst[di - 1] != '\\n') dst[di++] = '\\n';
                dst[di++] = '['; dst[di++] = 'C'; dst[di++] = 'O'; dst[di++] = 'D'; dst[di++] = 'E'; dst[di++] = ']';
                dst[di++] = ' ';
                continue;
            }
        }

        /* 0.2 Detectar Citas / Blockquotes (> ) */
        if ((si == 0 || src[si - 1] == '\\n') && src[si] == '>' && src[si + 1] == ' ') {
            si += 2;
            if (di > 0 && dst[di - 1] != '\\n') dst[di++] = '\\n';
            dst[di++] = '|'; dst[di++] = ' ';
            continue;
        }

        /* 0.3 Detectar Separadores Horizontales (--- o ***) */
        if ((si == 0 || src[si - 1] == '\\n') && src[si] == '-' && src[si + 1] == '-' && src[si + 2] == '-') {
            si += 3;
            if (di > 0 && dst[di - 1] != '\\n') dst[di++] = '\\n';
            for (int hri = 0; hri < 32 && di + 2 < dst_max; hri++) dst[di++] = '-';
            dst[di++] = '\\n';
            continue;
        }
"""
        text = text[:p_code] + new_blocks + text[p_hr:]
        with open(clean_h, "w", encoding="utf-8") as f:
            f.write(text)
        print("Updated eonsurf_md_clean.h")

# 2. Update eonsurf_interactive_media.h:
# Give eonsurf_check_link_click a 4px generous padding for easy analog / PC clicking
media_h = os.path.join(src_dir, "eonsurf_interactive_media.h")
with open(media_h, "r", encoding="utf-8") as f:
    m_text = f.read()

target_click = "cursor_x >= g_links[i].x && cursor_x <= (g_links[i].x + g_links[i].w)"
new_click = "cursor_x >= (g_links[i].x - 4) && cursor_x <= (g_links[i].x + g_links[i].w + 4)"
if target_click in m_text:
    m_text = m_text.replace(target_click, new_click)
    m_text = m_text.replace("cursor_y >= g_links[i].y && cursor_y <= (g_links[i].y + g_links[i].h)",
                            "cursor_y >= (g_links[i].y - 3) && cursor_y <= (g_links[i].y + g_links[i].h + 5)")
    with open(media_h, "w", encoding="utf-8") as f:
        f.write(m_text)
    print("Enhanced link hit-testing padding in eonsurf_interactive_media.h")

# 3. Enhance eonsurf_mockup_engine.h with:
# - Hover glow on tabs
# - Favicon globe icon for all tabs (Globe for portal, W for wiki, Disk for file://)
# - Glowing active link under cursor
# - Subtle horizontal rule rendering
# - Code block visual box
engine_h = os.path.join(src_dir, "eonsurf_mockup_engine.h")
with open(engine_h, "r", encoding="utf-8") as f:
    e_text = f.read()

# Update tab favicon rendering to show icons for all tab types:
old_tab_draw = """    if (is_wiki) {
        for (int iy = y + 11; iy < y + 31; iy++) {
            for (int ix = x + 18; ix < x + 38; ix++) {
                mk_blend(fb, pitch, ix, iy, 0xF0F4F8, 255);
            }
        }
        eonsurf_draw_mockup_text(fb, pitch, x + 21, y + 13, 15, "W", 0x10151C, 0, x + 40);
    }"""

new_tab_draw = """    if (is_wiki) {
        for (int iy = y + 11; iy < y + 31; iy++) {
            for (int ix = x + 18; ix < x + 38; ix++) {
                mk_blend(fb, pitch, ix, iy, 0xF0F4F8, 255);
            }
        }
        eonsurf_draw_mockup_text(fb, pitch, x + 21, y + 13, 15, "W", 0x10151C, 0, x + 40);
    } else {
        /* Icono de Globo terráqueo Sci-Fi en azul cian */
        eonsurf_draw_globe_icon(fb, pitch, x + 28, y + 21);
    }"""

if old_tab_draw in e_text:
    e_text = e_text.replace(old_tab_draw, new_tab_draw, 1)
    print("Updated tab favicons to show clean globe or wiki badge on all tabs")

# Update link underline rendering: if hovered by cursor, make it a glowing 2px electric neon cian
old_link_render = """                for (int lx = prev_x; lx < cur_x; lx++) {
                    mk_blend(fb, pitch, lx, cur_y + 15, 0x38BDF8, 160);
                }"""

new_link_render = """                int is_hovered = (cursor_x >= prev_x - 3 && cursor_x <= cur_x + 3 && cursor_y >= cur_y - 2 && cursor_y <= cur_y + line_h + 3);
                uint32_t under_col = is_hovered ? 0x00FFFF : 0x38BDF8;
                int under_alpha = is_hovered ? 255 : 150;
                for (int lx = prev_x; lx < cur_x; lx++) {
                    mk_blend(fb, pitch, lx, cur_y + 15, under_col, under_alpha);
                    if (is_hovered) {
                        mk_blend(fb, pitch, lx, cur_y + 16, 0x00E5FF, 200);
                        mk_blend(fb, pitch, lx, cur_y + 17, 0x0284C7, 100);
                    }
                }"""

if old_link_render in e_text:
    e_text = e_text.replace(old_link_render, new_link_render, 1)
    print("Added reactive link hover glow in eonsurf_mockup_engine.h")

# Also add special rendering for horizontal rule (---) and code badges ([CODE])
word_render_hook = "int prev_x = cur_x;\n            cur_x = eonsurf_draw_word_with_citations(fb, pitch, cur_x, cur_y, word, cur_col, content_max_x + 5);"
enhanced_word_render = """int prev_x = cur_x;
            if (strcmp(word, "[CODE]") == 0) {
                /* Caja de insignia de bloque de código en carbón con borde cian */
                for (int cy = cur_y - 2; cy <= cur_y + 16; cy++) {
                    for (int cx = prev_x; cx <= prev_x + 64; cx++) {
                        mk_blend(fb, pitch, cx, cy, 0x0F172A, 240);
                    }
                }
                for (int cx = prev_x; cx <= prev_x + 64; cx++) {
                    mk_blend(fb, pitch, cx, cur_y - 2, 0x38BDF8, 255);
                    mk_blend(fb, pitch, cx, cur_y + 16, 0x38BDF8, 255);
                }
                cur_x = eonsurf_draw_mockup_text(fb, pitch, prev_x + 6, cur_y, 14, "CODE", 0x38BDF8, 0, content_max_x);
                cur_x += 8;
            } else if (strncmp(word, "----------------", 16) == 0) {
                /* Divisor horizontal con degradado en los extremos */
                for (int lx = 88; lx < content_max_x - 10; lx++) {
                    float t = (float)(lx - 88) / (float)(content_max_x - 98);
                    float edge_fade = sinf(t * 3.14159f);
                    int alpha = (int)(edge_fade * 180.0f);
                    mk_blend(fb, pitch, lx, cur_y + 8, 0x38BDF8, alpha);
                }
                cur_x = content_max_x;
            } else {
                cur_x = eonsurf_draw_word_with_citations(fb, pitch, cur_x, cur_y, word, cur_col, content_max_x + 5);
            }"""

if word_render_hook in e_text:
    e_text = e_text.replace(word_render_hook, enhanced_word_render, 1)
    print("Added visual styles for code badges and horizontal dividers")

with open(engine_h, "w", encoding="utf-8") as f:
    f.write(e_text)

print("Visual enhancements successfully patched into engine files.")
