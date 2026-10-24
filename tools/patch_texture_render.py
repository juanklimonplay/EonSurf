with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    c = f.read()

# Buscamos init_default_layout
old_init = '''/* Inicializa la infobox de prueba para Wikipedia PS3 */
static void init_default_layout(void) {
    memset(&g_layout, 0, sizeof(EonPageLayout));
    strncpy(g_layout.title, "PlayStation 3", sizeof(g_layout.title) - 1);
    strncpy(g_layout.subtitle, "Consola de videojuegos de Sony", sizeof(g_layout.subtitle) - 1);
    
    g_layout.info_row_count = 5;
    strncpy(g_layout.info_rows[0].key, "Fabricante", sizeof(g_layout.info_rows[0].key) - 1);
    strncpy(g_layout.info_rows[0].value, "Sony Computer Ent.", sizeof(g_layout.info_rows[0].value) - 1);

    strncpy(g_layout.info_rows[1].key, "Lanzamiento", sizeof(g_layout.info_rows[1].key) - 1);
    strncpy(g_layout.info_rows[1].value, "11 de nov. de 2006", sizeof(g_layout.info_rows[1].value) - 1);

    strncpy(g_layout.info_rows[2].key, "CPU", sizeof(g_layout.info_rows[2].key) - 1);
    strncpy(g_layout.info_rows[2].value, "Cell Broadband Engine 3.2GHz", sizeof(g_layout.info_rows[2].value) - 1);

    strncpy(g_layout.info_rows[3].key, "GPU", sizeof(g_layout.info_rows[3].key) - 1);
    strncpy(g_layout.info_rows[3].value, "RSX 'Reality Synthesizer' 500MHz", sizeof(g_layout.info_rows[3].value) - 1);

    strncpy(g_layout.info_rows[4].key, "Ventas", sizeof(g_layout.info_rows[4].key) - 1);
    strncpy(g_layout.info_rows[4].value, "87.4 millones de unidades", sizeof(g_layout.info_rows[4].value) - 1);
}'''

new_init = '''/* Inicializa la infobox y genera la textura del thumbnail de la PS3 */
static void init_default_layout(void) {
    memset(&g_layout, 0, sizeof(EonPageLayout));
    strncpy(g_layout.title, "PlayStation 3", sizeof(g_layout.title) - 1);
    strncpy(g_layout.subtitle, "Consola de videojuegos de Sony", sizeof(g_layout.subtitle) - 1);
    
    g_layout.info_row_count = 5;
    strncpy(g_layout.info_rows[0].key, "Fabricante", sizeof(g_layout.info_rows[0].key) - 1);
    strncpy(g_layout.info_rows[0].value, "Sony Computer Ent.", sizeof(g_layout.info_rows[0].value) - 1);

    strncpy(g_layout.info_rows[1].key, "Lanzamiento", sizeof(g_layout.info_rows[1].key) - 1);
    strncpy(g_layout.info_rows[1].value, "11 de nov. de 2006", sizeof(g_layout.info_rows[1].value) - 1);

    strncpy(g_layout.info_rows[2].key, "CPU", sizeof(g_layout.info_rows[2].key) - 1);
    strncpy(g_layout.info_rows[2].value, "Cell Broadband Engine 3.2GHz", sizeof(g_layout.info_rows[2].value) - 1);

    strncpy(g_layout.info_rows[3].key, "GPU", sizeof(g_layout.info_rows[3].key) - 1);
    strncpy(g_layout.info_rows[3].value, "RSX 'Reality Synthesizer' 500MHz", sizeof(g_layout.info_rows[3].value) - 1);

    strncpy(g_layout.info_rows[4].key, "Ventas", sizeof(g_layout.info_rows[4].key) - 1);
    strncpy(g_layout.info_rows[4].value, "87.4 millones de unidades", sizeof(g_layout.info_rows[4].value) - 1);

    /* Generar textura de 120x80 para la miniatura fotográfica de la consola */
    g_layout.img_w = 120;
    g_layout.img_h = 80;
    g_layout.infobox_rgba_tex = (uint32_t *)malloc(120 * 80 * sizeof(uint32_t));
    if (g_layout.infobox_rgba_tex) {
        for (int y = 0; y < 80; y++) {
            for (int x = 0; x < 120; x++) {
                uint32_t color;
                /* Silueta de la PS3 Fat con acabado piano black y reflejo cian */
                if (y >= 25 && y <= 55 && x >= 15 && x <= 105) {
                    if (y == 25 || y == 55 || x == 15 || x == 105) {
                        color = 0xFF38BDF8; /* Contorno cromado de la bahía */
                    } else if (y == 40 && x >= 30 && x <= 90) {
                        color = 0xFFE2E8F0; /* Ranura Slot-in de Blu-Ray */
                    } else {
                        color = 0xFF0F172A; /* Piano black brillante */
                    }
                } else {
                    /* Gradiente de fondo del estudio fotográfico */
                    uint8_t grad = (uint8_t)(15 + (y * 20) / 80);
                    color = 0xFF000000 | (grad << 16) | ((grad + 5) << 8) | (grad + 15);
                }
                g_layout.infobox_rgba_tex[y * 120 + x] = color;
            }
        }
    }
}'''

# Buscamos la sección de renderizado de la imagen en la columna derecha
old_img_draw = '''        /* Área de imagen / preview del artículo */
        int img_box_y = canvas_y + 48;
        int img_box_h = 130;
        rect_rounded(col_right_x + 14, img_box_y, col_right_w - 28, img_box_h, 4, 0x000f172a);
        rect(col_right_x + 20, img_box_y + 6, col_right_w - 40, img_box_h - 12, 0x001e293b);
        text_hd(col_right_x + (col_right_w / 2) - 64, img_box_y + 48, 1, "[ FOTO CONSOLA ]", 0x0038bdf8);
        text_hd(col_right_x + 22, img_box_y + img_box_h + 8, 1, g_layout.subtitle, 0x0064748b);'''

new_img_draw = '''        /* Área de imagen fotográfica de la Infobox con textura nativa */
        int img_box_y = canvas_y + 48;
        int img_box_h = 130;
        rect_rounded(col_right_x + 14, img_box_y, col_right_w - 28, img_box_h, 6, 0x000b0f17);

        /* Si tenemos textura en g_layout.infobox_rgba_tex, dibujarla con escalado 2x centrado */
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
        } else {
            rect(col_right_x + 20, img_box_y + 6, col_right_w - 40, img_box_h - 12, 0x001e293b);
            text_hd(col_right_x + (col_right_w / 2) - 64, img_box_y + 48, 1, "[ FOTO CONSOLA ]", 0x0038bdf8);
        }
        text_hd(col_right_x + 22, img_box_y + img_box_h + 8, 1, g_layout.subtitle, 0x0064748b);'''

if old_init in c and old_img_draw in c:
    c = c.replace(old_init, new_init, 1)
    c = c.replace(old_img_draw, new_img_draw, 1)
    with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
        f.write(c)
    print('Infobox photo thumbnail texture integrated successfully!')
else:
    print('Could not find search blocks in main.c')
