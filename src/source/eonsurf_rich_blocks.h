/* ============================================================================
 * MOTOR DE RENDERIZADO VISUAL ENRIQUECIDO ESTILO NAVEGADOR WEB REAL (Chrome / NetSurf)
 * Lienzo continuo blanco #FFFFFF, tipografía oscura #202124, enlaces en azul #0B0080
 * ============================================================================ */
static inline void eonsurf_render_rich_web_blocks(uint32_t *fb, int pitch,
                                                   const char *article_body,
                                                   const char *article_title,
                                                   int content_max_x,
                                                   int cursor_x, int cursor_y,
                                                   int scroll_y, int *out_max_scroll) {
    /* 1. Parsear el contenido estructurado en bloques */
    eonsurf_parse_content_into_blocks(article_body, article_title);

    int page_start_x = 52;
    int cur_y = 126 - scroll_y;
    int base_start_y = cur_y;
    int link_idx = 0;

    for (int li = 0; li < g_link_count; li++) {
        g_links[li].x = 0; g_links[li].y = 0; g_links[li].w = 0; g_links[li].h = 0;
        g_links[li].visible = 0;
    }

    int web_col_w = content_max_x - page_start_x - 16;
    if (web_col_w < 300) web_col_w = 300;

    for (int bi = 0; bi < g_web_block_count; bi++) {
        EonWebBlock *b = &g_web_blocks[bi];
        int block_h = 24;

        switch (b->type) {
            case BLOCK_HERO_HEADER: {
                /* H1 Web Real: Gran encabezado editorial estilo Wikipedia / W3C */
                block_h = 42;
                if (cur_y + block_h >= 115 && cur_y <= 660) {
                    /* Título H1 en negro azabache tipográfico #1A1A1A (sin contorno) */
                    eonsurf_draw_mockup_text(fb, pitch, page_start_x + 10, cur_y + 4, 26, b->text, 0x1A1A1A, 0, page_start_x + web_col_w);
                    /* Línea sutil gris divisoria Wikipedia (#A2A9B1) debajo del H1 */
                    int div_y = cur_y + 36;
                    if (div_y >= 115 && div_y <= 660) {
                        for (int lx = page_start_x + 8; lx < page_start_x + web_col_w; lx++) {
                            fb[div_y * pitch + lx] = 0xFFA2A9B1;
                        }
                    }
                }
                cur_y += block_h + 12;
                break;
            }
            case BLOCK_SECTION_H2: {
                /* H2 Web Real: Encabezado de sección con subrayado sutil */
                block_h = 32;
                if (cur_y + block_h >= 115 && cur_y <= 660) {
                    eonsurf_draw_mockup_text(fb, pitch, page_start_x + 10, cur_y + 4, 20, b->text, 0x050505, 0, page_start_x + web_col_w);
                    int div_y = cur_y + 28;
                    if (div_y >= 115 && div_y <= 660) {
                        for (int lx = page_start_x + 10; lx < page_start_x + web_col_w - 20; lx++) {
                            fb[div_y * pitch + lx] = 0xFFC8CCD1;
                        }
                    }
                }
                cur_y += block_h + 10;
                break;
            }
            case BLOCK_SUBSECTION_H3: {
                /* H3 Web Real: Subtítulo 17px */
                block_h = 26;
                if (cur_y + block_h >= 115 && cur_y <= 660) {
                    eonsurf_draw_mockup_text(fb, pitch, page_start_x + 12, cur_y + 2, 17, b->text, 0x202124, 0, page_start_x + web_col_w);
                }
                cur_y += block_h + 8;
                break;
            }
            case BLOCK_CODE_BOX: {
                /* Bloque de código con fondo grisáceo suave #F1F3F5 y borde sutil #D0D7DE */
                block_h = 44;
                if (cur_y + block_h >= 115 && cur_y <= 660) {
                    /* Caja de fondo gris código */
                    for (int cy = cur_y; cy < cur_y + block_h; cy++) {
                        if (cy < 115 || cy > 660) continue;
                        for (int cx = page_start_x + 10; cx < page_start_x + web_col_w - 10; cx++) {
                            fb[cy * pitch + cx] = 0xFFF6F8FA;
                        }
                    }
                    /* Borde izquierdo azul/verde sutil */
                    for (int cy = cur_y; cy < cur_y + block_h; cy++) {
                        if (cy < 115 || cy > 660) continue;
                        for (int cx = page_start_x + 10; cx < page_start_x + 14; cx++) {
                            fb[cy * pitch + cx] = 0xFF0969DA;
                        }
                    }
                    eonsurf_draw_mockup_text(fb, pitch, page_start_x + 24, cur_y + 12, 15, b->text, 0x1F2328, 0, page_start_x + web_col_w - 20);
                }
                cur_y += block_h + 10;
                break;
            }
            case BLOCK_QUOTE_CALLOUT: {
                /* Cita web: barra izquierda gris oscura #A2A9B1 y texto suave */
                block_h = 36;
                if (cur_y + block_h >= 115 && cur_y <= 660) {
                    for (int cy = cur_y; cy < cur_y + block_h; cy++) {
                        if (cy < 115 || cy > 660) continue;
                        for (int cx = page_start_x + 12; cx < page_start_x + 16; cx++) {
                            fb[cy * pitch + cx] = 0xFF54595D;
                        }
                    }
                    eonsurf_draw_mockup_text(fb, pitch, page_start_x + 26, cur_y + 8, 16, b->text, 0x404244, 0, page_start_x + web_col_w - 20);
                }
                cur_y += block_h + 8;
                break;
            }
            case BLOCK_NAV_BUTTONS: {
                /* Botón / Enlace Web de navegación: estilo pill o link azul interactivo */
                block_h = 28;
                int is_hover = (cursor_x >= page_start_x + 12 && cursor_x <= page_start_x + web_col_w - 20 &&
                                cursor_y >= cur_y && cursor_y <= cur_y + block_h);
                if (cur_y + block_h >= 115 && cur_y <= 660) {
                    if (is_hover) {
                        for (int by = cur_y; by < cur_y + block_h; by++) {
                            if (by < 115 || by > 660) continue;
                            for (int bx = page_start_x + 12; bx < page_start_x + web_col_w - 20; bx++) {
                                fb[by * pitch + bx] = 0xFFEBF5FF;
                            }
                        }
                    }
                    /* Punto viñeta */
                    int dot_x = page_start_x + 18;
                    int dot_y = cur_y + 13;
                    for (int dy = -2; dy <= 2; dy++) {
                        for (int dx = -2; dx <= 2; dx++) {
                            if (dx * dx + dy * dy <= 4) {
                                fb[(dot_y + dy) * pitch + (dot_x + dx)] = 0xFF3366CC;
                            }
                        }
                    }
                    eonsurf_draw_mockup_text(fb, pitch, page_start_x + 28, cur_y + 5, 16, b->text, is_hover ? 0x0B0080 : 0x3366CC, 0, page_start_x + web_col_w - 20);

                    /* Registrar colisión de enlace */
                    if (link_idx < g_link_count) {
                        g_links[link_idx].x = page_start_x + 12;
                        g_links[link_idx].y = cur_y;
                        g_links[link_idx].w = web_col_w - 32;
                        g_links[link_idx].h = block_h;
                        g_links[link_idx].visible = 1;
                        link_idx++;
                    }
                }
                cur_y += block_h + 6;
                break;
            }
            case BLOCK_HR_DIVIDER: {
                block_h = 14;
                if (cur_y + block_h >= 115 && cur_y <= 660) {
                    for (int lx = page_start_x + 10; lx < page_start_x + web_col_w; lx++) {
                        fb[(cur_y + 6) * pitch + lx] = 0xFFD8DDE3;
                    }
                }
                cur_y += block_h;
                break;
            }
            default: {
                /* BLOCK_CARD_ARTICLE: Párrafo web real fluido sin tarjeta oscura, texto tipográfico negro #202124 y enlaces en azul #3366CC / #0B0080 */
                int text_len = strlen(b->text);
                int est_lines = (text_len * 9) / (web_col_w - 24) + 1;
                block_h = 4 + (est_lines * 22);

                if (cur_y + block_h >= 115 && cur_y <= 660) {
                    int line_y = cur_y;
                    const char *tp = b->text;
                    char word[128];
                    int pen_x = page_start_x + 12;
                    int max_px = page_start_x + web_col_w - 10;
                    int in_link = 0;
                    int link_start_x = 0;

                    while (*tp) {
                        if (*tp == '\x01') {
                            in_link = 1;
                            link_start_x = pen_x;
                            tp++;
                            continue;
                        }
                        if (*tp == '\x02') {
                            in_link = 0;
                            if (link_idx < g_link_count && pen_x > link_start_x && line_y >= 115 && line_y <= 655) {
                                g_links[link_idx].x = link_start_x;
                                g_links[link_idx].y = line_y - 2;
                                g_links[link_idx].w = pen_x - link_start_x;
                                g_links[link_idx].h = 20;
                                g_links[link_idx].visible = 1;
                                link_idx++;
                            }
                            tp++;
                            continue;
                        }
                        if (*tp == ' ') { pen_x += 7; tp++; continue; }

                        int wi = 0;
                        while (*tp && *tp != ' ' && *tp != '\n' && *tp != '\x01' && *tp != '\x02' && wi < 120) {
                            word[wi++] = *tp++;
                        }
                        word[wi] = '\0';
                        int word_w = wi * 9;

                        if (pen_x + word_w > max_px) {
                            if (in_link && link_idx < g_link_count && pen_x > link_start_x && line_y >= 115 && line_y <= 655) {
                                g_links[link_idx].x = link_start_x;
                                g_links[link_idx].y = line_y - 2;
                                g_links[link_idx].w = pen_x - link_start_x;
                                g_links[link_idx].h = 20;
                                g_links[link_idx].visible = 1;
                                link_idx++;
                            }
                            pen_x = page_start_x + 12;
                            link_start_x = pen_x;
                            line_y += 22;
                        }

                        if (line_y >= 115 && line_y <= 655) {
                            uint32_t text_col = 0x202124;
                            if (in_link) {
                                int is_hover = (cursor_x >= pen_x && cursor_x <= pen_x + word_w &&
                                                cursor_y >= line_y - 2 && cursor_y <= line_y + 18);
                                text_col = is_hover ? 0x0B0080 : 0x3366CC;
                            }
                            pen_x = eonsurf_draw_mockup_text(fb, pitch, pen_x, line_y, 16, word, text_col, 0, max_px);
                        } else {
                            pen_x += word_w;
                        }
                    }
                }
                cur_y += block_h + 10;
                break;
            }       }
    }

    /* Calcular scroll dinámico */
    int total_doc_h = cur_y - base_start_y;
    int max_s = total_doc_h - 440;
    if (max_s < 0) max_s = 0;
    if (out_max_scroll) *out_max_scroll = max_s;
}
