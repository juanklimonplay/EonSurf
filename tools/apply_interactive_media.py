with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Agregar #include "eonsurf_interactive_media.h"
inc_target = '#include "eonsurf_md_clean.h"'
inc_replace = '''#include "eonsurf_md_clean.h"
#include "eonsurf_interactive_media.h"'''
if inc_target in text:
    text = text.replace(inc_target, inc_replace, 1)

# 2. Descargar imagen real y guardarla en g_infobox_tex dentro de navigate_to
old_img_fetch = '''            if (strlen(raw_extracted) > 0) {
                /* Formatear Markdown limpio inyectando saltos en ## y marcas de enlace */
                eonsurf_format_markdown_and_links(raw_extracted, page_body, sizeof(page_body), md_links, &md_links_count, MAX_CLEAN_LINKS);
            } else {
                snprintf(page_body, sizeof(page_body), "Página recibida (Tamaño: %u bytes).", (unsigned int)chunk.size);
            }'''

new_img_fetch = '''            if (strlen(raw_extracted) > 0) {
                /* Formatear Markdown limpio inyectando saltos en ## y marcas de enlace */
                eonsurf_format_markdown_and_links(raw_extracted, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
                
                /* Extraer la primera imagen de la página y descargarla para el Infobox */
                char detected_img_url[256] = "";
                if (eonsurf_find_first_image_url(chunk.data, detected_img_url, sizeof(detected_img_url))) {
                    /* Descargar con timeout corto y convertir a ARGB nativo */
                    if (eonsurf_fetch_and_decode_infobox_image(detected_img_url)) {
                        /* Sincronizar textura con el layout */
                        g_layout.infobox_rgba_tex = g_infobox_tex;
                        g_layout.img_w = g_infobox_w;
                        g_layout.img_h = g_infobox_h;
                    }
                }
            } else {
                snprintf(page_body, sizeof(page_body), "Página recibida (Tamaño: %u bytes).", (unsigned int)chunk.size);
            }'''
if old_img_fetch in text:
    text = text.replace(old_img_fetch, new_img_fetch, 1)

# 3. Soporte de clic en enlaces directo con el cursor
old_click_handling = '''            /* Clic con Cruz [X] */
            if (pdata.BTN_CROSS && !prev_btn_cross) {'''

new_click_handling = '''            /* Clic con Cruz [X] */
            if (pdata.BTN_CROSS && !prev_btn_cross) {
                /* 1. Verificar si hizo clic sobre un enlace en el texto */
                const char *clicked_link = eonsurf_check_link_click(px, py);
                if (clicked_link) {
                    navigate_to(clicked_link);
                }'''
if old_click_handling in text:
    text = text.replace(old_click_handling, new_click_handling, 1)

# 4. Blitter con escalado eonsurf_blit_infobox_image en el recuadro negro
old_tex_render = '''        /* Si tenemos textura en g_layout.infobox_rgba_tex, dibujarla con escalado 2x centrado */
        if (g_layout.infobox_rgba_tex && g_layout.img_w > 0 && g_layout.img_h > 0) {
            int tex_x = col_right_x + (col_right_w - (int)g_layout.img_w * 2) / 2;
            int tex_y = img_box_y + (img_box_h - (int)g_layout.img_h * 1) / 2 - 5;
            if (tex_x < col_right_x + 16) tex_x = col_right_x + 16;

            for (int ty = 0; ty < (int)g_layout.img_h; ty++) {
                int screen_y = tex_y + ty;
                if (screen_y < 0 || screen_y >= (int)H) continue;
                for (int tx = 0; tx < (int)g_layout.img_w; tx++) {
                    uint32_t px_col = g_layout.infobox_rgba_tex[ty * g_layout.img_w + tx];
                    int screen_x = tex_x + tx * 2;
                    if (screen_x >= 0 && screen_x + 1 < (int)W) {
                        fb[cur][screen_y * W + screen_x] = px_col;
                        fb[cur][screen_y * W + screen_x + 1] = px_col;
                    }
                }
            }
        } else {'''

new_tex_render = '''        /* Si tenemos textura en g_layout.infobox_rgba_tex, dibujarla escalada con el blitter de punto fijo */
        if (g_layout.infobox_rgba_tex && g_layout.img_w > 0 && g_layout.img_h > 0) {
            int box_x = col_right_x + 18;
            int box_y = img_box_y + 4;
            int box_w = col_right_w - 36;
            int box_h = img_box_h - 8;
            eonsurf_blit_infobox_image(fb[cur], W, g_layout.infobox_rgba_tex, g_layout.img_w, g_layout.img_h,
                                       box_x, box_y, box_w, box_h);
        } else {'''
if old_tex_render in text:
    text = text.replace(old_tex_render, new_tex_render, 1)

with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
    f.write(text)

print("Applied interactive media & link clicking integration!")
