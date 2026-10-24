with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    text = f.read()

old_tb = '''        /* 2. Barra de herramientas superior moderna */
        rect(0, 0, W, 58, 0x00151821);
        rect(0, 58, W, 2, 0x003b82f6);

        rect_rounded(20, 14, 32, 32, 4, 0x002d3748);
        text_hd(28, 22, 1, "<", 0x00ffffff);

        rect_rounded(60, 14, 32, 32, 4, 0x002d3748);
        text_hd(68, 22, 1, ">", 0x00ffffff);

        /* Barra de Direcciones / Busqueda */
        int url_bar_w = W - 260;
        rect_rounded(105, 12, url_bar_w, 36, 6, 0x000f131a);
        rect(105, 46, url_bar_w, 2, 0x003b82f6);

        snprintf(buf, sizeof(buf), "%s", current_url);
        text_hd(120, 22, 1, buf, 0x0093c5fd);

        /* Indicador de red activa */
        rect_rounded(W - 140, 14, 120, 32, 4, net_initialized ? 0x00059669 : 0x004b5563);
        text_hd(W - 128, 22, 1, net_initialized ? "ONLINE" : "OFFLINE", 0x00ffffff);'''

new_tb = '''        /* 2. Barra de herramientas superior moderna Titanium Dark */
        rect(0, 0, W, 58, 0x000b0f17);
        rect(0, 58, W, 2, 0x000284c7);

        /* Botones Atrás / Adelante con píldora brillante */
        draw_rounded_pill(fb[cur], W, 20, 12, 36, 36, 6, 0x001e293b, 0x0038bdf8);
        text_hd(32, 22, 1, "<", 0x00ffffff);

        draw_rounded_pill(fb[cur], W, 64, 12, 36, 36, 6, 0x001e293b, 0x0038bdf8);
        text_hd(76, 22, 1, ">", 0x00ffffff);

        /* Barra de Direcciones / Búsqueda con píldora y borde de brillo cian */
        int url_bar_w = W - 390;
        draw_rounded_pill(fb[cur], W, 112, 11, url_bar_w, 38, 8, 0x00030712, 0x000284c7);

        snprintf(buf, sizeof(buf), "%s", current_url);
        text_hd(126, 22, 1, buf, 0x0038bdf8);

        /* Badge oficial Cell B.E. • 60 FPS */
        draw_rounded_pill(fb[cur], W, W - 264, 13, 148, 34, 6, 0x000f172a, 0x0006b6d4);
        text_hd(W - 252, 22, 1, "Cell B.E. • 60fps", 0x0038bdf8);

        /* Indicador de red activa Online/Offline */
        draw_rounded_pill(fb[cur], W, W - 104, 13, 84, 34, 6, net_initialized ? 0x00064e3b : 0x00450a0a, net_initialized ? 0x0010b981 : 0x00ef4444);
        text_hd(W - 90, 22, 1, net_initialized ? "ONLINE" : "OFFLINE", 0x00ffffff);'''

if old_tb in text:
    text = text.replace(old_tb, new_tb, 1)
    with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
        f.write(text)
    print("Replaced toolbar with Titanium pill styling")
else:
    print("old_tb not found")
