# Patch mockup_engine with progress bar, rich menu with bookmarks, audio toggle, and home
import os

repo_root = r"J:\EonPS3\EonSurf\repo"
src_dir = os.path.join(repo_root, "src", "source")
mockup_engine_path = os.path.join(src_dir, "eonsurf_mockup_engine.h")

with open(mockup_engine_path, "r", encoding="utf-8") as f:
    text = f.read()

# 1. Add progress bar rendering right below the URL bar
progress_bar_code = """
    /* Barra superior de progreso de red en tiempo real (Neón Cian) */
    extern float g_load_percent;
    extern int g_net_loading;
    if (g_net_loading && g_load_percent > 0.0f) {
        int bar_w = (int)((1140 - 192) * (g_load_percent / 100.0f));
        if (bar_w > 948) bar_w = 948;
        if (bar_w < 6) bar_w = 6;
        for (int py = 98; py <= 101; py++) {
            for (int px = 192; px < 192 + bar_w; px++) {
                mk_blend(fb, pitch, px, py, 0x00E5FF, 230);
            }
        }
    }
"""

pill_target = "eonsurf_draw_mockup_text(fb, pitch, 228, 69, 18, current_url, 0xF0F6FC, 0, 1140);"
if pill_target in text and "Barra superior de progreso de red" not in text:
    text = text.replace(pill_target, pill_target + "\n" + progress_bar_code, 1)
    print("Added neon network progress bar to mockup engine")

# 2. Enrich Settings OSD menu with Bookmarks, Home, Media, and UI controls
menu_target = "/* Men de Configuracin OSD Flotante (al presionar START o clic en emblema ES) */"
if menu_target not in text:
    # try non-accented or utf8
    for line in text.splitlines():
        if "Configuraci" in line and "OSD" in line:
            menu_target = line
            break

rich_menu = """    /* Menú de Configuración OSD Flotante (al presionar START o clic en emblema ES) */
    extern int g_settings_menu_open;
    if (g_settings_menu_open) {
        eonsurf_draw_glass_panel(fb, pitch, 820, 105, 440, 440, 14, 0x0F172A, 0x020617, 0x00E5FF, 250);
        eonsurf_draw_mockup_text(fb, pitch, 845, 122, 20, "Centro de Control EonSurf", 0x38BDF8, 1, 1240);

        /* Opciones de Interfaz */
        eonsurf_draw_mockup_text(fb, pitch, 845, 155, 15, "Interfaz y Visualizacion:", 0xFFFFFF, 1, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 178, 14, g_sidebar_visible ? "[X] Barra Lateral: Visible" : "[ ] Barra Lateral: Oculta (Lienzo 100%)", g_sidebar_visible ? 0x22C55E : 0x94A3B8, 0, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 200, 14, g_dock_visible ? "[X] Dock Inferior: Activo" : "[ ] Dock Inferior: Oculto", g_dock_visible ? 0x22C55E : 0x94A3B8, 0, 1240);

        /* Acceso Rápido y Navegación */
        eonsurf_draw_mockup_text(fb, pitch, 845, 230, 15, "Accesos Rapidos y Herramientas:", 0xFFFFFF, 1, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 252, 14, "-> 1. Portal de Inicio (eonsurf://portal)", 0x38BDF8, 0, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 274, 14, "-> 2. Explorador de Disco (file:///dev_hdd0/)", 0x38BDF8, 0, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 296, 14, "-> 3. Guardar Pagina Actual en Marcadores", 0xFACC15, 0, 1240);

        /* Multimedia y Sonido */
        extern int g_music_playing;
        eonsurf_draw_mockup_text(fb, pitch, 845, 326, 15, "Musica y Ambiente:", 0xFFFFFF, 1, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 348, 14, g_music_playing ? "[>] Audio Ambiente: Reproduciendo" : "[ ] Audio Ambiente: En Pausa", g_music_playing ? 0x22C55E : 0x94A3B8, 0, 1240);

        /* Puntero */
        eonsurf_draw_mockup_text(fb, pitch, 845, 376, 15, "Estilo de Puntero:", 0xFFFFFF, 1, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 865, 398, 14, (g_cursor_style == 0) ? "[*] Flecha Clasica OS" : (g_cursor_style == 1 ? "[*] Neon Cyan Precision" : "[*] Reticula Tactica"), 0x38BDF8, 0, 1240);

        /* Atajos Rápidos */
        eonsurf_draw_mockup_text(fb, pitch, 845, 435, 13, "L1/R1: Cambiar Pestanas | START / O: Cerrar Menu", 0x94A3B8, 0, 1240);
        eonsurf_draw_mockup_text(fb, pitch, 845, 458, 12, "EonSurf v0.3.1 | Created by JuAnKLiMoN | 2006-2026", 0x38BDF8, 1, 1240);
    }"""

old_menu_start = text.find("/* Men")
if old_menu_start != -1:
    old_menu_end = text.find("extern int g_dock_visible;", old_menu_start)
    if old_menu_end != -1:
        text = text[:old_menu_start] + rich_menu + "\n\n    " + text[old_menu_end:]
        print("Replaced settings menu with enriched modern control center")

with open(mockup_engine_path, "w", encoding="utf-8") as f:
    f.write(text)

print("Finished updating eonsurf_mockup_engine.h")
