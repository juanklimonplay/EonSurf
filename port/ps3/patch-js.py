#!/usr/bin/env python3
"""Mejoras de JavaScript de EonSurf sobre NetSurf 3.11 (etapa 1).

Se corre desde la raiz de netsurf-all-3.11 (lo llama tweak-sources.sh).

1. Redibujo en vivo: cuando un script cambia la pagina ya cargada, NetSurf 3.11
   no vuelve a armar las cajas y el cambio no se ve. Aca se rehace el arbol de
   cajas en segundo plano (el viejo se sigue mostrando mientras tanto) y se
   cambia por el nuevo al terminar.
"""
import os

NS = 'netsurf/'
HTML = NS + 'content/handlers/html/'


def patch(path, old, new, count=1):
    s = open(path).read()
    n = s.count(old)
    assert n >= count, (path, old[:60])
    open(path, 'w').write(s.replace(old, new))


# ---------------------------------------------------------------- redibujo en vivo
patch(HTML + 'private.h',
      '\t/** List of objects. */\n\tstruct content_html_object *object_list;\n',
      '\t/** List of objects. */\n\tstruct content_html_object *object_list;\n'
      '\t/** EonSurf: redibujo en vivo (ver dom_event.c) */\n'
      '\tbool eon_relayout_pending;\n'
      '\tbool eon_relayout_again;\n'
      '\tvoid *eon_old_bctx;\n'
      '\tstruct content_html_object *eon_old_objects;\n'
      '\tunsigned int eon_old_num_objects;\n')

s = open(HTML + 'dom_event.h').read()
i = s.rindex('#endif')
open(HTML + 'dom_event.h', 'w').write(s[:i] + '''/**
 * EonSurf: cancel a pending live relayout and free what it kept.
 */
struct html_content;
void eon_relayout_cancel(struct html_content *htmlc);

''' + s[i:])

patch(HTML + 'dom_event.c', '#include "html/dom_event.h"\n', '''#include "html/dom_event.h"
#include "html/interaction.h"
#include "content/content_protected.h"
#include "desktop/gui_internal.h"
#include "netsurf/misc.h"
#include "utils/talloc.h"

/*
 * EonSurf: redibujo en vivo.
 *
 * Cuando un script cambia el DOM de una pagina ya cargada se vuelve a
 * convertir todo el documento a cajas. Mientras se convierte se sigue
 * mostrando el arbol viejo; al terminar se libera junto con sus objetos.
 * Los cambios seguidos se juntan en una sola reconstruccion.
 */
#define EON_RELAYOUT_DELAY 40

static void eon_relayout_run(void *p);

static void eon_relayout_free_old(html_content *c)
{
	struct content_html_object *current = c->object_list;
	unsigned int current_num = c->num_objects;

	if (c->eon_old_objects != NULL) {
		c->object_list = c->eon_old_objects;
		html_object_free_objects(c);
		c->object_list = current;
		c->num_objects = current_num;
		c->eon_old_objects = NULL;
	}
	if (c->eon_old_bctx != NULL) {
		talloc_free(c->eon_old_bctx);
		c->eon_old_bctx = NULL;
	}
}

static void eon_relayout_done(html_content *c, bool success)
{
	c->box_conversion_context = NULL;

	if (success == false || c->aborted) {
		/* quedarse con lo viejo y tirar lo nuevo */
		html_object_free_objects(c);
		if (c->bctx != NULL) {
			talloc_free(c->bctx);
		}
		c->bctx = c->eon_old_bctx;
		c->object_list = c->eon_old_objects;
		c->num_objects = c->eon_old_num_objects;
		c->eon_old_bctx = NULL;
		c->eon_old_objects = NULL;
		NSLOG(netsurf, INFO, "EonSurf: live relayout failed");
		return;
	}

	/* nada debe apuntar a las cajas viejas */
	c->drag_type = HTML_DRAG_NONE;
	c->drag_owner.no_owner = true;
	c->selection_type = HTML_SELECTION_NONE;
	c->selection_owner.none = true;
	c->focus_type = HTML_FOCUS_SELF;
	c->focus_owner.self = true;

	eon_relayout_free_old(c);

	content__reformat(&c->base, false,
			  c->base.available_width, c->base.available_height);

	if (c->eon_relayout_again) {
		c->eon_relayout_again = false;
		c->eon_relayout_pending = true;
		guit->misc->schedule(EON_RELAYOUT_DELAY, eon_relayout_run, c);
	}
}

static void eon_relayout_run(void *p)
{
	html_content *c = p;
	dom_exception exc;
	dom_node *html;
	nserror err;

	c->eon_relayout_pending = false;

	if (c->aborted ||
	    content__get_status(&c->base) != CONTENT_STATUS_DONE) {
		return;
	}
	if (c->box_conversion_context != NULL) {
		c->eon_relayout_again = true;
		return;
	}

	exc = dom_document_get_document_element(c->document, (void *) &html);
	if ((exc != DOM_NO_ERR) || (html == NULL)) {
		return;
	}

	c->eon_old_bctx = c->bctx;
	c->eon_old_objects = c->object_list;
	c->eon_old_num_objects = c->num_objects;
	c->bctx = NULL;
	c->object_list = NULL;
	c->num_objects = 0;

	err = dom_to_box(html, c, eon_relayout_done,
			 &c->box_conversion_context);
	dom_node_unref(html);
	if (err != NSERROR_OK) {
		c->box_conversion_context = NULL;
		eon_relayout_done(c, false);
	}
}

static void eon_relayout_request(html_content *c)
{
	if (content__get_status(&c->base) != CONTENT_STATUS_DONE) {
		return;
	}
	if (c->box_conversion_context != NULL) {
		/* ya se esta reconstruyendo: repetir al terminar */
		c->eon_relayout_again = true;
		return;
	}
	if (c->eon_relayout_pending) {
		return;
	}
	c->eon_relayout_pending = true;
	guit->misc->schedule(EON_RELAYOUT_DELAY, eon_relayout_run, c);
}

/* exported interface documented in html/dom_event.h */
void eon_relayout_cancel(html_content *c)
{
	if (c->eon_relayout_pending) {
		guit->misc->schedule(-1, eon_relayout_run, c);
		c->eon_relayout_pending = false;
	}
	c->eon_relayout_again = false;
	eon_relayout_free_old(c);
}
''')

# disparar la reconstruccion: cualquier cambio en el arbol, salvo el valor
# de los campos de texto (eso ya lo maneja el propio campo)
patch(HTML + 'dom_event.c', '''			case DOM_HTML_ELEMENT_TYPE_TEXTAREA:
			case DOM_HTML_ELEMENT_TYPE_INPUT:
				html_texty_element_update(htmlc, (dom_node *)node);
			default:
				break;
			}
		}
		dom_node_unref(node);
	}
}''', '''			case DOM_HTML_ELEMENT_TYPE_TEXTAREA:
			case DOM_HTML_ELEMENT_TYPE_INPUT:
				html_texty_element_update(htmlc, (dom_node *)node);
				dom_node_unref(node);
				return;
			default:
				break;
			}
		}
		eon_relayout_request(htmlc);
		dom_node_unref(node);
	}
}''')

# al destruir la pagina, cancelar lo pendiente antes de liberar objetos
patch(HTML + 'html.c', '\t/* Free objects */\n\thtml_object_free_objects(html);\n',
      '\t/* EonSurf: redibujo en vivo pendiente */\n\teon_relayout_cancel(html);\n\n'
      '\t/* Free objects */\n\thtml_object_free_objects(html);\n')
s = open(HTML + 'html.c').read()
if '#include "html/dom_event.h"' not in s:
    patch(HTML + 'html.c', '#include "html/private.h"\n',
          '#include "html/private.h"\n#include "html/dom_event.h"\n')

# al rehacer las cajas, libcss vuelve a guardar sus datos en cada nodo:
# liberar los anteriores en vez de abortar
patch(NS + 'content/handlers/css/select.c', '\tassert(old_node_data == NULL);\n',
      '\tif (old_node_data != NULL) {\n'
      '\t\t/* EonSurf: datos de una seleccion anterior (redibujo en vivo) */\n'
      '\t\tnscss_dom_user_data_handler(DOM_NODE_DELETED,\n'
      '\t\t\t\tcorestring_dom___ns_key_libcss_node_data,\n'
      '\t\t\t\told_node_data, n, NULL);\n'
      '\t}\n')

print('JS EonSurf: redibujo en vivo aplicado')
