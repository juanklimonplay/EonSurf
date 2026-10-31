#!/usr/bin/env python3
"""Cambios en NetSurf para el motor QuickJS de EonSurf (los llama tweak-sources.sh).

Modulos de JavaScript: NetSurf 3.11 no conoce <script type="module">. Se lo
trata como JavaScript, marcando el script como modulo; al ejecutarlo se le pasa
a js_exec() un nombre con el prefijo "module:" para que eon_qjs.c lo evalue como
modulo. Los <script nomodule> (la version vieja para navegadores sin modulos) se
saltean.
"""

HTML = 'netsurf/content/handlers/html/'


def patch(path, old, new, count=1):
    s = open(path).read()
    n = s.count(old)
    assert n >= count, (path, old[:70])
    open(path, 'w').write(s.replace(old, new))


patch(HTML + 'html.h', '\tbool already_started;\n',
      '\tbool already_started;\n\tbool eon_module; /**< EonSurf: <script type="module"> */\n')

patch(HTML + 'script.c', 'typedef bool (script_handler_t)(struct jsthread *jsthread, const uint8_t *data, size_t size, const char *name);\n',
      '''typedef bool (script_handler_t)(struct jsthread *jsthread, const uint8_t *data, size_t size, const char *name);

/* EonSurf: modulos JS. html_process_script marca el proximo script creado. */
static bool eon_module_next = false;

static bool eon_run_script(script_handler_t *handler, struct jsthread *jsthread,
			   const uint8_t *data, size_t size, const char *name,
			   bool module)
{
	char *mname;
	bool ok;
	if (!module) {
		return handler(jsthread, data, size, name);
	}
	mname = malloc(strlen(name) + 8);
	if (mname == NULL) {
		return false;
	}
	strcpy(mname, "module:");
	strcat(mname, name);
	ok = handler(jsthread, data, size, mname);
	free(mname);
	return ok;
}
''')

patch(HTML + 'script.c', '\tnscript->force_async = true;\n',
      '\tnscript->force_async = true;\n\tnscript->eon_module = eon_module_next;\n')

# los tres lugares donde se ejecuta un script
patch(HTML + 'script.c', '''				script_handler(c->jsthread, data, size,
					       nsurl_access(hlcache_handle_get_url(s->data.handle)));''',
      '''				eon_run_script(script_handler, c->jsthread, data, size,
					       nsurl_access(hlcache_handle_get_url(s->data.handle)),
					       s->eon_module);''')
patch(HTML + 'script.c', '''			script_handler(parent->jsthread, data, size,
				       nsurl_access(hlcache_handle_get_url(s->data.handle)));''',
      '''			eon_run_script(script_handler, parent->jsthread, data, size,
				       nsurl_access(hlcache_handle_get_url(s->data.handle)),
				       s->eon_module);''')
patch(HTML + 'script.c', '''		script_handler(c->jsthread,
			       (const uint8_t *)dom_string_data(script),
			       dom_string_byte_length(script),
			       "?inline script?");''',
      '''		eon_run_script(script_handler, c->jsthread,
			       (const uint8_t *)dom_string_data(script),
			       dom_string_byte_length(script),
			       "?inline script?", nscript->eon_module);''')

# type="module" -> JavaScript marcado como modulo; nomodule -> se saltea
patch(HTML + 'script.c', '''	exc = dom_element_get_attribute(node, corestring_dom_type, &mimetype);
	if (exc != DOM_NO_ERR || mimetype == NULL) {
		mimetype = dom_string_ref(corestring_dom_text_javascript);
	}
''', '''	exc = dom_element_get_attribute(node, corestring_dom_type, &mimetype);
	if (exc != DOM_NO_ERR || mimetype == NULL) {
		mimetype = dom_string_ref(corestring_dom_text_javascript);
	}

	/* EonSurf: modulos JS */
	eon_module_next = false;
	if (dom_string_byte_length(mimetype) == 9 &&
	    strncasecmp(dom_string_data(mimetype), "importmap", 9) == 0) {
		/* import map: se la pasa al motor, que traduce los import "pelados" */
		dom_string *text = NULL;
		dom_string_unref(mimetype);
		if (dom_node_get_text_content(node, &text) == DOM_NO_ERR && text != NULL) {
			js_exec(c->jsthread, (const uint8_t *)dom_string_data(text),
				dom_string_byte_length(text), "importmap:");
			dom_string_unref(text);
		}
		return DOM_HUBBUB_OK;
	}
	if (dom_string_byte_length(mimetype) == 6 &&
	    strncasecmp(dom_string_data(mimetype), "module", 6) == 0) {
		dom_string_unref(mimetype);
		mimetype = dom_string_ref(corestring_dom_text_javascript);
		eon_module_next = true;
	} else {
		dom_string *nomod = NULL;
		bool has = false;
		if (dom_string_create((const uint8_t *)"nomodule", 8, &nomod) == DOM_NO_ERR) {
			dom_element_has_attribute(node, nomod, &has);
			dom_string_unref(nomod);
		}
		if (has) {
			dom_string_unref(mimetype);
			return DOM_HUBBUB_OK;
		}
	}
''')

s = open(HTML + 'script.c').read()
for inc in ('#include <strings.h>\n', '#include <stdlib.h>\n', '#include <string.h>\n'):
    if inc not in s:
        s = inc + s
open(HTML + 'script.c', 'w').write(s)

print('EonSurf: modulos JS en NetSurf')

# Codigo HTTP real para fetch/XHR (llcache no lo expone): se lee al llegar las
# cabeceras, mientras el fetch todavia existe
with open('netsurf/content/llcache.c', 'a') as f:
    f.write('''

/* EonSurf: codigo HTTP de un pedido (0 si ya no hay fetch) */
long eon_llcache_http_code(const llcache_handle *handle);
long eon_llcache_http_code(const llcache_handle *handle)
{
	if (handle == NULL || handle->object == NULL ||
	    handle->object->fetch.fetch == NULL) {
		return 0;
	}
	return fetch_http_code(handle->object->fetch.fetch);
}
''')
print('EonSurf: codigo HTTP en llcache')

# Medicion: cuanto tarda cada recalculo de diseno (layout) de una pagina
patch(HTML + 'html.c', '''	ms_interval = (ms_after - ms_before) * 3;''', '''	NSLOG(netsurf, INFO, "EonSurf: layout %dx%d took %u ms", width, height,
	      (unsigned)(ms_after - ms_before));
	ms_interval = (ms_after - ms_before) * 3;''')
print('EonSurf: medicion de layout')


# z-index: NetSurf pinta los hijos en el orden del documento. Se reemplaza el
# cuerpo de html_redraw_box_children: primero el flujo normal y los flotantes,
# despues los hijos posicionados ordenados por z-index (estable: a igual z,
# orden del documento). Aproximado: solo entre hermanos del mismo padre.
old_body = """	struct box *c;

	for (c = box->children; c; c = c->next) {

		if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
			if (!html_redraw_box(html, c,
					x_parent + box->x -
					scrollbar_get_offset(box->scroll_x),
					y_parent + box->y -
					scrollbar_get_offset(box->scroll_y),
					clip, scale, current_background_color,
					ctx))
				return false;
	}
	for (c = box->float_children; c; c = c->next_float)
		if (!html_redraw_box(html, c,
				x_parent + box->x -
				scrollbar_get_offset(box->scroll_x),
				y_parent + box->y -
				scrollbar_get_offset(box->scroll_y),
				clip, scale, current_background_color,
				ctx))
			return false;

	return true;
}"""
new_body = """	struct box *c;
	struct box *eon_stack[64];
	int32_t eon_z[64];
	int eon_n = 0, eon_i;
	int bx = x_parent + box->x - scrollbar_get_offset(box->scroll_x);
	int by = y_parent + box->y - scrollbar_get_offset(box->scroll_y);

	for (c = box->children; c; c = c->next) {
		if (c->type == BOX_FLOAT_LEFT || c->type == BOX_FLOAT_RIGHT)
			continue;
		/* EonSurf: los posicionados se guardan para pintarlos encima */
		if (c->style != NULL && eon_n < 64 &&
		    css_computed_position(c->style) != CSS_POSITION_STATIC) {
			int32_t z = 0;
			if (css_computed_z_index(c->style, &z) != CSS_Z_INDEX_SET)
				z = 0;
			for (eon_i = eon_n; eon_i > 0 && eon_z[eon_i - 1] > z; eon_i--) {
				eon_stack[eon_i] = eon_stack[eon_i - 1];
				eon_z[eon_i] = eon_z[eon_i - 1];
			}
			eon_stack[eon_i] = c;
			eon_z[eon_i] = z;
			eon_n++;
			continue;
		}
		if (!html_redraw_box(html, c, bx, by, clip, scale,
				current_background_color, ctx))
			return false;
	}
	for (c = box->float_children; c; c = c->next_float)
		if (!html_redraw_box(html, c, bx, by, clip, scale,
				current_background_color, ctx))
			return false;

	/* EonSurf: posicionados encima, de menor a mayor z-index */
	for (eon_i = 0; eon_i < eon_n; eon_i++)
		if (!html_redraw_box(html, eon_stack[eon_i], bx, by, clip, scale,
				current_background_color, ctx))
			return false;

	return true;
}"""
_r = open(HTML + 'redraw.c').read()
assert _r.count(old_body) == 1, _r.count(old_body)
open(HTML + 'redraw.c', 'w').write(_r.replace(old_body, new_body))
print('EonSurf: z-index en el pintado')

# Nombres de atributo/elemento como en el analizador HTML: libdom exigia nombres
# XML validos y descartaba "@click", ".value", "?disabled" (Lit los usa en sus
# plantillas: sin ellos las piezas se corren y aparece codigo como texto)
_v = open('libdom/src/utils/validate.c').read()
_old = """	if (is_first_char(ch) == false)
		return false;
	
	s += clen;
	slen -= clen;
	
	while (slen > 0) {
		err = parserutils_charset_utf8_to_ucs4(s, slen, &ch, &clen);
		if (err != PARSERUTILS_OK) {
			return false;
		}

		if (is_name_char(ch) == false)
			return false;
"""
assert _v.count(_old) == 1
_new = """	/* EonSurf: criterio del analizador HTML, no de XML */
	(void)is_first_char;
	(void)is_name_char;
	while (slen > 0) {
		err = parserutils_charset_utf8_to_ucs4(s, slen, &ch, &clen);
		if (err != PARSERUTILS_OK) {
			return false;
		}

		if (ch <= 0x20 || ch == '/' || ch == '>' || ch == '=' ||
		    ch == 0x22 || ch == 0x27 || ch == 0x7f)
			return false;
"""
open('libdom/src/utils/validate.c', 'w').write(_v.replace(_old, _new))
print('EonSurf: nombres de atributo como en HTML')
