/* ============================================================================
 * MOTOR DE RENDERIZADO VISUAL ENRIQUECIDO POR BLOQUES WEB (Estilo NetSurf + Dark Glass)
 * ============================================================================ */
static inline void eonsurf_render_rich_web_blocks(uint32_t *fb, int pitch,
                                                   const char *article_body,
                                                   const char *article_title,
                                                   int content_max_x,
                                                   int cursor_x, int cursor_y,
                                                   int scroll_y, int *out_max_scroll) {
    /* 1. Parsear el contenido estructurado en bloques */
    eonsurf_parse_content_into_blocks(article_body, article_title);

    int cur_y = 150 - scroll_y;
    int base_start_y = cur_y;
    int link_idx = 0;

    for (int li = 0; li < g_link_count; li++) {
        g_links[li].x = 0; g_links[li].y = 0; g_links[li].w = 0; g_links[li].h = 0;
        g_links[li].visible = 0;
    }

    int card_w = content_max_x - 88;
    if (card_w < 300) card_w = 300;

    for (int bi = 0; bi < g_web_block_count; bi++) {
        EonWebBlock *b = &g_web_blocks[bi];
        int block_h = 48;

        switch (b->type) {
            case BLOCK_HERO_HEADER: {
                block_h = 58;
                if (cur_y + block_h >= 120 && cur_y <= 650) {
                    /* Tarjeta Hero Header con degradado neón */
                    eonsurf_draw_web_card_box(fb, pitch, 88, cur_y, card_w, block_h, 0x182436, 0x00E5FF, 2, 0, 8);
                    /* Barra de acento cian a la izquierda */
                    for (int by = cur_y + 4; by < cur_y + block_h - 4; by++) {
                        for (int bx = 90; bx < 96; bx++) mk_blend(fb, pitch, bx, by, 0x00E5FF, 255);
                    }
                    /* Texto H1 grande */
                    eonsurf_draw_mockup_text(fb, pitch, 108, cur_y + 14, 24, b->text, 0xFFFFFF, 1, 88 + card_w - 20);
                }
                cur_y += block_h + 14;
                break;
            }
            case BLOCK_SECTION_H2: {
                block_h = 44;
                if (cur_y + block_h >= 120 && cur_y <= 650) {
                    eonsurf_draw_web_card_box(fb, pitch, 88, cur_y, card_w, block_h, 0x141E2B, 0x38BDF8, 1, 0, 6);
                    /* Badge H2 */
                    for (int by = cur_y + 10; by < cur_y + 34; by++) {
                        for (int bx = 98; bx < 104; bx++) mk_blend(fb, pitch, bx, by, 0x38BDF8, 255);
                    }
                    eonsurf_draw_mockup_text(fb, pitch, 114, cur_y + 12, 19, b->text, 0xF0F6FC, 1, 88 + card_w - 20);
                }
                cur_y += block_h + 10;
                break;
            }
            case BLOCK_SUBSECTION_H3: {
                block_h = 36;
                if (cur_y + block_h >= 120 && cur_y <= 650) {
                    eonsurf_draw_mockup_text(fb, pitch, 96, cur_y + 8, 17, b->text, 0x38BDF8, 1, 88 + card_w - 20);
                    /* Línea sutil de subrayado */
                    for (int lx = 96; lx < 88 + card_w - 20; lx++) mk_blend(fb, pitch, lx, cur_y + 32, 0x1E293B, 180);
                }
                cur_y += block_h + 8;
                break;
            }
            case BLOCK_CODE_BOX: {
                block_h = 52;
                if (cur_y + block_h >= 120 && cur_y <= 650) {
                    eonsurf_draw_web_card_box(fb, pitch, 88, cur_y, card_w, block_h, 0x0A0F18, 0x22C55E, 1, 0, 6);
                    /* Insignia CODE */
                    for (int by = cur_y + 6; by < cur_y + 24; by++) {
                        for (int bx = 98; bx < 150; bx++) mk_blend(fb, pitch, bx, by, 0x14532D, 220);
                    }
                    eonsurf_draw_mockup_text(fb, pitch, 104, cur_y + 8, 12, "CODE", 0x4ADE80, 0, 150);
                    eonsurf_draw_mockup_text(fb, pitch, 160, cur_y + 16, 15, b->text, 0x86EFAC, 0, 88 + card_w - 20);
                }
                cur_y += block_h + 10;
                break;
            }
            case BLOCK_QUOTE_CALLOUT: {
                block_h = 46;
                if (cur_y + block_h >= 120 && cur_y <= 650) {
                    eonsurf_draw_web_card_box(fb, pitch, 88, cur_y, card_w, block_h, 0x131924, 0xFACC15, 1, 0, 6);
                    /* Barra dorada */
                    for (int by = cur_y + 4; by < cur_y + block_h - 4; by++) {
                        for (int bx = 90; bx < 96; bx++) mk_blend(fb, pitch, bx, by, 0xFACC15, 255);
                    }
                    eonsurf_draw_mockup_text(fb, pitch, 108, cur_y + 14, 15, b->text, 0xFEF08A, 0, 88 + card_w - 20);
                }
                cur_y += block_h + 10;
                break;
            }
            case BLOCK_NAV_BUTTONS: {
                block_h = 42;
                int is_hover = (cursor_x >= 88 && cursor_x <= 88 + card_w && cursor_y >= cur_y && cursor_y <= cur_y + block_h);
                if (cur_y + block_h >= 120 && cur_y <= 650) {
                    uint32_t btn_bg = is_hover ? 0x1E3A5F : 0x132235;
                    uint32_t btn_border = is_hover ? 0x00FFFF : 0x38BDF8;
                    eonsurf_draw_web_card_box(fb, pitch, 88, cur_y, card_w, block_h, btn_bg, btn_border, is_hover ? 2 : 1, is_hover, 6);
                    
                    /* Flechita / Pastilla interactiva */
                    for (int dy = -4; dy <= 4; dy++) {
                        for (int dx = 0; dx <= 5; dx++) {
                            if (dx <= 4 - abs(dy)) {
                                mk_blend(fb, pitch, 102 + dx, cur_y + 21 + dy, 0x38BDF8, 255);
                            }
                        }
                    }
                    eonsurf_draw_mockup_text(fb, pitch, 118, cur_y + 12, 16, b->text, is_hover ? 0xFFFFFF : 0x7DD3FC, 1, 88 + card_w - 20);
                    
                    /* Registrar colisión de enlace */
                    if (link_idx < g_link_count) {
                        g_links[link_idx].x = 88;
                        g_links[link_idx].y = cur_y;
                        g_links[link_idx].w = card_w;
                        g_links[link_idx].h = block_h;
                        g_links[link_idx].visible = 1;
                        link_idx++;
                    }
                }
                cur_y += block_h + 8;
                break;
            }
            case BLOCK_HR_DIVIDER: {
                block_h = 16;
                if (cur_y + block_h >= 120 && cur_y <= 650) {
                    for (int lx = 88; lx < 88 + card_w; lx++) {
                        float t = (float)(lx - 88) / (float)card_w;
                        float edge_fade = sinf(t * 3.14159f);
                        int alpha = (int)(edge_fade * 160.0f);
                        mk_blend(fb, pitch, lx, cur_y + 8, 0x38BDF8, alpha);
                    }
                }
                cur_y += block_h;
                break;
            }
            default: {
                /* BLOCK_CARD_ARTICLE: Párrafo en tarjeta elegante con auto-wrapping */
                int text_len = strlen(b->text);
                int est_lines = (text_len * 9) / (card_w - 40) + 1;
                block_h = 24 + (est_lines * 22);
                if (block_h < 44) block_h = 44;

                int is_hover = (cursor_x >= 88 && cursor_x <= 88 + card_w && cursor_y >= cur_y && cursor_y <= cur_y + block_h);

                if (cur_y + block_h >= 120 && cur_y <= 650) {
                    uint32_t card_bg = is_hover ? 0x141E2C : 0x0E1622;
                    uint32_t card_border = is_hover ? 0x00E5FF : 0x1E293B;
                    eonsurf_draw_web_card_box(fb, pitch, 88, cur_y, card_w, block_h, card_bg, card_border, 1, is_hover, 6);

                    /* Dibujar texto envuelto en líneas */
                    int line_y = cur_y + 12;
                    const char *tp = b->text;
                    char word[128];
                    int pen_x = 104;
                    int max_px = 88 + card_w - 20;

                    while (*tp) {
                        if (*tp == '\x01' || *tp == '\x02') { tp++; continue; }
                        if (*tp == ' ') { pen_x += 7; tp++; continue; }

                        int wi = 0;
                        while (*tp && *tp != ' ' && *tp != '\n' && *tp != '\x01' && *tp != '\x02' && wi < 120) {
                            word[wi++] = *tp++;
                        }
                        word[wi] = '\0';
                        int word_w = wi * 9;

                        if (pen_x + word_w > max_px) {
                            pen_x = 104;
                            line_y += 22;
                        }

                        if (line_y >= 120 && line_y <= 645) {
                            pen_x = eonsurf_draw_mockup_text(fb, pitch, pen_x, line_y, 16, word, is_hover ? 0xFFFFFF : 0xE2E8F0, 0, max_px);
                        } else {
                            pen_x += word_w;
                        }
                    }
                }
                cur_y += block_h + 12;
                break;
            }
        }
    }

    /* Calcular scroll dinámico */
    int total_doc_h = cur_y - base_start_y;
    int max_s = total_doc_h - 400;
    if (max_s < 0) max_s = 0;
    if (out_max_scroll) *out_max_scroll = max_s;
}
