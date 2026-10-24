with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    text = f.read()

# Buscamos la sección de dibujo del lienzo
old_canvas = '''        /* 4. LIENZO WEB MODERNO ("Reader Canvas" Blanco/Gris Editorial #FFFFFF) */
        int canvas_x = 50;
        int canvas_y = 120;
        int canvas_w = W - 100;
        int canvas_h = (page_links_count > 0) ? (H - 260) : (H - 175);

        /* Sombra exterior y cuerpo de la pagina web */
        rect_rounded(canvas_x - 3, canvas_y - 3, canvas_w + 6, canvas_h + 6, 8, 0x000f131a);
        rect_rounded(canvas_x, canvas_y, canvas_w, canvas_h, 6, 0x00f8fafc);

        /* Cabecera del articulo web */
        rect(canvas_x, canvas_y, canvas_w, 42, 0x00f1f5f9);
        rect(canvas_x, canvas_y + 42, canvas_w, 1, 0x00e2e8f0);
        text_hd(canvas_x + 20, canvas_y + 13, 1, page_title, 0x000f172a);

        /* Contenido web con tipografia HD 8x16 de NetSurf Zap (Color texto editorial: #1E293B) */
        int content_height = text_wrapped_hd(canvas_x + 24, canvas_y + 55, 1, canvas_w - 50, 22,
                                             canvas_y + 45, canvas_y + canvas_h - 15, scroll_y, page_body, 0x001e293b);
        
        /* Calcular limites de scroll */
        max_scroll_y = (content_height > canvas_h - 60) ? (content_height - (canvas_h - 60)) : 0;

        /* Barra de scroll vertical interactiva */
        if (max_scroll_y > 0) {
            int scrollbar_track_h = canvas_h - 60;
            int thumb_h = (scrollbar_track_h * (canvas_h - 60)) / content_height;
            if (thumb_h < 25) thumb_h = 25;
            int thumb_y = canvas_y + 50 + (scroll_y * (scrollbar_track_h - thumb_h)) / max_scroll_y;

            rect(canvas_x + canvas_w - 12, canvas_y + 50, 8, scrollbar_track_h, 0x00e2e8f0);
            rect_rounded(canvas_x + canvas_w - 12, thumb_y, 8, thumb_h, 4, 0x003b82f6);
        }'''

new_canvas = '''        /* 4. MOTOR DE MAQUETACIÓN ESTRUCTURADA 70/30 (Estilo EonSurf UI Mockup) */
        int canvas_x = 40;
        int canvas_y = 120;
        int canvas_w = W - 80;
        int canvas_h = (page_links_count > 0) ? (H - 260) : (H - 175);

        /* Columna Izquierda: 68% del ancho para el Artículo y Texto */
        int col_left_w = (canvas_w * 68) / 100;
        /* Columna Derecha: 30% del ancho para la Tarjeta Infobox */
        int col_right_x = canvas_x + col_left_w + 20;
        int col_right_w = canvas_w - col_left_w - 20;

        /* Sombra y cuerpo del artículo izquierdo */
        rect_rounded(canvas_x - 3, canvas_y - 3, col_left_w + 6, canvas_h + 6, 8, 0x000f131a);
        rect_rounded(canvas_x, canvas_y, col_left_w, canvas_h, 6, 0x00f8fafc);

        /* Cabecera del artículo web */
        rect(canvas_x, canvas_y, col_left_w, 42, 0x00f1f5f9);
        rect(canvas_x, canvas_y + 42, col_left_w, 1, 0x00e2e8f0);
        text_hd(canvas_x + 20, canvas_y + 13, 1, page_title, 0x000f172a);

        /* Contenido del artículo con tipografía Zap 8x16 en la columna izquierda */
        int content_height = text_wrapped_hd(canvas_x + 24, canvas_y + 55, 1, col_left_w - 48, 22,
                                             canvas_y + 45, canvas_y + canvas_h - 15, scroll_y, page_body, 0x001e293b);
        max_scroll_y = (content_height > canvas_h - 60) ? (content_height - (canvas_h - 60)) : 0;

        /* Barra de scroll vertical de la columna izquierda */
        if (max_scroll_y > 0) {
            int scrollbar_track_h = canvas_h - 60;
            int thumb_h = (scrollbar_track_h * (canvas_h - 60)) / content_height;
            if (thumb_h < 25) thumb_h = 25;
            int thumb_y = canvas_y + 50 + (scroll_y * (scrollbar_track_h - thumb_h)) / max_scroll_y;

            rect(canvas_x + col_left_w - 10, canvas_y + 50, 6, scrollbar_track_h, 0x00e2e8f0);
            rect_rounded(canvas_x + col_left_w - 10, thumb_y, 6, thumb_h, 3, 0x003b82f6);
        }

        /* --- COLUMNA DERECHA: TARJETA INFOBOX WIKIPEDIA (30%) --- */
        rect_rounded(col_right_x - 3, canvas_y - 3, col_right_w + 6, canvas_h + 6, 8, 0x000f131a);
        rect_rounded(col_right_x, canvas_y, col_right_w, canvas_h, 6, 0x00ffffff);

        /* Cabecera de la Infobox */
        rect(col_right_x, canvas_y, col_right_w, 38, 0x000ea5e9);
        text_hd(col_right_x + 16, canvas_y + 11, 1, g_layout.title, 0x00ffffff);

        /* Área de imagen / preview del artículo */
        int img_box_y = canvas_y + 48;
        int img_box_h = 130;
        rect_rounded(col_right_x + 14, img_box_y, col_right_w - 28, img_box_h, 4, 0x000f172a);
        rect(col_right_x + 20, img_box_y + 6, col_right_w - 40, img_box_h - 12, 0x001e293b);
        text_hd(col_right_x + (col_right_w / 2) - 64, img_box_y + 48, 1, "[ FOTO CONSOLA ]", 0x0038bdf8);
        text_hd(col_right_x + 22, img_box_y + img_box_h + 8, 1, g_layout.subtitle, 0x0064748b);

        /* Tabla de especificaciones clave-valor */
        int table_start_y = img_box_y + img_box_h + 32;
        rect(col_right_x + 14, table_start_y - 4, col_right_w - 28, 1, 0x00cbd5e1);

        for (uint32_t r = 0; r < g_layout.info_row_count; r++) {
            int row_y = table_start_y + (r * 26);
            if (row_y + 24 > canvas_y + canvas_h) break;

            if (r % 2 == 1) {
                rect(col_right_x + 14, row_y - 2, col_right_w - 28, 24, 0x00f8fafc);
            }
            text_hd(col_right_x + 18, row_y + 3, 1, g_layout.info_rows[r].key, 0x000f172a);
            text_hd(col_right_x + 120, row_y + 3, 1, g_layout.info_rows[r].value, 0x002563eb);
        }'''

if old_canvas in text:
    text = text.replace(old_canvas, new_canvas, 1)
    with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
        f.write(text)
    print("Canvas replaced with 70/30 layout")
else:
    print("Old canvas not found")
