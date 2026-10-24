with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    text = f.read()

old_res = '''            char clean_body[7800] = "";
            eonsurf_fsm_extract_text_and_links(chunk.data, chunk.size, clean_body, sizeof(clean_body), page_links, &page_links_count);

            if (strlen(clean_body) > 0) {
                snprintf(page_body, sizeof(page_body), "%s", clean_body);
            } else {
                snprintf(page_body, sizeof(page_body), "Pagina recibida (Tamano: %u bytes).", (unsigned int)chunk.size);
            }'''

new_res = '''            char clean_body[7800] = "";
            eonsurf_fsm_extract_text_and_links(chunk.data, chunk.size, clean_body, sizeof(clean_body), page_links, &page_links_count);

            if (strlen(clean_body) > 0) {
                snprintf(page_body, sizeof(page_body), "%s", clean_body);
            } else {
                snprintf(page_body, sizeof(page_body), "Página recibida (Tamaño: %u bytes).", (unsigned int)chunk.size);
            }

            /* Actualizar título del documento en la Infobox 70/30 */
            if (strstr(target_url, "PlayStation_3") || strstr(target_url, "playstation")) {
                strncpy(g_layout.title, "PlayStation 3", sizeof(g_layout.title) - 1);
                strncpy(g_layout.subtitle, "Consola de videojuegos de Sony", sizeof(g_layout.subtitle) - 1);
            } else if (strstr(target_url, "duckduckgo")) {
                strncpy(g_layout.title, "DuckDuckGo Search", sizeof(g_layout.title) - 1);
                strncpy(g_layout.subtitle, "Resultados web en vivo", sizeof(g_layout.subtitle) - 1);
            } else {
                strncpy(g_layout.title, "Web Document", sizeof(g_layout.title) - 1);
                strncpy(g_layout.subtitle, target_url, sizeof(g_layout.subtitle) - 1);
            }'''

if old_res in text:
    text = text.replace(old_res, new_res, 1)
    with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
        f.write(text)
    print("Updated navigate_to with dynamic Infobox title sync")
else:
    print("old_res not found")
