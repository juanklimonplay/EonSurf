with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    c = f.read()

idx1 = c.find('/* 6. Barra de estado inferior */')
idx2 = c.find('/* 7. Puntero de alta visibilidad')

new_bottom = '''/* 6. Barra de estado inferior translúcida con botones PS3 estilizados */
        rect(0, H - 36, W, 36, 0x000b0f17);
        rect(0, H - 36, W, 1, 0x001e293b);

        /* Iconos de botones estilo PlayStation */
        text_hd(24, H - 24, 1, "/_\\\\ Buscar / URL", 0x0010b981); /* Triángulo Verde */
        text_hd(190, H - 24, 1, "X Abrir Enlace", 0x0038bdf8);   /* Cruz Azul */
        text_hd(350, H - 24, 1, "O Volver Atras", 0x00f43f5e);   /* Círculo Rojo */
        text_hd(510, H - 24, 1, "[R-Stick / Pad] Desplazar", 0x0094a3b8);
        text_hd(770, H - 24, 1, "[SELECT+START] Salir", 0x0064748b);

        /* Coordenadas en píldora compacta */
        draw_rounded_pill(fb[cur], W, W - 180, H - 30, 160, 24, 4, 0x00030712, 0x001e293b);
        snprintf(buf, sizeof(buf), "RSX: %d, %d", px, py);
        text_hd(W - 165, H - 23, 1, buf, 0x0038bdf8);

        '''

c = c[:idx1] + new_bottom + c[idx2:]
with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
    f.write(c)
print('Successfully replaced bottom bar!')
