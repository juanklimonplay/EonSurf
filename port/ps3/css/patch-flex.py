#!/usr/bin/env python3
"""Flexbox moderno en NetSurf (lo llama patch-css.py desde netsurf-all-3.11).

El flex de NetSurf 3.11 no tiene gap ni justify-content, y casi todas las
paginas actuales los usan (cabeceras con el logo a la izquierda y el menu a la
derecha, botones centrados, tarjetas separadas). Aca se agregan:

- gap: el preprocesador CSS deja "gap" como column-gap (libcss no tiene
  row-gap), y se usa en los dos ejes. Las lineas se arman sin contar los gaps y
  despues los items se encogen para hacerles lugar: asi una grilla convertida a
  flex (3 columnas de 33,333%) sigue entrando en una fila.
- justify-content: flex-end, center, space-between, space-around, space-evenly.
- los hijos directos inline (<span>, <a>, <img>) pasan a ser items propios.
"""

F = 'netsurf/content/handlers/html/layout_flex.c'


def patch(old, new):
    s = open(F).read()
    assert old in s, old[:60]
    open(F, 'w').write(s.replace(old, new, 1))


patch('''	int pos;

	size_t first;
	size_t count;
	size_t frozen;
};''', '''	int pos;
	int gaps; /* EonSurf: suma de los gaps entre items de la linea */

	size_t first;
	size_t count;
	size_t frozen;
};''')

patch('''	bool horizontal;
	bool main_reversed;
	enum css_flex_wrap_e wrap;''', '''	bool horizontal;
	bool main_reversed;
	enum css_flex_wrap_e wrap;

	int main_gap; /* EonSurf: gap entre items */
	int cross_gap; /* EonSurf: gap entre lineas */''')

patch('''	ctx->main_reversed = lh__flex_direction_reversed(flex);

	return ctx;''', '''	ctx->main_reversed = lh__flex_direction_reversed(flex);

	{
		/* EonSurf: gap (llega como column-gap) */
		css_fixed glen = 0;
		css_unit gunit = CSS_UNIT_PX;

		if (css_computed_column_gap(flex->style, &glen, &gunit) ==
				CSS_COLUMN_GAP_SET && gunit != CSS_UNIT_PCT) {
			ctx->main_gap = FIXTOINT(css_unit_len2device_px(
					flex->style, &content->unit_len_ctx,
					glen, gunit));
			if (ctx->main_gap < 0) {
				ctx->main_gap = 0;
			}
		}
		ctx->cross_gap = ctx->main_gap;
	}

	return ctx;''')

# armar la linea: contar los items en flujo para saber cuantos gaps hay
patch('''	struct flex_line_data *line;
	int used_main = 0;

	if (!layout_flex_ctx__ensure_line(ctx)) {''', '''	struct flex_line_data *line;
	int used_main = 0;
	int inflow = 0;

	if (!layout_flex_ctx__ensure_line(ctx)) {''')

patch('''			if (lh__box_is_absolute(item->box) == false) {
				line->main_size += item->main_size;
				used_main += pos_main;
''', '''			if (lh__box_is_absolute(item->box) == false) {
				line->main_size += item->main_size;
				used_main += pos_main;
				inflow++;
''')

patch('''	if (line->count > 0) {
		ctx->line.count++;
	} else {''', '''	line->gaps = inflow > 1 ? ctx->main_gap * (inflow - 1) : 0;

	if (line->count > 0) {
		ctx->line.count++;
	} else {''')

# resolver: el espacio de los gaps no es para los items
patch('''	if (available_main == AUTO) {
		available_main = INT_MAX;
	}

	grow = (line->main_size < available_main);''', '''	if (available_main == AUTO) {
		available_main = INT_MAX;
	} else {
		available_main -= line->gaps; /* EonSurf */
	}

	grow = (line->main_size < available_main);''')

# colocar: gaps y justify-content
patch('''	size_t item_count = line->first + line->count;
	int extra_remainder = 0;
	int extra = 0;

	if (ctx->main_reversed) {
		main_pos = lh__box_size_main(ctx->horizontal, ctx->flex) -
				main_pos;
	}

	if (ctx->available_main != AUTO &&
	    ctx->available_main != UNKNOWN_WIDTH &&
	    ctx->available_main > line->used_main_size) {
		if (line->main_auto_margin_count > 0) {
			extra = ctx->available_main - line->used_main_size;

			extra_remainder = extra % line->main_auto_margin_count;
			extra /= line->main_auto_margin_count;
		}
	}
''', '''	size_t item_count = line->first + line->count;
	int extra_remainder = 0;
	int extra = 0;
	int dir = ctx->main_reversed ? -1 : 1; /* EonSurf */
	int between = 0;
	bool first_item = true;

	if (ctx->main_reversed) {
		main_pos = lh__box_size_main(ctx->horizontal, ctx->flex) -
				main_pos;
	}

	if (ctx->available_main != AUTO &&
	    ctx->available_main != UNKNOWN_WIDTH &&
	    ctx->available_main - line->gaps > line->used_main_size) {
		int free_main = ctx->available_main - line->gaps -
				line->used_main_size;
		int n = 0;

		for (size_t i = line->first; i < item_count; i++) {
			if (!lh__box_is_absolute(ctx->item.data[i].box)) {
				n++;
			}
		}

		if (line->main_auto_margin_count > 0) {
			extra = free_main;

			extra_remainder = extra % line->main_auto_margin_count;
			extra /= line->main_auto_margin_count;
		} else if (n > 0) {
			/* EonSurf: justify-content */
			switch (css_computed_justify_content(ctx->flex->style)) {
			case CSS_JUSTIFY_CONTENT_FLEX_END:
				main_pos += dir * free_main;
				break;
			case CSS_JUSTIFY_CONTENT_CENTER:
				main_pos += dir * (free_main / 2);
				break;
			case CSS_JUSTIFY_CONTENT_SPACE_BETWEEN:
				if (n > 1) {
					between = free_main / (n - 1);
				}
				break;
			case CSS_JUSTIFY_CONTENT_SPACE_AROUND:
				between = free_main / n;
				main_pos += dir * (between / 2);
				break;
			case CSS_JUSTIFY_CONTENT_SPACE_EVENLY:
				between = free_main / (n + 1);
				main_pos += dir * between;
				break;
			default:
				break;
			}
		}
	}
''')

patch('''		box_size_main = lh__box_size_main(ctx->horizontal, b);
		box_pos_main = ctx->horizontal ? &b->x : &b->y;

		if (!lh__box_is_absolute(b)) {
			if (b->margin[main_start] == AUTO) {''', '''		box_size_main = lh__box_size_main(ctx->horizontal, b);
		box_pos_main = ctx->horizontal ? &b->x : &b->y;

		if (!lh__box_is_absolute(b)) {
			/* EonSurf: gap y reparto de justify-content */
			if (!first_item) {
				main_pos += dir * (ctx->main_gap + between);
			}
			first_item = false;

			if (b->margin[main_start] == AUTO) {''')

# lineas: gap entre lineas y tamano del contenedor con los gaps
patch('''		ctx->cross_size += line->cross_size;
		if (ctx->main_size < line->main_size) {
			ctx->main_size = line->main_size;
		}''', '''		ctx->cross_size += line->cross_size;
		if (ctx->line.count > 1) {
			ctx->cross_size += ctx->cross_gap; /* EonSurf */
		}
		if (ctx->main_size < line->main_size + line->gaps) {
			ctx->main_size = line->main_size + line->gaps;
		}''')

patch('''		layout_flex__place_line_items_cross(ctx, line,
				extra + extra_remainder);
''', '''		if (i + 1 < ctx->line.count) {
			line_pos += (reversed ? -1 : 1) * ctx->cross_gap; /* EonSurf */
		}

		layout_flex__place_line_items_cross(ctx, line,
				extra + extra_remainder);
''')

# hijos directos de un flex: son items aunque sean <span>, <a> o <img>
# (NetSurf solo convertia inline-block e inline-flex, y metia todos los inline
# juntos en un solo item anonimo: sin gap ni space-between entre ellos)
B = 'netsurf/content/handlers/html/box_construct.c'
s = open(B).read()
old = """			case BOX_INLINE_BLOCK:
				box->type = BOX_BLOCK;
				break;
			default:
				break;
			}
		}
	}
"""
assert old in s
s = s.replace(old, """			case BOX_INLINE_BLOCK:
				box->type = BOX_BLOCK;
				break;
			case BOX_INLINE:
				/* EonSurf: solo si el flex es el padre directo
				 * (containing_block se saltea los inline) */
				{
					dom_node *eon_p = NULL;
					if (dom_node_get_parent_node(ctx->n, &eon_p) ==
							DOM_NO_ERR && eon_p != NULL) {
						if (box_for_node(eon_p) ==
								props.containing_block) {
							box->type = BOX_BLOCK;
						}
						dom_node_unref(eon_p);
					}
				}
				break;
			default:
				break;
			}
		}
	}
""", 1)
open(B, 'w').write(s)

# aspect-ratio: el preprocesador CSS lo pasa como column-count (100000 + ancho/alto
# * 1000), que libcss entiende y NetSurf no usa (no tiene columnas). El alto
# sale del ancho cuando el alto es auto.
L = 'netsurf/content/handlers/html/layout_internal.h'
s = open(L).read()
old = """static inline void layout_find_dimensions(
		const css_unit_ctx *unit_len_ctx,"""
assert old in s
s = s.replace(old, """/* EonSurf: alto segun aspect-ratio (AUTO si no tiene) */
static inline int eon_aspect_height(const css_computed_style *style, int width)
{
	int32_t ar = 0;

	if (style == NULL || width == AUTO || width <= 0) {
		return AUTO;
	}
	if (css_computed_column_count(style, &ar) != CSS_COLUMN_COUNT_SET ||
			ar <= 100000) {
		return AUTO;
	}
	return (int)((int64_t)width * 1000 / (ar - 100000));
}

""" + old, 1)
old = """		if (*height != AUTO) {
			layout_handle_box_sizing(unit_len_ctx, box,
					available_width, false, height);
		}
	}
"""
assert old in s
s = s.replace(old, """		if (*height != AUTO) {
			layout_handle_box_sizing(unit_len_ctx, box,
					available_width, false, height);
		} else if (width != NULL) {
			*height = eon_aspect_height(style, *width); /* EonSurf */
		}
	}
""", 1)
open(L, 'w').write(s)

LC = 'netsurf/content/handlers/html/layout.c'
s = open(LC).read()
old = """	box->width = layout_solve_width(box, available_width, width, lm, rm,
			max_width, min_width);
	box->height = height;
"""
assert old in s
s = s.replace(old, old + """	if (box->height == AUTO) {
		box->height = eon_aspect_height(style, box->width); /* EonSurf */
	}
""", 1)
open(LC, 'w').write(s)

s = open(F).read()
old = """		if (ctx->horizontal) {
			b->width = item->target_main_size -
					lh__delta_outer_width(b);
"""
assert old in s
s = s.replace(old, old + """			if (b->height == AUTO) {
				b->height = eon_aspect_height(b->style, b->width);
			}
""", 1)
open(F, 'w').write(s)

print('EonSurf: flex con gap, justify-content, blockificacion y aspect-ratio')
