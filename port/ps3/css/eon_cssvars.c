/*
 * EonSurf: variables CSS (custom properties) para NetSurf 3.11.
 *
 * libcss no entiende `--color: red` ni `var(--color)`, y hoy casi todos los
 * sitios definen colores, tamanos y fuentes asi. Este preprocesador junta el
 * CSS completo de cada hoja, recuerda las definiciones `--nombre: valor` y
 * reemplaza cada `var(--nombre, respaldo)` por su valor antes de que libcss
 * lo lea. Es una aproximacion: no respeta la cascada por elemento (gana la
 * primera definicion de cada hoja, que casi siempre es la de :root), pero
 * alcanza para que la mayoria de los disenos tomen sus colores y medidas.
 *
 * Se incluye desde content/handlers/css/css.c (ver port/ps3/css/patch-css.py).
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define EON_VARS_MAX 8192
#define EON_VAR_DEPTH 8

typedef struct {
	char *name;   /* sin los guiones iniciales: "color-fondo" */
	char *value;
	unsigned int sheet; /* hoja que la definio por ultima vez */
} eon_var;

static eon_var eon_vars[EON_VARS_MAX];
static unsigned int eon_nvars;
static unsigned int eon_sheet_serial;

/* buffers por hoja mientras llegan los datos */
typedef struct eon_cssbuf {
	struct eon_cssbuf *next;
	const void *sheet;
	char *data;
	size_t len, cap;
} eon_cssbuf;

static eon_cssbuf *eon_cssbufs;

static eon_cssbuf *eon_cssbuf_get(const void *sheet, bool create)
{
	eon_cssbuf *b;
	for (b = eon_cssbufs; b != NULL; b = b->next) {
		if (b->sheet == sheet) return b;
	}
	if (!create) return NULL;
	b = calloc(1, sizeof(*b));
	if (b == NULL) return NULL;
	b->sheet = sheet;
	b->next = eon_cssbufs;
	eon_cssbufs = b;
	return b;
}

static bool eon_css_buffer(const void *sheet, const char *data, size_t size)
{
	eon_cssbuf *b = eon_cssbuf_get(sheet, true);
	if (b == NULL) return false;
	if (b->len + size + 1 > b->cap) {
		size_t cap = b->cap ? b->cap * 2 : 16384;
		char *nd;
		while (cap < b->len + size + 1) cap *= 2;
		nd = realloc(b->data, cap);
		if (nd == NULL) return false;
		b->data = nd;
		b->cap = cap;
	}
	memcpy(b->data + b->len, data, size);
	b->len += size;
	b->data[b->len] = '\0';
	return true;
}

static void eon_cssbuf_free(const void *sheet)
{
	eon_cssbuf **p;
	for (p = &eon_cssbufs; *p != NULL; p = &(*p)->next) {
		if ((*p)->sheet == sheet) {
			eon_cssbuf *b = *p;
			*p = b->next;
			free(b->data);
			free(b);
			return;
		}
	}
}

/* tabla hash (direccionamiento abierto) nombre -> indice en eon_vars:
 * GitHub tiene miles de variables y decenas de miles de var() */
#define EON_HASH_SIZE 16384 /* potencia de 2, mayor que EON_VARS_MAX */
static int eon_hash[EON_HASH_SIZE]; /* 0 = libre, si no indice + 1 */

static unsigned int eon_hash_of(const char *s, size_t n)
{
	unsigned int h = 2166136261u;
	while (n--) h = (h ^ (unsigned char)*s++) * 16777619u;
	return h;
}

static eon_var *eon_var_find(const char *name, size_t len)
{
	unsigned int i = eon_hash_of(name, len) & (EON_HASH_SIZE - 1);
	while (eon_hash[i] != 0) {
		eon_var *v = &eon_vars[eon_hash[i] - 1];
		if (strncmp(v->name, name, len) == 0 && v->name[len] == '\0') return v;
		i = (i + 1) & (EON_HASH_SIZE - 1);
	}
	return NULL;
}

static eon_var *eon_var_add(const char *name, size_t len)
{
	unsigned int i = eon_hash_of(name, len) & (EON_HASH_SIZE - 1);
	if (eon_nvars >= EON_VARS_MAX) return NULL;
	while (eon_hash[i] != 0) i = (i + 1) & (EON_HASH_SIZE - 1);
	eon_hash[i] = (int)eon_nvars + 1;
	return &eon_vars[eon_nvars++];
}

static bool eon_is_ident(char c)
{
	return isalnum((unsigned char)c) || c == '-' || c == '_' || (unsigned char)c >= 0x80;
}

/* fin de un valor: ';' o '}' fuera de parentesis y comillas */
static const char *eon_value_end(const char *p, const char *end)
{
	int depth = 0;
	char q = 0;
	for (; p < end; p++) {
		if (q) {
			if (*p == '\\' && p + 1 < end) p++;
			else if (*p == q) q = 0;
			continue;
		}
		if (*p == '"' || *p == '\'') q = *p;
		else if (*p == '(' || *p == '[') depth++;
		else if (*p == ')' || *p == ']') depth--;
		else if ((*p == ';' || *p == '}' || *p == '!') && depth <= 0) return p;
	}
	return end;
}

static char *eon_strndup_trim(const char *s, const char *e)
{
	char *r;
	while (s < e && isspace((unsigned char)*s)) s++;
	while (e > s && isspace((unsigned char)e[-1])) e--;
	r = malloc((size_t)(e - s) + 1);
	if (r == NULL) return NULL;
	memcpy(r, s, (size_t)(e - s));
	r[e - s] = '\0';
	return r;
}

/* primera pasada: definiciones --nombre: valor */
static void eon_collect_vars(const char *css, size_t len, unsigned int sheet)
{
	const char *p = css, *end = css + len;
	char q = 0;
	while (p + 3 < end) {
		if (q) {
			if (*p == '\\') p++;
			else if (*p == q) q = 0;
			p++;
			continue;
		}
		if (*p == '"' || *p == '\'') { q = *p++; continue; }
		if (p[0] == '/' && p[1] == '*') {
			const char *c = strstr(p + 2, "*/");
			p = c ? c + 2 : end;
			continue;
		}
		if (p[0] == '-' && p[1] == '-' && (p == css || !eon_is_ident(p[-1]))) {
			const char *n = p + 2, *ne = n, *v;
			while (ne < end && eon_is_ident(*ne)) ne++;
			v = ne;
			while (v < end && isspace((unsigned char)*v)) v++;
			if (ne > n && v < end && *v == ':') {
				const char *ve = eon_value_end(v + 1, end);
				eon_var *var = eon_var_find(n, (size_t)(ne - n));
				/* gana la primera definicion de cada hoja (la de :root, casi siempre) */
				if (var == NULL && (var = eon_var_add(n, (size_t)(ne - n))) != NULL) {
					var->name = eon_strndup_trim(n, ne);
					var->value = eon_strndup_trim(v + 1, ve);
					var->sheet = sheet;
				} else if (var != NULL && var->sheet != sheet) {
					free(var->value);
					var->value = eon_strndup_trim(v + 1, ve);
					var->sheet = sheet;
				}
				p = ve;
				continue;
			}
		}
		p++;
	}
}

/* salida que crece */
typedef struct { char *s; size_t len, cap; bool ok; } eon_out;

static void eon_put(eon_out *o, const char *s, size_t n)
{
	if (!o->ok) return;
	if (o->len + n + 1 > o->cap) {
		size_t cap = o->cap ? o->cap * 2 : 16384;
		char *ns;
		while (cap < o->len + n + 1) cap *= 2;
		ns = realloc(o->s, cap);
		if (ns == NULL) { o->ok = false; return; }
		o->s = ns;
		o->cap = cap;
	}
	memcpy(o->s + o->len, s, n);
	o->len += n;
	o->s[o->len] = '\0';
}

static void eon_subst(eon_out *o, const char *p, const char *end, int depth);

/* p apunta justo despues de "var(" ; devuelve el puntero despues del ')' */
static const char *eon_one_var(eon_out *o, const char *p, const char *end, int depth)
{
	const char *n, *ne, *close, *comma = NULL;
	int d = 0;
	char q = 0;
	eon_var *var;

	for (close = p; close < end; close++) {
		if (q) { if (*close == q) q = 0; continue; }
		if (*close == '"' || *close == '\'') q = *close;
		else if (*close == '(') d++;
		else if (*close == ')') { if (d == 0) break; d--; }
		else if (*close == ',' && d == 0 && comma == NULL) comma = close;
	}
	if (close >= end) {
		eon_put(o, "var(", 4);
		return p;
	}
	n = p;
	while (n < close && isspace((unsigned char)*n)) n++;
	if (n + 2 <= close && n[0] == '-' && n[1] == '-') n += 2;
	ne = n;
	while (ne < close && eon_is_ident(*ne)) ne++;
	var = eon_var_find(n, (size_t)(ne - n));
	if (var != NULL && var->value != NULL && var->value[0] != '\0') {
		eon_subst(o, var->value, var->value + strlen(var->value), depth + 1);
	} else if (comma != NULL) {
		eon_subst(o, comma + 1, close, depth + 1);
	} else {
		/* sin valor ni respaldo: la declaracion queda invalida, como en un navegador */
		eon_put(o, "var(", 4);
		eon_put(o, p, (size_t)(close - p));
		eon_put(o, ")", 1);
	}
	return close + 1;
}

static void eon_subst(eon_out *o, const char *p, const char *end, int depth)
{
	const char *start = p;
	if (depth > EON_VAR_DEPTH) {
		eon_put(o, p, (size_t)(end - p));
		return;
	}
	while (p + 4 <= end) {
		if ((p[0] == 'v' || p[0] == 'V') && (p[1] == 'a' || p[1] == 'A') &&
		    (p[2] == 'r' || p[2] == 'R') && p[3] == '(' &&
		    (p == start || !eon_is_ident(p[-1]))) {
			eon_put(o, start, (size_t)(p - start));
			p = eon_one_var(o, p + 4, end, depth);
			start = p;
			continue;
		}
		p++;
	}
	eon_put(o, start, (size_t)(end - start));
}

/* Devuelve el CSS de la hoja con las variables resueltas (liberar con free),
 * o NULL si no habia nada guardado. */
static char *eon_css_take(const void *sheet, size_t *len)
{
	eon_cssbuf *b = eon_cssbuf_get(sheet, false);
	eon_out o = { NULL, 0, 0, true };
	if (b == NULL || b->data == NULL) {
		eon_cssbuf_free(sheet);
		return NULL;
	}
	eon_sheet_serial++;
	eon_collect_vars(b->data, b->len, eon_sheet_serial);
	if (strstr(b->data, "var(") == NULL && strstr(b->data, "VAR(") == NULL) {
		/* nada que reemplazar: devolver el mismo texto */
		char *r = b->data;
		*len = b->len;
		b->data = NULL;
		eon_cssbuf_free(sheet);
		return r;
	}
	eon_subst(&o, b->data, b->data + b->len, 0);
	if (!o.ok) {
		/* sin memoria: devolver el CSS original */
		char *r = b->data;
		free(o.s);
		*len = b->len;
		b->data = NULL;
		eon_cssbuf_free(sheet);
		return r;
	}
	eon_cssbuf_free(sheet);
	*len = o.len;
	return o.s;
}
