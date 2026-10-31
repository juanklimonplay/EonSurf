/*
 * EonSurf: motor JavaScript QuickJS-ng para NetSurf (implementa javascript/js.h).
 *
 * La parte en C es chica a proposito: expone al JavaScript un objeto oculto
 * "__ns" con operaciones primitivas sobre libdom (nodos, atributos, arbol,
 * formularios), temporizadores, eventos y red. Toda la API web estandar
 * (Node, Element, Event, classList, style, fetch...) esta escrita en JavaScript
 * moderno en dom.js, que se compila dentro del programa.
 *
 * Cada nodo de libdom tiene un unico objeto JS (identidad estable: a === b).
 * Los objetos se guardan en __ns.keep[id] mientras viva la pagina.
 *
 * Copyright 2026 EonSurf. Licencia MIT, como el resto de los bindings de NetSurf.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>

#include <curl/curl.h>
#include <nsutils/time.h>
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>

#include "quickjs.h"

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/corestrings.h"
#include "netsurf/browser_window.h"
#include "netsurf/misc.h"
#include "desktop/gui_internal.h"
#include "desktop/browser_history.h"
#include "content/content.h"
#include "content/content_protected.h"
#include "content/llcache.h"
#include "content/urldb.h"
#include "html/private.h"
#include "html/box.h"
#include "html/box_inspect.h"

#include "javascript/js.h"
#include "javascript/content.h"

#include "eon_dom_js.h" /* dom.js como arreglo de bytes: eon_dom_js, eon_dom_js_len */

#ifndef EONSURF_STORAGE_DIR
#define EONSURF_STORAGE_DIR "/dev_hdd0/game/EONS00001/USRDIR/storage"
#endif
#define EON_EXEC_TIMEOUT_MS 30000 /* el PPU es lento: sitios como BBC necesitan mas de 10 s */
#define EON_STORAGE_MAX (4 * 1024 * 1024)
#define EON_MAX_LISTEN 64

struct jsheap {
	JSRuntime *rt;
	unsigned int live_threads;
	bool pending_destroy;
	uint64_t exec_start; /* ms; 0 = no esta corriendo JS */
};

typedef struct eon_timer {
	struct eon_timer *next;
	struct jsthread *thread;
	int32_t id;
} eon_timer;

long eon_llcache_http_code(const llcache_handle *handle); /* patch-netsurf.py */

typedef struct eon_req {
	struct eon_req *next;
	struct jsthread *thread;
	int32_t id;
	long http_code;
	llcache_handle *handle;
	char *post;
} eon_req;

struct jsthread {
	jsheap *heap;
	JSContext *ctx;
	struct browser_window *bw;
	struct html_content *htmlc;
	JSValue ns;          /* objeto __ns */
	JSValue keep;        /* __ns.keep: objetos de nodos */
	int32_t next_node_id;
	bool closed;         /* la pagina se cerro: no correr mas JS */
	bool pending_destroy;
	unsigned int in_use;
	eon_timer *timers;
	dom_event_listener *listener;
	dom_string *listening[EON_MAX_LISTEN];
	unsigned int n_listening;
};

static JSClassID eon_node_class_id;
static dom_string *eon_key;  /* clave de user data: id del objeto JS del nodo */
static eon_req *eon_reqs;

/* ------------------------------------------------------------------ util */

static uint64_t eon_now_ms(void)
{
	uint64_t ms = 0;
	nsu_getmonotonic_ms(&ms);
	return ms;
}

static int eon_interrupt(JSRuntime *rt, void *opaque)
{
	jsheap *heap = opaque;
	(void)rt;
	return heap->exec_start != 0 &&
		eon_now_ms() > heap->exec_start + EON_EXEC_TIMEOUT_MS;
}

static struct jsthread *eon_thread(JSContext *ctx)
{
	return JS_GetContextOpaque(ctx);
}

static void eon_log_exception(JSContext *ctx, const char *where)
{
	JSValue e = JS_GetException(ctx);
	const char *msg = JS_ToCString(ctx, e);
	JSValue stack = JS_IsError(e) ?
		JS_GetPropertyStr(ctx, e, "stack") : JS_UNDEFINED;
	const char *st = JS_IsUndefined(stack) ? NULL : JS_ToCString(ctx, stack);
	NSLOG(netsurf, WARNING, "JS error in %s: %s %s", where,
	      msg ? msg : "?", st ? st : "");
	if (st) JS_FreeCString(ctx, st);
	JS_FreeValue(ctx, stack);
	if (msg) JS_FreeCString(ctx, msg);
	JS_FreeValue(ctx, e);
}

/* promesas rechazadas sin manejar: al registro (ahi fallan los modulos) */
static void eon_rejection(JSContext *ctx, JSValueConst promise, JSValueConst reason,
			  bool is_handled, void *opaque)
{
	const char *msg;
	(void)promise; (void)opaque;
	if (is_handled) return;
	msg = JS_ToCString(ctx, reason);
	NSLOG(netsurf, WARNING, "JS unhandled rejection: %s", msg ? msg : "?");
	if (msg) JS_FreeCString(ctx, msg);
}

/* Promesas y demas trabajos pendientes: correrlos despues de cada entrada a JS */
static void eon_run_jobs(struct jsthread *t)
{
	JSContext *c;
	int n = 0;
	while (n++ < 10000 && JS_ExecutePendingJob(t->heap->rt, &c) > 0) {
	}
	if (n >= 10000) {
		NSLOG(netsurf, WARNING, "JS: too many pending jobs");
	}
}

/* Entrar a JS desde C: marcar el tiempo para cortar scripts colgados */
static bool eon_enter(struct jsthread *t)
{
	if (t == NULL || t->closed || t->pending_destroy) {
		return false;
	}
	t->in_use++;
	t->heap->exec_start = eon_now_ms();
	return true;
}

static void eon_destroy_thread_now(struct jsthread *t);

static void eon_leave(struct jsthread *t)
{
	eon_run_jobs(t);
	t->heap->exec_start = 0;
	t->in_use--;
	if (t->in_use == 0 && t->pending_destroy) {
		eon_destroy_thread_now(t);
	}
}

/* llamar __ns.<name>(args...) */
static JSValue eon_call_ns(struct jsthread *t, const char *name, int argc, JSValue *argv)
{
	JSValue f = JS_GetPropertyStr(t->ctx, t->ns, name);
	JSValue r;
	if (!JS_IsFunction(t->ctx, f)) {
		JS_FreeValue(t->ctx, f);
		return JS_UNDEFINED;
	}
	r = JS_Call(t->ctx, f, t->ns, argc, argv);
	JS_FreeValue(t->ctx, f);
	if (JS_IsException(r)) {
		eon_log_exception(t->ctx, name);
		r = JS_UNDEFINED;
	}
	return r;
}

/* ------------------------------------------------------------------ cadenas */

static JSValue eon_ds_to_js(JSContext *ctx, dom_string *ds)
{
	if (ds == NULL) {
		return JS_NULL;
	}
	return JS_NewStringLen(ctx, dom_string_data(ds), dom_string_byte_length(ds));
}

/* toma la referencia de ds */
static JSValue eon_ds_take(JSContext *ctx, dom_string *ds)
{
	JSValue v = eon_ds_to_js(ctx, ds);
	if (ds != NULL) dom_string_unref(ds);
	return v;
}

static dom_string *eon_js_to_ds(JSContext *ctx, JSValueConst v)
{
	size_t len;
	dom_string *ds = NULL;
	const char *s = JS_ToCStringLen(ctx, &len, v);
	if (s == NULL) {
		return NULL;
	}
	if (dom_string_create((const uint8_t *)s, len, &ds) != DOM_NO_ERR) {
		ds = NULL;
	}
	JS_FreeCString(ctx, s);
	return ds;
}

/* ------------------------------------------------------------------ nodos */

typedef struct {
	dom_node *node;
} eon_node_t;

static void eon_node_finalizer(JSRuntime *rt, JSValue val)
{
	eon_node_t *n = JS_GetOpaque(val, eon_node_class_id);
	(void)rt;
	if (n != NULL) {
		void *old = NULL;
		dom_node_set_user_data(n->node, eon_key, NULL, NULL, &old);
		dom_node_unref(n->node);
		free(n);
	}
}

static JSClassDef eon_node_class = {
	"EonNode",
	.finalizer = eon_node_finalizer,
};

static dom_node *eon_unwrap(JSContext *ctx, JSValueConst v)
{
	eon_node_t *n = JS_GetOpaque(v, eon_node_class_id);
	(void)ctx;
	return n != NULL ? n->node : NULL;
}

/* objeto JS unico para un nodo; no toma la referencia de node */
static JSValue eon_wrap(JSContext *ctx, dom_node *node)
{
	struct jsthread *t = eon_thread(ctx);
	void *ud = NULL;
	eon_node_t *n;
	JSValue obj, args[3];
	dom_node_type type;
	dom_string *name = NULL;

	if (node == NULL) {
		return JS_NULL;
	}
	if (dom_node_get_user_data(node, eon_key, &ud) == DOM_NO_ERR && ud != NULL) {
		JSValue o = JS_GetPropertyUint32(ctx, t->keep, (uint32_t)(uintptr_t)ud);
		if (!JS_IsUndefined(o)) {
			return o;
		}
	}

	obj = JS_NewObjectClass(ctx, (int)eon_node_class_id);
	if (JS_IsException(obj)) {
		return obj;
	}
	n = calloc(1, sizeof(*n));
	if (n == NULL) {
		JS_FreeValue(ctx, obj);
		return JS_ThrowOutOfMemory(ctx);
	}
	n->node = dom_node_ref(node);
	JS_SetOpaque(obj, n);

	t->next_node_id++;
	dom_node_set_user_data(node, eon_key, (void *)(uintptr_t)t->next_node_id, NULL, &ud);
	JS_SetPropertyUint32(ctx, t->keep, (uint32_t)t->next_node_id, JS_DupValue(ctx, obj));

	/* dom.js le pone el prototipo correcto (HTMLDivElement, Text...) */
	dom_node_get_node_type(node, &type);
	dom_node_get_node_name(node, &name);
	args[0] = obj;
	args[1] = JS_NewInt32(ctx, (int32_t)type);
	args[2] = eon_ds_take(ctx, name);
	JS_FreeValue(ctx, eon_call_ns(t, "setup", 3, args));
	JS_FreeValue(ctx, args[2]);
	return obj;
}

/* toma la referencia de node */
static JSValue eon_wrap_take(JSContext *ctx, dom_node *node)
{
	JSValue v = eon_wrap(ctx, node);
	if (node != NULL) dom_node_unref(node);
	return v;
}

#define NODE_ARG(i, var) \
	dom_node *var = eon_unwrap(ctx, argv[i]); \
	if (var == NULL) return JS_ThrowTypeError(ctx, "not a node")

static JSValue eon_dom_error(JSContext *ctx, dom_exception exc)
{
	return JS_ThrowTypeError(ctx, "DOM error %d", (int)exc);
}

/* ------------------------------------------------------------------ __ns: arbol */

static JSValue ns_document(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct jsthread *t = eon_thread(ctx);
	(void)this_val; (void)argc; (void)argv;
	if (t->htmlc == NULL || t->htmlc->document == NULL) {
		return JS_NULL;
	}
	return eon_wrap(ctx, (dom_node *)t->htmlc->document);
}

static JSValue ns_type(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_node_type type;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	dom_node_get_node_type(n, &type);
	return JS_NewInt32(ctx, (int32_t)type);
}

static JSValue ns_name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_string *s = NULL;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	dom_node_get_node_name(n, &s);
	return eon_ds_take(ctx, s);
}

static JSValue ns_value(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_string *s = NULL;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	dom_node_get_node_value(n, &s);
	return eon_ds_take(ctx, s);
}

static JSValue ns_set_value(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_string *s;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	s = eon_js_to_ds(ctx, argv[1]);
	dom_node_set_node_value(n, s);
	if (s) dom_string_unref(s);
	return JS_UNDEFINED;
}

static JSValue ns_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_string *s = NULL;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	dom_node_get_text_content(n, &s);
	return eon_ds_take(ctx, s);
}

static JSValue ns_set_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_string *s;
	dom_exception exc;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	s = eon_js_to_ds(ctx, argv[1]);
	exc = dom_node_set_text_content(n, s);
	if (s) dom_string_unref(s);
	return exc == DOM_NO_ERR ? JS_UNDEFINED : eon_dom_error(ctx, exc);
}

#define NAV_FUNC(fname, getter) \
static JSValue fname(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) \
{ \
	dom_node *r = NULL; \
	NODE_ARG(0, n); \
	(void)this_val; (void)argc; \
	getter(n, &r); \
	return eon_wrap_take(ctx, r); \
}
NAV_FUNC(ns_parent, dom_node_get_parent_node)
NAV_FUNC(ns_first, dom_node_get_first_child)
NAV_FUNC(ns_last, dom_node_get_last_child)
NAV_FUNC(ns_next, dom_node_get_next_sibling)
NAV_FUNC(ns_prev, dom_node_get_previous_sibling)

static JSValue ns_children(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue arr = JS_NewArray(ctx);
	dom_node *c = NULL, *next;
	uint32_t i = 0;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	dom_node_get_first_child(n, &c);
	while (c != NULL) {
		JS_SetPropertyUint32(ctx, arr, i++, eon_wrap(ctx, c));
		next = NULL;
		dom_node_get_next_sibling(c, &next);
		dom_node_unref(c);
		c = next;
	}
	return arr;
}

static JSValue ns_append(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_node *r = NULL;
	dom_exception exc;
	NODE_ARG(0, p);
	NODE_ARG(1, c);
	(void)this_val; (void)argc;
	exc = dom_node_append_child(p, c, &r);
	if (exc != DOM_NO_ERR) return eon_dom_error(ctx, exc);
	return eon_wrap_take(ctx, r);
}

static JSValue ns_insert(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_node *r = NULL;
	dom_node *ref = argc > 2 ? eon_unwrap(ctx, argv[2]) : NULL;
	dom_exception exc;
	NODE_ARG(0, p);
	NODE_ARG(1, c);
	(void)this_val;
	exc = ref != NULL ? dom_node_insert_before(p, c, ref, &r) :
			    dom_node_append_child(p, c, &r);
	if (exc != DOM_NO_ERR) return eon_dom_error(ctx, exc);
	return eon_wrap_take(ctx, r);
}

static JSValue ns_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_node *r = NULL;
	dom_exception exc;
	NODE_ARG(0, p);
	NODE_ARG(1, c);
	(void)this_val; (void)argc;
	exc = dom_node_remove_child(p, c, &r);
	if (exc != DOM_NO_ERR) return eon_dom_error(ctx, exc);
	return eon_wrap_take(ctx, r);
}

static JSValue ns_replace(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_node *r = NULL;
	dom_exception exc;
	NODE_ARG(0, p);
	NODE_ARG(1, nw);
	NODE_ARG(2, old);
	(void)this_val; (void)argc;
	exc = dom_node_replace_child(p, nw, old, &r);
	if (exc != DOM_NO_ERR) return eon_dom_error(ctx, exc);
	return eon_wrap_take(ctx, r);
}

static JSValue ns_clone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_node *r = NULL;
	dom_exception exc;
	NODE_ARG(0, n);
	(void)this_val;
	exc = dom_node_clone_node(n, argc > 1 && JS_ToBool(ctx, argv[1]), &r);
	if (exc != DOM_NO_ERR) return eon_dom_error(ctx, exc);
	return eon_wrap_take(ctx, r);
}

/* ------------------------------------------------------------------ __ns: crear */

static dom_document *eon_doc(JSContext *ctx)
{
	struct jsthread *t = eon_thread(ctx);
	return (t->htmlc != NULL) ? t->htmlc->document : NULL;
}

static JSValue ns_create_element(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_document *doc = eon_doc(ctx);
	dom_string *tag;
	dom_element *el = NULL;
	dom_exception exc;
	(void)this_val; (void)argc;
	if (doc == NULL) return JS_NULL;
	tag = eon_js_to_ds(ctx, argv[0]);
	if (tag == NULL) return JS_EXCEPTION;
	exc = dom_document_create_element(doc, tag, &el);
	dom_string_unref(tag);
	if (exc != DOM_NO_ERR) return eon_dom_error(ctx, exc);
	return eon_wrap_take(ctx, (dom_node *)el);
}

static JSValue ns_create_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_document *doc = eon_doc(ctx);
	dom_string *s;
	dom_text *n = NULL;
	(void)this_val; (void)argc;
	if (doc == NULL) return JS_NULL;
	s = eon_js_to_ds(ctx, argv[0]);
	if (s == NULL) return JS_EXCEPTION;
	dom_document_create_text_node(doc, s, &n);
	dom_string_unref(s);
	return eon_wrap_take(ctx, (dom_node *)n);
}

static JSValue ns_create_comment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_document *doc = eon_doc(ctx);
	dom_string *s;
	dom_comment *n = NULL;
	(void)this_val; (void)argc;
	if (doc == NULL) return JS_NULL;
	s = eon_js_to_ds(ctx, argv[0]);
	if (s == NULL) return JS_EXCEPTION;
	dom_document_create_comment(doc, s, &n);
	dom_string_unref(s);
	return eon_wrap_take(ctx, (dom_node *)n);
}

static JSValue ns_create_fragment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_document *doc = eon_doc(ctx);
	dom_document_fragment *n = NULL;
	(void)this_val; (void)argc; (void)argv;
	if (doc == NULL) return JS_NULL;
	dom_document_create_document_fragment(doc, &n);
	return eon_wrap_take(ctx, (dom_node *)n);
}

/* ------------------------------------------------------------------ __ns: atributos */

static JSValue ns_get_attr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_string *k, *v = NULL;
	dom_node_type type;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	dom_node_get_node_type(n, &type);
	if (type != DOM_ELEMENT_NODE) return JS_NULL;
	k = eon_js_to_ds(ctx, argv[1]);
	if (k == NULL) return JS_EXCEPTION;
	dom_element_get_attribute((dom_element *)n, k, &v);
	dom_string_unref(k);
	return eon_ds_take(ctx, v);
}

static JSValue ns_set_attr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_string *k, *v;
	dom_exception exc;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	k = eon_js_to_ds(ctx, argv[1]);
	v = eon_js_to_ds(ctx, argv[2]);
	exc = (k && v) ? dom_element_set_attribute((dom_element *)n, k, v) : DOM_NO_ERR;
	if (k) dom_string_unref(k);
	if (v) dom_string_unref(v);
	return exc == DOM_NO_ERR ? JS_UNDEFINED : eon_dom_error(ctx, exc);
}

static JSValue ns_remove_attr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_string *k;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	k = eon_js_to_ds(ctx, argv[1]);
	if (k) {
		dom_element_remove_attribute((dom_element *)n, k);
		dom_string_unref(k);
	}
	return JS_UNDEFINED;
}

static JSValue ns_attr_names(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue arr = JS_NewArray(ctx);
	dom_namednodemap *map = NULL;
	dom_ulong len = 0, i;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	if (dom_node_get_attributes(n, &map) != DOM_NO_ERR || map == NULL) return arr;
	dom_namednodemap_get_length(map, &len);
	for (i = 0; i < len; i++) {
		dom_node *a = NULL;
		dom_string *name = NULL;
		dom_namednodemap_item(map, i, &a);
		if (a == NULL) continue;
		dom_node_get_node_name(a, &name);
		JS_SetPropertyUint32(ctx, arr, i, eon_ds_take(ctx, name));
		dom_node_unref(a);
	}
	dom_namednodemap_unref(map);
	return arr;
}

/* ------------------------------------------------------------------ __ns: busqueda */

static JSValue ns_by_id(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_document *doc = eon_doc(ctx);
	dom_string *id;
	dom_element *el = NULL;
	(void)this_val; (void)argc;
	if (doc == NULL) return JS_NULL;
	id = eon_js_to_ds(ctx, argv[0]);
	if (id == NULL) return JS_EXCEPTION;
	dom_document_get_element_by_id(doc, id, &el);
	dom_string_unref(id);
	return eon_wrap_take(ctx, (dom_node *)el);
}

static JSValue ns_by_tag(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue arr = JS_NewArray(ctx);
	dom_nodelist *list = NULL;
	dom_node_type type;
	dom_string *tag;
	uint32_t len = 0, i;
	NODE_ARG(0, root);
	(void)this_val; (void)argc;
	tag = eon_js_to_ds(ctx, argv[1]);
	if (tag == NULL) return arr;
	dom_node_get_node_type(root, &type);
	if (type == DOM_DOCUMENT_NODE) {
		dom_document_get_elements_by_tag_name((dom_document *)root, tag, &list);
	} else if (type == DOM_ELEMENT_NODE) {
		dom_element_get_elements_by_tag_name((dom_element *)root, tag, &list);
	}
	dom_string_unref(tag);
	if (list == NULL) return arr;
	dom_nodelist_get_length(list, &len);
	for (i = 0; i < len; i++) {
		dom_node *c = NULL;
		dom_nodelist_item(list, i, &c);
		JS_SetPropertyUint32(ctx, arr, i, eon_wrap_take(ctx, c));
	}
	dom_nodelist_unref(list);
	return arr;
}

/* ------------------------------------------------------------------ __ns: innerHTML */

static JSValue ns_set_inner_html(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	size_t size;
	const char *s;
	dom_hubbub_parser_params params;
	dom_hubbub_parser *parser = NULL;
	dom_document *doc = NULL;
	dom_document_fragment *fragment = NULL;
	dom_node *child = NULL, *html = NULL, *body = NULL;
	dom_nodelist *bodies = NULL;
	uint32_t blen = 0;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;

	s = JS_ToCStringLen(ctx, &size, argv[1]);
	if (s == NULL) return JS_EXCEPTION;
	if (dom_node_get_owner_document(n, &doc) != DOM_NO_ERR || doc == NULL) goto out;

	memset(&params, 0, sizeof(params));
	params.enc = "UTF-8";
	params.fix_enc = true;
	params.enable_script = false;
	if (dom_hubbub_fragment_parser_create(&params, doc, &parser, &fragment) != DOM_HUBBUB_OK) goto out;
	if (dom_hubbub_parser_parse_chunk(parser, (const uint8_t *)s, size) != DOM_HUBBUB_OK) goto out;
	if (dom_hubbub_parser_completed(parser) != DOM_HUBBUB_OK) goto out;

	/* vaciar el nodo */
	while (dom_node_get_first_child(n, &child) == DOM_NO_ERR && child != NULL) {
		dom_node *r = NULL;
		dom_node_remove_child(n, child, &r);
		dom_node_unref(child);
		if (r) dom_node_unref(r);
		child = NULL;
	}

	/* hubbub arma <html><head/><body>...</body></html>: mover los hijos del body */
	if (dom_node_get_first_child(fragment, &html) != DOM_NO_ERR || html == NULL) goto out;
	if (dom_element_get_elements_by_tag_name((dom_element *)html, corestring_dom_BODY, &bodies) != DOM_NO_ERR || bodies == NULL) goto out;
	dom_nodelist_get_length(bodies, &blen);
	if (blen == 0) goto out;
	dom_nodelist_item(bodies, 0, &body);
	if (body == NULL) goto out;
	while (dom_node_get_first_child(body, &child) == DOM_NO_ERR && child != NULL) {
		dom_node *r = NULL, *r2 = NULL;
		dom_node_remove_child(body, child, &r);
		if (r) dom_node_unref(r);
		dom_node_append_child(n, child, &r2);
		if (r2) dom_node_unref(r2);
		dom_node_unref(child);
		child = NULL;
	}

out:
	if (body) dom_node_unref(body);
	if (bodies) dom_nodelist_unref(bodies);
	if (html) dom_node_unref(html);
	if (fragment) dom_node_unref((dom_node *)fragment);
	if (parser) dom_hubbub_parser_destroy(parser);
	if (doc) dom_node_unref((dom_node *)doc);
	JS_FreeCString(ctx, s);
	return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ __ns: formularios */

/* formGet(node, prop): prop = "value" | "checked" | "selected" | "selectedIndex" */
static JSValue ns_form_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_html_element_type tt;
	const char *prop;
	JSValue r = JS_UNDEFINED;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	if (dom_html_element_get_tag_type(n, &tt) != DOM_NO_ERR) return JS_UNDEFINED;
	prop = JS_ToCString(ctx, argv[1]);
	if (prop == NULL) return JS_EXCEPTION;

	if (tt == DOM_HTML_ELEMENT_TYPE_INPUT) {
		if (strcmp(prop, "value") == 0) {
			dom_string *v = NULL;
			dom_html_input_element_get_value((dom_html_input_element *)n, &v);
			r = v ? eon_ds_take(ctx, v) : JS_NewString(ctx, "");
		} else if (strcmp(prop, "checked") == 0) {
			bool b = false;
			dom_html_input_element_get_checked((dom_html_input_element *)n, &b);
			r = JS_NewBool(ctx, b);
		}
	} else if (tt == DOM_HTML_ELEMENT_TYPE_TEXTAREA) {
		if (strcmp(prop, "value") == 0) {
			dom_string *v = NULL;
			dom_html_text_area_element_get_value((dom_html_text_area_element *)n, &v);
			r = v ? eon_ds_take(ctx, v) : JS_NewString(ctx, "");
		}
	} else if (tt == DOM_HTML_ELEMENT_TYPE_SELECT) {
		if (strcmp(prop, "value") == 0) {
			dom_string *v = NULL;
			dom_html_select_element_get_value((dom_html_select_element *)n, &v);
			r = v ? eon_ds_take(ctx, v) : JS_NewString(ctx, "");
		} else if (strcmp(prop, "selectedIndex") == 0) {
			int32_t i = -1;
			dom_html_select_element_get_selected_index((dom_html_select_element *)n, &i);
			r = JS_NewInt32(ctx, i);
		}
	} else if (tt == DOM_HTML_ELEMENT_TYPE_OPTION) {
		if (strcmp(prop, "selected") == 0) {
			bool b = false;
			dom_html_option_element_get_selected((dom_html_option_element *)n, &b);
			r = JS_NewBool(ctx, b);
		}
	}
	JS_FreeCString(ctx, prop);
	return r;
}

static JSValue ns_form_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	dom_html_element_type tt;
	const char *prop;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	if (dom_html_element_get_tag_type(n, &tt) != DOM_NO_ERR) return JS_UNDEFINED;
	prop = JS_ToCString(ctx, argv[1]);
	if (prop == NULL) return JS_EXCEPTION;

	if (strcmp(prop, "value") == 0) {
		dom_string *v = eon_js_to_ds(ctx, argv[2]);
		if (v != NULL) {
			if (tt == DOM_HTML_ELEMENT_TYPE_INPUT)
				dom_html_input_element_set_value((dom_html_input_element *)n, v);
			else if (tt == DOM_HTML_ELEMENT_TYPE_TEXTAREA)
				dom_html_text_area_element_set_value((dom_html_text_area_element *)n, v);
			else if (tt == DOM_HTML_ELEMENT_TYPE_SELECT)
				dom_html_select_element_set_value((dom_html_select_element *)n, v);
			dom_string_unref(v);
		}
	} else if (strcmp(prop, "checked") == 0 && tt == DOM_HTML_ELEMENT_TYPE_INPUT) {
		dom_html_input_element_set_checked((dom_html_input_element *)n, JS_ToBool(ctx, argv[2]));
	}
	JS_FreeCString(ctx, prop);
	return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ __ns: geometria */

/* rect(node) -> [x, y, ancho, alto] en pixeles de la pagina, o null si no se ve */
static JSValue ns_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct box *box = NULL;
	JSValue arr;
	int x = 0, y = 0;
	NODE_ARG(0, n);
	(void)this_val; (void)argc;
	if (dom_node_get_user_data(n, corestring_dom___ns_key_box_node_data, (void *)&box) != DOM_NO_ERR || box == NULL) {
		return JS_NULL;
	}
	box_coords(box, &x, &y);
	arr = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt32(ctx, x));
	JS_SetPropertyUint32(ctx, arr, 1, JS_NewInt32(ctx, y));
	JS_SetPropertyUint32(ctx, arr, 2, JS_NewInt32(ctx, box->padding[LEFT] + box->width + box->padding[RIGHT]));
	JS_SetPropertyUint32(ctx, arr, 3, JS_NewInt32(ctx, box->padding[TOP] + box->height + box->padding[BOTTOM]));
	return arr;
}

/* ------------------------------------------------------------------ __ns: ventana */

static JSValue ns_href(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct jsthread *t = eon_thread(ctx);
	nsurl *u = NULL;
	(void)this_val; (void)argc; (void)argv;
	if (t->htmlc != NULL) u = t->htmlc->base_url;
	if (u == NULL && t->bw != NULL) u = browser_window_access_url(t->bw);
	if (u == NULL) return JS_NewString(ctx, "about:blank");
	return JS_NewStringLen(ctx, nsurl_access(u), nsurl_length(u));
}

static JSValue ns_navigate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct jsthread *t = eon_thread(ctx);
	const char *s;
	nsurl *u = NULL, *base;
	(void)this_val; (void)argc;
	if (t->bw == NULL) return JS_UNDEFINED;
	s = JS_ToCString(ctx, argv[0]);
	if (s == NULL) return JS_EXCEPTION;
	base = (t->htmlc != NULL) ? t->htmlc->base_url : NULL;
	if ((base != NULL ? nsurl_join(base, s, &u) : nsurl_create(s, &u)) == NSERROR_OK) {
		browser_window_navigate(t->bw, u, base, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
		nsurl_unref(u);
	}
	JS_FreeCString(ctx, s);
	return JS_UNDEFINED;
}

static JSValue ns_history(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct jsthread *t = eon_thread(ctx);
	int32_t d = 0;
	(void)this_val; (void)argc;
	JS_ToInt32(ctx, &d, argv[0]);
	if (t->bw == NULL) return JS_UNDEFINED;
	if (d < 0) browser_window_history_back(t->bw, false);
	else if (d > 0) browser_window_history_forward(t->bw, false);
	else browser_window_reload(t->bw, false);
	return JS_UNDEFINED;
}

static JSValue ns_cookie(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct jsthread *t = eon_thread(ctx);
	nsurl *u = (t->htmlc != NULL) ? t->htmlc->base_url : NULL;
	(void)this_val;
	if (u == NULL) return JS_NewString(ctx, "");
	if (argc > 0 && !JS_IsUndefined(argv[0])) {
		const char *s = JS_ToCString(ctx, argv[0]);
		if (s != NULL) {
			urldb_set_cookie(s, u, NULL);
			JS_FreeCString(ctx, s);
		}
		return JS_UNDEFINED;
	} else {
		char *c = urldb_get_cookie(u, false);
		JSValue r = JS_NewString(ctx, c != NULL ? c : "");
		free(c);
		return r;
	}
}

static JSValue ns_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *s;
	(void)this_val;
	if (argc < 1) return JS_UNDEFINED;
	s = JS_ToCString(ctx, argv[0]);
	if (s != NULL) {
		NSLOG(netsurf, INFO, "console: %s", s);
		JS_FreeCString(ctx, s);
	}
	return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ __ns: temporizadores */

static void eon_timer_fire(void *p)
{
	eon_timer *tm = p, **pp;
	struct jsthread *t = tm->thread;
	JSValue arg;

	for (pp = &t->timers; *pp != NULL; pp = &(*pp)->next) {
		if (*pp == tm) {
			*pp = tm->next;
			break;
		}
	}
	if (eon_enter(t)) {
		arg = JS_NewInt32(t->ctx, tm->id);
		JS_FreeValue(t->ctx, eon_call_ns(t, "onTimer", 1, &arg));
		eon_leave(t);
	}
	free(tm);
}

/* setTimer(id, ms): dom.js guarda la funcion y repite si es setInterval */
static JSValue ns_set_timer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct jsthread *t = eon_thread(ctx);
	eon_timer *tm;
	int32_t id = 0, ms = 0;
	(void)this_val; (void)argc;
	JS_ToInt32(ctx, &id, argv[0]);
	JS_ToInt32(ctx, &ms, argv[1]);
	if (t->closed) return JS_UNDEFINED;
	if (ms < 4) ms = 4;
	tm = calloc(1, sizeof(*tm));
	if (tm == NULL) return JS_ThrowOutOfMemory(ctx);
	tm->thread = t;
	tm->id = id;
	tm->next = t->timers;
	t->timers = tm;
	guit->misc->schedule(ms, eon_timer_fire, tm);
	return JS_UNDEFINED;
}

static JSValue ns_clear_timer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct jsthread *t = eon_thread(ctx);
	eon_timer **pp;
	int32_t id = 0;
	(void)this_val; (void)argc;
	JS_ToInt32(ctx, &id, argv[0]);
	for (pp = &t->timers; *pp != NULL; pp = &(*pp)->next) {
		if ((*pp)->id == id) {
			eon_timer *tm = *pp;
			*pp = tm->next;
			guit->misc->schedule(-1, eon_timer_fire, tm);
			free(tm);
			break;
		}
	}
	return JS_UNDEFINED;
}

static void eon_cancel_timers(struct jsthread *t)
{
	while (t->timers != NULL) {
		eon_timer *tm = t->timers;
		t->timers = tm->next;
		guit->misc->schedule(-1, eon_timer_fire, tm);
		free(tm);
	}
}

/* ------------------------------------------------------------------ __ns: eventos */

/* Un unico escuchador de libdom en el documento, en fase de captura, para cada
 * tipo de evento que el JS pidio. Recibe todo lo que NetSurf despacha (clic,
 * submit...) y lo pasa a dom.js, que hace la propagacion estandar. */
static void eon_dom_event(struct dom_event *evt, void *pw)
{
	struct jsthread *t = pw;
	dom_event_target *target = NULL;
	dom_string *type = NULL;
	JSValue args[2], r;

	if (!eon_enter(t)) return;
	dom_event_get_target(evt, &target);
	dom_event_get_type(evt, &type);
	args[0] = eon_wrap(t->ctx, (dom_node *)target);
	args[1] = eon_ds_to_js(t->ctx, type);
	r = eon_call_ns(t, "onEvent", 2, args);
	if (JS_ToBool(t->ctx, r)) {
		dom_event_prevent_default(evt);
	}
	JS_FreeValue(t->ctx, r);
	JS_FreeValue(t->ctx, args[0]);
	JS_FreeValue(t->ctx, args[1]);
	if (type) dom_string_unref(type);
	if (target) dom_node_unref((dom_node *)target);
	eon_leave(t);
}

static JSValue ns_listen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct jsthread *t = eon_thread(ctx);
	dom_document *doc = eon_doc(ctx);
	dom_string *type;
	unsigned int i;
	(void)this_val; (void)argc;
	if (doc == NULL || t->n_listening >= EON_MAX_LISTEN) return JS_UNDEFINED;
	type = eon_js_to_ds(ctx, argv[0]);
	if (type == NULL) return JS_EXCEPTION;
	for (i = 0; i < t->n_listening; i++) {
		if (dom_string_isequal(t->listening[i], type)) {
			dom_string_unref(type);
			return JS_UNDEFINED;
		}
	}
	if (t->listener == NULL &&
	    dom_event_listener_create(eon_dom_event, t, &t->listener) != DOM_NO_ERR) {
		dom_string_unref(type);
		return JS_UNDEFINED;
	}
	dom_event_target_add_event_listener(doc, type, t->listener, true);
	t->listening[t->n_listening++] = type;
	return JS_UNDEFINED;
}

static void eon_unlisten(struct jsthread *t)
{
	dom_document *doc = (t->htmlc != NULL) ? t->htmlc->document : NULL;
	unsigned int i;
	for (i = 0; i < t->n_listening; i++) {
		if (doc != NULL && t->listener != NULL) {
			dom_event_target_remove_event_listener(doc, t->listening[i], t->listener, true);
		}
		dom_string_unref(t->listening[i]);
	}
	t->n_listening = 0;
	if (t->listener != NULL) {
		dom_event_listener_unref(t->listener);
		t->listener = NULL;
	}
}

/* ------------------------------------------------------------------ __ns: red */

static void eon_release_handle(void *p)
{
	llcache_handle_release(p);
}

static void eon_req_unlink(eon_req *r)
{
	eon_req **p;
	for (p = &eon_reqs; *p != NULL; p = &(*p)->next) {
		if (*p == r) {
			*p = r->next;
			return;
		}
	}
}

static void eon_http_done(struct jsthread *t, int32_t id, int status,
			  const uint8_t *data, size_t len, const char *type)
{
	JSValue args[4];
	args[0] = JS_NewInt32(t->ctx, id);
	args[1] = JS_NewInt32(t->ctx, status);
	args[2] = JS_NewStringLen(t->ctx, data != NULL ? (const char *)data : "", data != NULL ? len : 0);
	args[3] = JS_NewString(t->ctx, type != NULL ? type : "");
	JS_FreeValue(t->ctx, eon_call_ns(t, "onHttp", 4, args));
	JS_FreeValue(t->ctx, args[2]);
	JS_FreeValue(t->ctx, args[3]);
}

/* despues de terminar, la cache todavia puede avisar (p. ej. al ponerse al dia):
 * esos avisos van aca y no tocan el pedido ya liberado */
static nserror eon_http_ignore(llcache_handle *h, const llcache_event *ev, void *pw)
{
	(void)h; (void)ev; (void)pw;
	return NSERROR_OK;
}

static nserror eon_http_cb(llcache_handle *h, const llcache_event *ev, void *pw)
{
	eon_req *r = pw;
	struct jsthread *t = r->thread;
	const uint8_t *data = NULL;
	size_t len = 0;
	int status;

	if (ev->type == LLCACHE_EVENT_HAD_HEADERS) {
		r->http_code = eon_llcache_http_code(h);
		return NSERROR_OK;
	} else if (ev->type == LLCACHE_EVENT_DONE) {
		data = llcache_handle_get_source_data(h, &len);
		status = r->http_code > 0 ? (int)r->http_code : 200;
	} else if (ev->type == LLCACHE_EVENT_ERROR) {
		status = 0;
	} else {
		return NSERROR_OK;
	}
	eon_req_unlink(r);
	llcache_handle_change_callback(h, eon_http_ignore, NULL);
	if (eon_enter(t)) {
		eon_http_done(t, r->id, status, data, len,
			      status ? llcache_handle_get_header(h, "Content-Type") : NULL);
		eon_leave(t);
	}
	guit->misc->schedule(0, eon_release_handle, h);
	free(r->post);
	free(r);
	return NSERROR_OK;
}

/* http(id, method, url, body): url relativa a la pagina; responde por __ns.onHttp */
static JSValue ns_http(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct jsthread *t = eon_thread(ctx);
	llcache_post_data post;
	const char *method, *url;
	nsurl *u = NULL, *base;
	eon_req *r;
	int32_t id = 0;
	nserror err;
	(void)this_val; (void)argc;

	JS_ToInt32(ctx, &id, argv[0]);
	method = JS_ToCString(ctx, argv[1]);
	url = JS_ToCString(ctx, argv[2]);
	base = (t->htmlc != NULL) ? t->htmlc->base_url : NULL;
	err = (method && url) ? (base ? nsurl_join(base, url, &u) : nsurl_create(url, &u)) : NSERROR_BAD_PARAMETER;
	if (err == NSERROR_OK && (r = calloc(1, sizeof(*r))) != NULL) {
		r->thread = t;
		r->id = id;
		if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
			const char *b = JS_IsNull(argv[3]) || JS_IsUndefined(argv[3]) ? NULL : JS_ToCString(ctx, argv[3]);
			r->post = strdup(b != NULL ? b : "");
			if (b) JS_FreeCString(ctx, b);
			post.type = LLCACHE_POST_URL_ENCODED;
			post.data.urlenc = r->post;
		}
		r->next = eon_reqs;
		eon_reqs = r;
		err = llcache_handle_retrieve(u, LLCACHE_RETRIEVE_NO_ERROR_PAGES, base,
					      r->post != NULL ? &post : NULL,
					      eon_http_cb, r, &r->handle);
		if (err != NSERROR_OK) {
			eon_req_unlink(r);
			free(r->post);
			free(r);
		}
	}
	if (u) nsurl_unref(u);
	if (method) JS_FreeCString(ctx, method);
	if (url) JS_FreeCString(ctx, url);
	if (err != NSERROR_OK) {
		eon_http_done(t, id, 0, NULL, 0, NULL);
	}
	return JS_UNDEFINED;
}

static void eon_abort_requests(struct jsthread *t)
{
	eon_req **p = &eon_reqs;
	while (*p != NULL) {
		eon_req *r = *p;
		if (r->thread == t) {
			*p = r->next;
			llcache_handle_abort(r->handle);
			llcache_handle_release(r->handle);
			free(r->post);
			free(r);
		} else {
			p = &r->next;
		}
	}
}

/* ------------------------------------------------------------------ __ns: almacenamiento */

static void eon_storage_path(const char *origin, char *buf, size_t n)
{
	size_t j = (size_t)snprintf(buf, n, "%s/", EONSURF_STORAGE_DIR);
	for (; *origin != '\0' && j + 6 < n; origin++) {
		unsigned char c = (unsigned char)*origin;
		buf[j++] = (isalnum(c) || c == '.' || c == '-') ? (char)c : '_';
	}
	snprintf(buf + j, n - j, ".json");
}

/* storage(origin) -> texto | undefined ; storage(origin, texto) -> guarda */
static JSValue ns_storage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	char path[512];
	const char *origin;
	FILE *f;
	long size;
	char *buf;
	JSValue r = JS_UNDEFINED;
	(void)this_val;

	origin = JS_ToCString(ctx, argv[0]);
	if (origin == NULL) return JS_EXCEPTION;
	eon_storage_path(origin, path, sizeof(path));
	JS_FreeCString(ctx, origin);

	if (argc >= 2 && !JS_IsUndefined(argv[1])) {
		size_t len;
		const char *data = JS_ToCStringLen(ctx, &len, argv[1]);
		if (data != NULL && len <= EON_STORAGE_MAX) {
			mkdir(EONSURF_STORAGE_DIR, 0777);
			f = fopen(path, "wb");
			if (f != NULL) {
				fwrite(data, 1, len, f);
				fclose(f);
			}
		}
		if (data) JS_FreeCString(ctx, data);
		return JS_UNDEFINED;
	}

	f = fopen(path, "rb");
	if (f == NULL) return JS_UNDEFINED;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size > 0 && size <= EON_STORAGE_MAX && (buf = malloc((size_t)size)) != NULL) {
		size = (long)fread(buf, 1, (size_t)size, f);
		r = JS_NewStringLen(ctx, buf, (size_t)size);
		free(buf);
	}
	fclose(f);
	return r;
}

/* ------------------------------------------------------------------ modulos JS */

/* Los import se resuelven contra la URL del modulo que los pide y se bajan con
 * libcurl en el momento (bloquea un instante, pero el cargador de QuickJS es
 * sincronico). Los nombres "pelados" ("react") no se soportan sin import map. */

typedef struct {
	char *buf;
	size_t len;
	size_t cap;
} eon_buf;

static size_t eon_curl_write(void *p, size_t sz, size_t n, void *ud)
{
	eon_buf *b = ud;
	size_t add = sz * n;
	if (b->len + add + 1 > b->cap) {
		size_t cap = (b->cap ? b->cap * 2 : 65536);
		char *nb;
		while (cap < b->len + add + 1) cap *= 2;
		nb = realloc(b->buf, cap);
		if (nb == NULL) return 0;
		b->buf = nb;
		b->cap = cap;
	}
	memcpy(b->buf + b->len, p, add);
	b->len += add;
	b->buf[b->len] = '\0';
	return add;
}

/* baja url; devuelve texto terminado en 0 (liberar con free) o NULL */
static char *eon_fetch_sync(const char *url, size_t *len)
{
	/* una sola conexion reutilizada: evita un saludo TLS nuevo por cada import
	 * (en la PS3 cada uno cuesta segundos) */
	static CURL *c = NULL;
	eon_buf b = { NULL, 0, 0 };
	long code = 0;
	CURLcode rc;
	nsurl *u = NULL;
	char *cookie = NULL;

	if (c == NULL) {
		c = curl_easy_init();
		if (c == NULL) return NULL;
	} else {
		curl_easy_setopt(c, CURLOPT_COOKIE, NULL);
	}
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, eon_curl_write);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
	if (nsoption_charp(ca_bundle) != NULL) {
		curl_easy_setopt(c, CURLOPT_CAINFO, nsoption_charp(ca_bundle));
	}
	if (nsurl_create(url, &u) == NSERROR_OK) {
		cookie = urldb_get_cookie(u, true);
		if (cookie != NULL) curl_easy_setopt(c, CURLOPT_COOKIE, cookie);
		nsurl_unref(u);
	}
	rc = curl_easy_perform(c);
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
	free(cookie);
	if (rc != CURLE_OK || code >= 400 || b.buf == NULL) {
		NSLOG(netsurf, WARNING, "EonSurf: module fetch failed %s (curl %d, http %ld)", url, (int)rc, code);
		free(b.buf);
		return NULL;
	}
	*len = b.len;
	return b.buf;
}

/* import "react" -> URL segun la import map de la pagina (__ns.importMap) */
static char *eon_importmap_lookup(JSContext *ctx, const char *name)
{
	struct jsthread *t = eon_thread(ctx);
	JSValue f, r, arg;
	char *out = NULL;
	f = JS_GetPropertyStr(ctx, t->ns, "resolveImport");
	if (JS_IsFunction(ctx, f)) {
		arg = JS_NewString(ctx, name);
		r = JS_Call(ctx, f, t->ns, 1, &arg);
		JS_FreeValue(ctx, arg);
		if (JS_IsString(r)) {
			const char *s = JS_ToCString(ctx, r);
			if (s) {
				out = js_strdup(ctx, s);
				JS_FreeCString(ctx, s);
			}
		}
		JS_FreeValue(ctx, r);
	}
	JS_FreeValue(ctx, f);
	return out;
}

static char *eon_mod_normalize(JSContext *ctx, const char *base, const char *name, void *opaque)
{
	nsurl *b = NULL, *u = NULL;
	char *r = NULL;
	(void)opaque;
	/* nombres "pelados" (sin ./ ../ / ni esquema): import map */
	if (name[0] != '.' && name[0] != '/' && strstr(name, "://") == NULL &&
	    strncmp(name, "data:", 5) != 0 && strncmp(name, "blob:", 5) != 0) {
		r = eon_importmap_lookup(ctx, name);
		if (r != NULL) {
			return r;
		}
	}
	if (nsurl_create(base, &b) == NSERROR_OK) {
		if (nsurl_join(b, name, &u) == NSERROR_OK) {
			r = js_strdup(ctx, nsurl_access(u));
			nsurl_unref(u);
		}
		nsurl_unref(b);
	}
	if (r == NULL) {
		r = js_strdup(ctx, name);
	}
	return r;
}

static void eon_mod_set_meta(JSContext *ctx, JSModuleDef *m, const char *url)
{
	JSValue meta = JS_GetImportMeta(ctx, m);
	if (!JS_IsException(meta)) {
		JS_DefinePropertyValueStr(ctx, meta, "url", JS_NewString(ctx, url), JS_PROP_C_W_E);
		JS_FreeValue(ctx, meta);
	}
}

static JSModuleDef *eon_mod_loader(JSContext *ctx, const char *name, void *opaque)
{
	size_t len = 0;
	char *src;
	JSValue fn;
	JSModuleDef *m;
	(void)opaque;

	NSLOG(netsurf, INFO, "EonSurf: import %s", name);
	src = eon_fetch_sync(name, &len);
	if (src == NULL) {
		JS_ThrowReferenceError(ctx, "could not load module '%s'", name);
		return NULL;
	}
	fn = JS_Eval(ctx, src, len, name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
	free(src);
	if (JS_IsException(fn)) {
		return NULL;
	}
	m = JS_VALUE_GET_PTR(fn);
	eon_mod_set_meta(ctx, m, name);
	JS_FreeValue(ctx, fn);
	return m;
}

/* evaluar un <script type="module">; name = URL del script o de la pagina */
static JSValue eon_eval_module(struct jsthread *t, const char *buf, size_t len, const char *name)
{
	JSValue fn = JS_Eval(t->ctx, buf, len, name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
	if (JS_IsException(fn)) {
		return fn;
	}
	eon_mod_set_meta(t->ctx, JS_VALUE_GET_PTR(fn), name);
	return JS_EvalFunction(t->ctx, fn);
}

/* ------------------------------------------------------------------ tabla __ns */

static const JSCFunctionListEntry eon_ns_funcs[] = {
	JS_CFUNC_DEF("document", 0, ns_document),
	JS_CFUNC_DEF("type", 1, ns_type),
	JS_CFUNC_DEF("name", 1, ns_name),
	JS_CFUNC_DEF("value", 1, ns_value),
	JS_CFUNC_DEF("setValue", 2, ns_set_value),
	JS_CFUNC_DEF("text", 1, ns_text),
	JS_CFUNC_DEF("setText", 2, ns_set_text),
	JS_CFUNC_DEF("parent", 1, ns_parent),
	JS_CFUNC_DEF("first", 1, ns_first),
	JS_CFUNC_DEF("last", 1, ns_last),
	JS_CFUNC_DEF("next", 1, ns_next),
	JS_CFUNC_DEF("prev", 1, ns_prev),
	JS_CFUNC_DEF("children", 1, ns_children),
	JS_CFUNC_DEF("append", 2, ns_append),
	JS_CFUNC_DEF("insert", 3, ns_insert),
	JS_CFUNC_DEF("remove", 2, ns_remove),
	JS_CFUNC_DEF("replace", 3, ns_replace),
	JS_CFUNC_DEF("clone", 2, ns_clone),
	JS_CFUNC_DEF("createElement", 1, ns_create_element),
	JS_CFUNC_DEF("createText", 1, ns_create_text),
	JS_CFUNC_DEF("createComment", 1, ns_create_comment),
	JS_CFUNC_DEF("createFragment", 0, ns_create_fragment),
	JS_CFUNC_DEF("getAttr", 2, ns_get_attr),
	JS_CFUNC_DEF("setAttr", 3, ns_set_attr),
	JS_CFUNC_DEF("removeAttr", 2, ns_remove_attr),
	JS_CFUNC_DEF("attrNames", 1, ns_attr_names),
	JS_CFUNC_DEF("byId", 1, ns_by_id),
	JS_CFUNC_DEF("byTag", 2, ns_by_tag),
	JS_CFUNC_DEF("setInnerHTML", 2, ns_set_inner_html),
	JS_CFUNC_DEF("formGet", 2, ns_form_get),
	JS_CFUNC_DEF("formSet", 3, ns_form_set),
	JS_CFUNC_DEF("rect", 1, ns_rect),
	JS_CFUNC_DEF("href", 0, ns_href),
	JS_CFUNC_DEF("navigate", 1, ns_navigate),
	JS_CFUNC_DEF("history", 1, ns_history),
	JS_CFUNC_DEF("cookie", 1, ns_cookie),
	JS_CFUNC_DEF("log", 1, ns_log),
	JS_CFUNC_DEF("setTimer", 2, ns_set_timer),
	JS_CFUNC_DEF("clearTimer", 1, ns_clear_timer),
	JS_CFUNC_DEF("listen", 1, ns_listen),
	JS_CFUNC_DEF("http", 4, ns_http),
	JS_CFUNC_DEF("storage", 2, ns_storage),
};

/* ------------------------------------------------------------------ js.h */

/* newlib de PSL1GHT declara clock_gettime pero no la trae */
#ifdef __PPU__
#include <sys/systime.h>
int clock_gettime(clockid_t id, struct timespec *tp);
int clock_gettime(clockid_t id, struct timespec *tp)
{
	u64 us = sysGetSystemTime();
	(void)id;
	tp->tv_sec = (time_t)(us / 1000000);
	tp->tv_nsec = (long)(us % 1000000) * 1000;
	return 0;
}
#endif

void js_initialise(void)
{
	if (eon_key == NULL) {
		dom_string_create((const uint8_t *)"__eon_qjs", 9, &eon_key);
	}
	/* registra text/javascript para que NetSurf nos pase los scripts */
	javascript_init();
	NSLOG(netsurf, INFO, "EonSurf: QuickJS-ng %s", JS_GetVersion());
}

void js_finalise(void)
{
	if (eon_key != NULL) {
		dom_string_unref(eon_key);
		eon_key = NULL;
	}
}

nserror js_newheap(int timeout, jsheap **heap)
{
	jsheap *h = calloc(1, sizeof(*h));
	(void)timeout;
	if (h == NULL) return NSERROR_NOMEM;
	h->rt = JS_NewRuntime();
	if (h->rt == NULL) {
		free(h);
		return NSERROR_NOMEM;
	}
	JS_SetMemoryLimit(h->rt, 96 * 1024 * 1024);
	JS_SetMaxStackSize(h->rt, 512 * 1024);
	JS_SetInterruptHandler(h->rt, eon_interrupt, h);
	JS_SetModuleLoaderFunc(h->rt, eon_mod_normalize, eon_mod_loader, h);
	JS_SetHostPromiseRejectionTracker(h->rt, eon_rejection, NULL);
	if (eon_node_class_id == 0) {
		JS_NewClassID(h->rt, &eon_node_class_id);
	}
	JS_NewClass(h->rt, eon_node_class_id, &eon_node_class);
	*heap = h;
	return NSERROR_OK;
}

static void eon_destroy_heap_now(jsheap *h)
{
	JS_FreeRuntime(h->rt);
	free(h);
}

void js_destroyheap(jsheap *heap)
{
	heap->pending_destroy = true;
	if (heap->live_threads == 0) {
		eon_destroy_heap_now(heap);
	}
}

nserror js_newthread(jsheap *heap, void *win_priv, void *doc_priv, jsthread **thread)
{
	struct jsthread *t = calloc(1, sizeof(*t));
	JSValue global, r;

	if (t == NULL) return NSERROR_NOMEM;
	t->heap = heap;
	t->bw = win_priv;
	t->htmlc = doc_priv;
	t->ctx = JS_NewContext(heap->rt);
	if (t->ctx == NULL) {
		free(t);
		return NSERROR_NOMEM;
	}
	JS_SetContextOpaque(t->ctx, t);
	heap->live_threads++;

	t->ns = JS_NewObject(t->ctx);
	JS_SetPropertyFunctionList(t->ctx, t->ns, eon_ns_funcs,
				   sizeof(eon_ns_funcs) / sizeof(eon_ns_funcs[0]));
	t->keep = JS_NewArray(t->ctx);
	JS_SetPropertyStr(t->ctx, t->ns, "keep", JS_DupValue(t->ctx, t->keep));

	global = JS_GetGlobalObject(t->ctx);
	JS_DefinePropertyValueStr(t->ctx, global, "__ns", JS_DupValue(t->ctx, t->ns),
				  JS_PROP_CONFIGURABLE); /* no enumerable */
	JS_FreeValue(t->ctx, global);

	/* dom.js arma window, document y toda la API web sobre __ns */
	heap->exec_start = eon_now_ms();
	r = JS_Eval(t->ctx, (const char *)eon_dom_js, eon_dom_js_len - 1, "dom.js",
		    JS_EVAL_TYPE_GLOBAL);
	heap->exec_start = 0;
	if (JS_IsException(r)) {
		eon_log_exception(t->ctx, "dom.js");
	}
	JS_FreeValue(t->ctx, r);
	eon_run_jobs(t);

	NSLOG(netsurf, INFO, "EonSurf: QuickJS thread %p (bw %p, html %p)", t, win_priv, doc_priv);
	*thread = t;
	return NSERROR_OK;
}

nserror js_closethread(jsthread *thread)
{
	struct jsthread *t = thread;
	if (t->closed) return NSERROR_OK;
	if (t->in_use == 0 && !t->pending_destroy) {
		t->in_use++;
		JS_FreeValue(t->ctx, eon_call_ns(t, "onClose", 0, NULL));
		eon_run_jobs(t);
		t->in_use--;
	}
	t->closed = true;
	eon_cancel_timers(t);
	eon_abort_requests(t);
	eon_unlisten(t);
	return NSERROR_OK;
}

static void eon_destroy_thread_now(struct jsthread *t)
{
	jsheap *heap = t->heap;
	t->closed = true;
	eon_cancel_timers(t);
	eon_abort_requests(t);
	eon_unlisten(t);
	JS_FreeValue(t->ctx, t->keep);
	JS_FreeValue(t->ctx, t->ns);
	JS_FreeContext(t->ctx);
	free(t);
	heap->live_threads--;
	if (heap->pending_destroy && heap->live_threads == 0) {
		eon_destroy_heap_now(heap);
	}
}

void js_destroythread(jsthread *thread)
{
	struct jsthread *t = thread;
	t->pending_destroy = true;
	if (t->in_use == 0) {
		eon_destroy_thread_now(t);
	}
}

bool js_exec(jsthread *thread, const uint8_t *txt, size_t txtlen, const char *name)
{
	struct jsthread *t = thread;
	JSValue r;
	bool ok;
	char *buf;

	if (txt == NULL || txtlen == 0 || !eon_enter(t)) {
		return false;
	}
	/* JS_Eval necesita el texto terminado en 0 */
	buf = malloc(txtlen + 1);
	if (buf == NULL) {
		eon_leave(t);
		return false;
	}
	memcpy(buf, txt, txtlen);
	buf[txtlen] = '\0';
	NSLOG(netsurf, INFO, "EonSurf: js_exec %s (%u bytes)", name != NULL ? name : "script", (unsigned)txtlen);
	if (name != NULL && strcmp(name, "importmap:") == 0) {
		/* <script type="importmap">: dom.js guarda el JSON */
		JSValue arg = JS_NewStringLen(t->ctx, buf, txtlen);
		r = eon_call_ns(t, "addImportMap", 1, &arg);
		JS_FreeValue(t->ctx, arg);
	} else if (name != NULL && strncmp(name, "module:", 7) == 0) {
		/* <script type="module">: los inline se resuelven contra la pagina */
		const char *mname = name + 7;
		JSValue href = JS_UNDEFINED;
		const char *hs = NULL;
		if (mname[0] == '?') {
			href = ns_href(t->ctx, JS_UNDEFINED, 0, NULL);
			hs = JS_ToCString(t->ctx, href);
			mname = hs != NULL ? hs : "about:blank";
		}
		r = eon_eval_module(t, buf, txtlen, mname);
		if (hs) JS_FreeCString(t->ctx, hs);
		JS_FreeValue(t->ctx, href);
	} else {
		r = JS_Eval(t->ctx, buf, txtlen, name != NULL ? name : "script",
			    JS_EVAL_TYPE_GLOBAL);
	}
	free(buf);
	ok = !JS_IsException(r);
	if (!ok) {
		eon_log_exception(t->ctx, name != NULL ? name : "script");
	} else if (JS_IsBool(r)) {
		/* javascript: URLs y manejadores devuelven false para cancelar */
		ok = JS_ToBool(t->ctx, r);
	}
	JS_FreeValue(t->ctx, r);
	eon_leave(t);
	return ok;
}

bool js_fire_event(jsthread *thread, const char *type, struct dom_document *doc, struct dom_node *target)
{
	struct jsthread *t = thread;
	JSValue args[2], r;
	bool prevented;
	(void)doc;
	if (!eon_enter(t)) return false;
	args[0] = JS_NewString(t->ctx, type);
	args[1] = eon_wrap(t->ctx, target);
	r = eon_call_ns(t, "onFire", 2, args);
	prevented = JS_ToBool(t->ctx, r);
	JS_FreeValue(t->ctx, r);
	JS_FreeValue(t->ctx, args[0]);
	JS_FreeValue(t->ctx, args[1]);
	eon_leave(t);
	return prevented;
}

bool js_dom_event_add_listener(jsthread *thread, struct dom_document *document,
			       struct dom_node *node, struct dom_string *event_type_dom,
			       void *js_funcval)
{
	(void)thread; (void)document; (void)node; (void)event_type_dom; (void)js_funcval;
	return true;
}

/* Elementos nuevos con atributos on*="...": dom.js arma los manejadores */
void js_handle_new_element(jsthread *thread, struct dom_element *node)
{
	struct jsthread *t = thread;
	dom_namednodemap *map = NULL;
	dom_ulong len = 0, i;
	bool has_on = false;
	JSValue arg;

	/* elementos personalizados (<mi-elemento>): dom.js los "mejora" si estan definidos */
	{
		dom_string *tag = NULL;
		dom_node_get_node_name((dom_node *)node, &tag);
		if (tag != NULL) {
			has_on = memchr(dom_string_data(tag), '-', dom_string_byte_length(tag)) != NULL;
			dom_string_unref(tag);
		}
	}
	if (dom_node_get_attributes((dom_node *)node, &map) != DOM_NO_ERR || map == NULL) return;
	dom_namednodemap_get_length(map, &len);
	for (i = 0; i < len && !has_on; i++) {
		dom_node *a = NULL;
		dom_string *name = NULL;
		dom_namednodemap_item(map, i, &a);
		if (a == NULL) continue;
		dom_node_get_node_name(a, &name);
		if (name != NULL) {
			const char *s = dom_string_data(name);
			has_on = dom_string_byte_length(name) > 2 &&
				(s[0] == 'o' || s[0] == 'O') && (s[1] == 'n' || s[1] == 'N');
			dom_string_unref(name);
		}
		dom_node_unref(a);
	}
	dom_namednodemap_unref(map);
	if (!has_on || !eon_enter(t)) return;
	arg = eon_wrap(t->ctx, (dom_node *)node);
	JS_FreeValue(t->ctx, eon_call_ns(t, "onNewElement", 1, &arg));
	JS_FreeValue(t->ctx, arg);
	eon_leave(t);
}

void js_event_cleanup(jsthread *thread, struct dom_event *evt)
{
	(void)thread; (void)evt;
}
