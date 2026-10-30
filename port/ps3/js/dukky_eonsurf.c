/*
 * EonSurf: funciones nativas para el polyfill de JavaScript (eonsurf-polyfill.js).
 *
 * Se inserta en content/handlers/javascript/duktape/dukky.c (ver patch-js.py).
 *
 *   __eonHttp(id, method, url, base, body)
 *       Pide url (relativa a base) con la cache de NetSurf; al terminar llama
 *       __eonHttpDone(id, status, text, contentType). status 0 = error.
 *   __eonStorage(origin)        -> texto guardado o undefined
 *   __eonStorage(origin, text)  -> guarda el texto
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "utils/nsurl.h"
#include "content/llcache.h"
#include "desktop/gui_internal.h"
#include "netsurf/misc.h"

#ifndef EONSURF_STORAGE_DIR
#define EONSURF_STORAGE_DIR "/dev_hdd0/game/EONS00001/USRDIR/storage"
#endif
#define EON_THREAD_KEY "\xff" "eonThread"
#define EON_STORAGE_MAX (4 * 1024 * 1024)

typedef struct eon_req {
	struct eon_req *next;
	jsthread *thread;
	duk_int_t id;
	llcache_handle *handle;
	char *post;
} eon_req;

static eon_req *eon_reqs;

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

static void eon_release_handle(void *p)
{
	llcache_handle_release(p);
}

static void eon_call_done(duk_context *ctx, duk_int_t id, int status,
			  const uint8_t *data, size_t len, const char *type)
{
	duk_get_global_string(ctx, "__eonHttpDone");
	if (duk_is_function(ctx, -1)) {
		duk_push_int(ctx, id);
		duk_push_int(ctx, status);
		duk_push_lstring(ctx, data != NULL ? (const char *)data : "",
				 data != NULL ? len : 0);
		duk_push_string(ctx, type != NULL ? type : "");
		dukky_pcall(ctx, 4, true);
	}
	duk_pop(ctx);
}

static nserror eon_http_cb(llcache_handle *h, const llcache_event *ev, void *pw)
{
	eon_req *r = pw;
	jsthread *thread = r->thread;
	const uint8_t *data = NULL;
	size_t len = 0;
	int status;

	if (ev->type == LLCACHE_EVENT_DONE) {
		data = llcache_handle_get_source_data(h, &len);
		status = 200;
	} else if (ev->type == LLCACHE_EVENT_ERROR) {
		status = 0;
	} else {
		return NSERROR_OK;
	}

	eon_req_unlink(r);
	if (thread != NULL && thread->pending_destroy == false) {
		dukky_enter_thread(thread);
		eon_call_done(thread->ctx, r->id, status, data, len,
			      status ? llcache_handle_get_header(h, "Content-Type") : NULL);
		dukky_leave_thread(thread);
	}

	/* no liberar la cache desde su propio aviso */
	guit->misc->schedule(0, eon_release_handle, h);
	free(r->post);
	free(r);
	return NSERROR_OK;
}

static duk_ret_t eon_native_http(duk_context *ctx)
{
	duk_int_t id = duk_get_int(ctx, 0);
	const char *method = duk_safe_to_string(ctx, 1);
	const char *url = duk_safe_to_string(ctx, 2);
	const char *base = duk_safe_to_string(ctx, 3);
	llcache_post_data post;
	nsurl *b = NULL, *u = NULL;
	jsthread *thread;
	eon_req *r;
	nserror err;

	duk_get_global_string(ctx, EON_THREAD_KEY);
	thread = duk_get_pointer(ctx, -1);
	duk_pop(ctx);

	if (nsurl_create(base, &b) == NSERROR_OK) {
		err = nsurl_join(b, url, &u);
		nsurl_unref(b);
	} else {
		err = nsurl_create(url, &u);
	}
	if (err != NSERROR_OK) {
		eon_call_done(ctx, id, 0, NULL, 0, NULL);
		return 0;
	}

	r = calloc(1, sizeof(*r));
	if (r == NULL) {
		nsurl_unref(u);
		eon_call_done(ctx, id, 0, NULL, 0, NULL);
		return 0;
	}
	r->thread = thread;
	r->id = id;
	if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
		r->post = strdup(duk_is_null_or_undefined(ctx, 4) ?
				 "" : duk_safe_to_string(ctx, 4));
		post.type = LLCACHE_POST_URL_ENCODED;
		post.data.urlenc = r->post;
	}

	/* enlazar antes: la cache puede responder enseguida */
	r->next = eon_reqs;
	eon_reqs = r;
	err = llcache_handle_retrieve(u, LLCACHE_RETRIEVE_NO_ERROR_PAGES, NULL,
				      r->post != NULL ? &post : NULL,
				      eon_http_cb, r, &r->handle);
	nsurl_unref(u);
	if (err != NSERROR_OK) {
		eon_req_unlink(r);
		free(r->post);
		free(r);
		eon_call_done(ctx, id, 0, NULL, 0, NULL);
	}
	return 0;
}

/* cancelar los pedidos de una pagina que se cierra */
static void eon_abort_thread(jsthread *thread)
{
	eon_req **p = &eon_reqs;
	while (*p != NULL) {
		eon_req *r = *p;
		if (r->thread == thread) {
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

static void eon_storage_path(const char *origin, char *buf, size_t n)
{
	size_t j = (size_t)snprintf(buf, n, "%s/", EONSURF_STORAGE_DIR);
	for (; *origin != '\0' && j + 6 < n; origin++) {
		unsigned char c = (unsigned char)*origin;
		buf[j++] = (isalnum(c) || c == '.' || c == '-') ? (char)c : '_';
	}
	snprintf(buf + j, n - j, ".json");
}

static duk_ret_t eon_native_storage(duk_context *ctx)
{
	char path[512];
	FILE *f;
	long size;
	char *buf;

	eon_storage_path(duk_safe_to_string(ctx, 0), path, sizeof(path));

	if (duk_get_top(ctx) >= 2 && !duk_is_undefined(ctx, 1)) {
		duk_size_t len;
		const char *data = duk_safe_to_lstring(ctx, 1, &len);
		if (len > EON_STORAGE_MAX) {
			return 0;
		}
		mkdir(EONSURF_STORAGE_DIR, 0777);
		f = fopen(path, "wb");
		if (f != NULL) {
			fwrite(data, 1, len, f);
			fclose(f);
		}
		return 0;
	}

	f = fopen(path, "rb");
	if (f == NULL) {
		return 0;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= 0 || size > EON_STORAGE_MAX ||
	    (buf = malloc((size_t)size)) == NULL) {
		fclose(f);
		return 0;
	}
	size = (long)fread(buf, 1, (size_t)size, f);
	fclose(f);
	duk_push_lstring(ctx, buf, (duk_size_t)size);
	free(buf);
	return 1;
}

static void eon_register_natives(duk_context *ctx, jsthread *thread)
{
	duk_push_pointer(ctx, thread);
	duk_put_global_string(ctx, EON_THREAD_KEY);
	duk_push_c_function(ctx, eon_native_http, 5);
	duk_put_global_string(ctx, "__eonHttp");
	duk_push_c_function(ctx, eon_native_storage, DUK_VARARGS);
	duk_put_global_string(ctx, "__eonStorage");
}
