# Patch main.c menu clicks, L1/R1 tab switching and D-Pad spatial nav
import os

repo_root = r"J:\EonPS3\EonSurf\repo"
src_dir = os.path.join(repo_root, "src", "source")
main_c_path = os.path.join(src_dir, "main.c")

with open(main_c_path, "r", encoding="utf-8") as f:
    text = f.read()

# 1. Update clicks inside the Settings Control Center
old_menu_clicks = """                /* Clic dentro de las opciones del Centro de Control EonSurf si est abierto */
                else if (g_settings_menu_open && px >= 820 && px <= 1250 && py >= 105 && py <= 500) {
                    if (px >= 835 && px <= 1235 && py >= 175 && py <= 209) {
                        /* Opcin 1: Alternar barra lateral */
                        g_sidebar_visible = !g_sidebar_visible;
                    } else if (px >= 835 && px <= 1235 && py >= 215 && py <= 249) {
                        /* Opcin 2: Alternar dock inferior */
                        g_dock_visible = !g_dock_visible;
                    } else if (px >= 835 && px <= 1235 && py >= 255 && py <= 289) {
                        /* Opcin 3: Alternar estilo de puntero */
                        g_cursor_style = (g_cursor_style + 1) % 3;
                    } else if (px >= 835 && px <= 1235 && py >= 295 && py <= 329) {
                        /* Opcin 4: Resetear zoom y scroll */
                        scroll_y = 0;
                    } else if (px >= 835 && px <= 1235 && py >= 335 && py <= 369) {
                        /* Opcin 5: Ir al Portal de Inicio */
                        navigate_to("eonsurf://portal");
                        g_settings_menu_open = 0;
                    } else if (py >= 435 && py <= 470) {
                        g_settings_menu_open = 0;
                    }
                }"""

# Find by substring match
find_str = "px >= 820 && px <= 1250 && py >= 105"
pos = text.find(find_str)
if pos != -1:
    block_start = text.rfind("else if (g_settings_menu_open", 0, pos)
    block_end = text.find("int tab_click = eonsurf_tabs_check_click", pos)
    if block_start != -1 and block_end != -1:
        new_menu_clicks = """else if (g_settings_menu_open && px >= 820 && px <= 1260 && py >= 105 && py <= 520) {
                    if (py >= 170 && py <= 195) {
                        /* Opción 1: Alternar barra lateral */
                        g_sidebar_visible = !g_sidebar_visible;
                    } else if (py >= 196 && py <= 220) {
                        /* Opción 2: Alternar dock inferior */
                        g_dock_visible = !g_dock_visible;
                    } else if (py >= 245 && py <= 268) {
                        /* Opción 3: Portal de Inicio */
                        navigate_to("eonsurf://portal");
                        g_settings_menu_open = 0;
                    } else if (py >= 269 && py <= 290) {
                        /* Opción 4: Explorador local HDD */
                        navigate_to("file:///dev_hdd0/");
                        g_settings_menu_open = 0;
                    } else if (py >= 291 && py <= 318) {
                        /* Opción 5: Añadir a marcadores */
                        eonsurf_bookmark_add(page_title, current_url);
                    } else if (py >= 340 && py <= 368) {
                        /* Opción 6: Música ambiental */
                        eonsurf_toggle_background_music();
                    } else if (py >= 390 && py <= 420) {
                        /* Opción 7: Estilo de puntero */
                        g_cursor_style = (g_cursor_style + 1) % 3;
                    } else if (py >= 430) {
                        g_settings_menu_open = 0;
                    }
                }

                """
        text = text[:block_start] + new_menu_clicks + text[block_end:]
        print("Updated menu click options")

# 2. Add quick tab switching with L1 and R1 when not scrolling or as tab shortcuts
# Let's see how L1/R1 are currently used:
# if (pdata.BTN_L1) scroll_y -= 16;
# if (pdata.BTN_R1) scroll_y += 16;
# Let's add edge detection for L1/R1 for fast tab switching:
tab_switch_code = """
            /* Cambio rápido de pestaña con L1 / R1 */
            static int prev_btn_l1 = 0;
            static int prev_btn_r1 = 0;
            if (pdata.BTN_L1 && !prev_btn_l1) {
                int prev_tab = (g_current_tab - 1 + MAX_TABS) % MAX_TABS;
                while (!g_tabs[prev_tab].active && prev_tab != g_current_tab) {
                    prev_tab = (prev_tab - 1 + MAX_TABS) % MAX_TABS;
                }
                if (g_tabs[prev_tab].active) {
                    strncpy(g_tabs[g_current_tab].url, current_url, sizeof(g_tabs[g_current_tab].url) - 1);
                    strncpy(g_tabs[g_current_tab].title, page_title, sizeof(g_tabs[g_current_tab].title) - 1);
                    strncpy(g_tabs[g_current_tab].body, page_body, sizeof(g_tabs[g_current_tab].body) - 1);
                    g_tabs[g_current_tab].scroll_y = scroll_y;

                    eonsurf_tab_select(prev_tab);
                    strncpy(current_url, g_tabs[g_current_tab].url, sizeof(current_url) - 1);
                    strncpy(page_title, g_tabs[g_current_tab].title, sizeof(page_title) - 1);
                    strncpy(page_body, g_tabs[g_current_tab].body, sizeof(page_body) - 1);
                    scroll_y = g_tabs[g_current_tab].scroll_y;
                }
            }
            if (pdata.BTN_R1 && !prev_btn_r1) {
                int next_tab = (g_current_tab + 1) % MAX_TABS;
                while (!g_tabs[next_tab].active && next_tab != g_current_tab) {
                    next_tab = (next_tab + 1) % MAX_TABS;
                }
                if (g_tabs[next_tab].active) {
                    strncpy(g_tabs[g_current_tab].url, current_url, sizeof(g_tabs[g_current_tab].url) - 1);
                    strncpy(g_tabs[g_current_tab].title, page_title, sizeof(g_tabs[g_current_tab].title) - 1);
                    strncpy(g_tabs[g_current_tab].body, page_body, sizeof(g_tabs[g_current_tab].body) - 1);
                    g_tabs[g_current_tab].scroll_y = scroll_y;

                    eonsurf_tab_select(next_tab);
                    strncpy(current_url, g_tabs[g_current_tab].url, sizeof(current_url) - 1);
                    strncpy(page_title, g_tabs[g_current_tab].title, sizeof(page_title) - 1);
                    strncpy(page_body, g_tabs[g_current_tab].body, sizeof(page_body) - 1);
                    scroll_y = g_tabs[g_current_tab].scroll_y;
                }
            }
            prev_btn_l1 = pdata.BTN_L1;
            prev_btn_r1 = pdata.BTN_R1;
"""

scroll_target = "if (pdata.BTN_L1) scroll_y -= 16;"
if scroll_target in text and "prev_btn_l1" not in text:
    text = text.replace(scroll_target, tab_switch_code + "\n            " + scroll_target, 1)
    print("Added L1/R1 instantaneous tab switching")

with open(main_c_path, "w", encoding="utf-8") as f:
    f.write(text)

print("Finished step 4 patching.")
