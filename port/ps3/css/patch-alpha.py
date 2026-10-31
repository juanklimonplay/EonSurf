#!/usr/bin/env python3
"""Fondos semitransparentes (rgba, hsla, #rrggbbaa) en las paginas.

El plotter del framebuffer pintaba el color opaco aunque tuviera alfa: un fondo
rgba(65,137,230,.15) (celeste muy claro) salia azul fuerte y el texto azul de
encima desaparecia (botones de MercadoLibre), y los fondos oscuros de los
modales tapaban todo. El color de NetSurf trae (1 - alfa) en el byte alto; se
pinta como un bitmap de 1x1 con alfa, que libnsfb ya sabe mezclar. (La
interfaz propia, fbtk, usa la convencion contraria y llama a libnsfb directo:
no pasa por aca.)
"""

F = 'netsurf/frontends/framebuffer/framebuffer.c'
s = open(F).read()
old = """	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		nsfb_plot_rectangle_fill(nsfb, &rect, style->fill_colour);
	}"""
assert old in s
s = s.replace(old, """	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		colour fc = style->fill_colour;
		if ((fc >> 24) != 0) {
			/* EonSurf: color con alfa */
			nsfb_colour_t px = ((0xffu - (fc >> 24)) << 24) |
					(fc & 0xffffff);
			if ((px >> 24) != 0) {
				nsfb_plot_bitmap(nsfb, &rect, &px, 1, 1, 1, true);
			}
		} else {
			nsfb_plot_rectangle_fill(nsfb, &rect, fc);
		}
	}""", 1)
open(F, 'w').write(s)
print('EonSurf: fondos con alfa')
