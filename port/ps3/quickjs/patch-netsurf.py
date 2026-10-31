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
