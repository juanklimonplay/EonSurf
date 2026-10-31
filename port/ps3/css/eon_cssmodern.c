/*
 * EonSurf: "modernizador" de CSS para NetSurf 3.11.
 *
 * Despues de resolver las variables (eon_cssvars.c), recorre las reglas y
 * traduce lo que libcss no entiende a algo equivalente que si entiende:
 *
 *   inset: a b c d            -> top/right/bottom/left
 *   inset-inline / -block     -> left+right / top+bottom
 *   display: grid             -> display: flex + flex-wrap: wrap
 *   grid-template-columns     -> regla extra "SELECTOR > * { width: X }"
 *   grid-column: 1 / -1, span -> width: 100%
 *   gap / row-gap             -> column-gap (+ margen abajo de los hijos)
 *   place-items/place-content -> align-items/justify-content
 *
 * No es grid de verdad, pero las grillas de tarjetas, columnas y diseños de
 * dos o tres columnas quedan parecidas en vez de apiladas o encimadas.
 *
 * Se incluye desde content/handlers/css/css.c despues de eon_cssvars.c.
 */

#include <strings.h>

/* memmem propio: con -std=c99 newlib no lo declara */
static const char *eon_memmem(const char *h, size_t hn, const char *n, size_t nn)
{
	size_t i;
	if (nn == 0) return h;
	for (i = 0; i + nn <= hn; i++) {
		if (h[i] == n[0] && memcmp(h + i, n, nn) == 0) return h + i;
	}
	return NULL;
}

typedef struct {
	const char *s;
	size_t n;
} eon_span;

static bool eon_span_is(eon_span v, const char *lit)
{
	size_t l = strlen(lit);
	return v.n == l && strncasecmp(v.s, lit, l) == 0;
}

static eon_span eon_trim(const char *s, const char *e)
{
	eon_span r;
	while (s < e && isspace((unsigned char)*s)) s++;
	while (e > s && isspace((unsigned char)e[-1])) e--;
	r.s = s;
	r.n = (size_t)(e - s);
	return r;
}

/* parte un valor en hasta max palabras de primer nivel (respeta parentesis) */
static int eon_words(eon_span v, eon_span *out, int max)
{
	const char *p = v.s, *end = v.s + v.n;
	int n = 0;
	while (p < end && n < max) {
		const char *w;
		int d = 0;
		while (p < end && isspace((unsigned char)*p)) p++;
		if (p >= end) break;
		w = p;
		while (p < end && (d > 0 || !isspace((unsigned char)*p))) {
			if (*p == '(') d++;
			else if (*p == ')') d--;
			p++;
		}
		out[n].s = w;
		out[n].n = (size_t)(p - w);
		n++;
	}
	return n;
}

static void eon_putz(eon_out *o, const char *s)
{
	eon_put(o, s, strlen(s));
}

static void eon_putspan(eon_out *o, eon_span v)
{
	eon_put(o, v.s, v.n);
}

static void eon_decl(eon_out *o, const char *prop, eon_span v, eon_span imp)
{
	eon_putz(o, prop);
	eon_put(o, ":", 1);
	eon_putspan(o, v);
	if (imp.n) {
		eon_put(o, " ", 1);
		eon_putspan(o, imp);
	}
	eon_put(o, ";", 1);
}

/* medida CSS -> porcentaje aproximado de un contenedor de 1200px */
static double eon_len_pct(const char *v)
{
	double n;
	char *e;
	if (v == NULL || *v == '\0') return 0;
	n = strtod(v, &e);
	if (e == v) return 0;
	if (*e == '%') return n;
	if (strncasecmp(e, "rem", 3) == 0 || strncasecmp(e, "em", 2) == 0) return n * 16.0 / 12.0;
	if (strncasecmp(e, "px", 2) == 0 || *e == '\0') return n / 12.0;
	if (strncasecmp(e, "vw", 2) == 0) return n;
	return n / 12.0;
}

/* pistas de grid-template-columns: cada una es una medida fija o "fr" */
#define EON_MAX_TRACKS 12
typedef struct {
	int n;                      /* cantidad de columnas; 0 = no se entendio */
	char fixed[EON_MAX_TRACKS][48]; /* medida fija, o "" si es fr */
	double fr[EON_MAX_TRACKS];
	char autofill[48];          /* repeat(auto-fill, minmax(X, ...)) -> X */
} eon_tracks;

static void eon_track_add(eon_tracks *t, eon_span w)
{
	if (t->n >= EON_MAX_TRACKS) return;
	t->fixed[t->n][0] = '\0';
	t->fr[t->n] = 0;
	if (w.n > 2 && strncasecmp(w.s + w.n - 2, "fr", 2) == 0) {
		t->fr[t->n] = atof(w.s) > 0 ? atof(w.s) : 1;
	} else if (w.n > 7 && strncasecmp(w.s, "minmax(", 7) == 0) {
		/* minmax(min, max): si max es fr se reparte, si no se usa max */
		const char *c = memchr(w.s, ',', w.n);
		eon_span mx = c ? eon_trim(c + 1, w.s + w.n - 1) : w;
		if (mx.n > 2 && strncasecmp(mx.s + mx.n - 2, "fr", 2) == 0) t->fr[t->n] = 1;
		else snprintf(t->fixed[t->n], 48, "%.*s", (int)mx.n, mx.s);
	} else if (w.n && (isdigit((unsigned char)w.s[0]) || w.s[0] == '.') &&
		   !(w.n && w.s[w.n - 1] == '%' && 0)) {
		snprintf(t->fixed[t->n], 48, "%.*s", (int)w.n, w.s);
	} else {
		t->fr[t->n] = 1; /* auto, min-content, fit-content(): repartir */
	}
	t->n++;
}

static void eon_parse_tracks(eon_span v, eon_tracks *t)
{
	eon_span w[EON_MAX_TRACKS];
	int n, i;
	memset(t, 0, sizeof(*t));
	n = eon_words(v, w, EON_MAX_TRACKS);
	for (i = 0; i < n; i++) {
		if (w[i].n > 7 && strncasecmp(w[i].s, "repeat(", 7) == 0) {
			const char *a = w[i].s + 7, *e = w[i].s + w[i].n - 1, *c = memchr(a, ',', (size_t)(e - a));
			eon_span inner;
			if (c == NULL) continue;
			inner = eon_trim(c + 1, e);
			if (isdigit((unsigned char)*a)) {
				int k, rep = atoi(a);
				eon_span iw[EON_MAX_TRACKS];
				int in = eon_words(inner, iw, EON_MAX_TRACKS), j;
				for (k = 0; k < rep && k < EON_MAX_TRACKS; k++)
					for (j = 0; j < in; j++) eon_track_add(t, iw[j]);
			} else {
				/* auto-fill / auto-fit: ancho minimo de cada tarjeta */
				const char *mm = eon_memmem(inner.s, inner.n, "minmax(", 7);
				if (mm != NULL) {
					const char *x = mm + 7, *cc = memchr(x, ',', (size_t)(inner.s + inner.n - x));
					if (cc) {
						eon_span m = eon_trim(x, cc);
						snprintf(t->autofill, sizeof(t->autofill), "%.*s", (int)m.n, m.s);
					}
				} else {
					snprintf(t->autofill, sizeof(t->autofill), "%.*s", (int)inner.n, inner.s);
				}
				return;
			}
		} else {
			eon_track_add(t, w[i]);
		}
	}
}

/* "a, b" -> escribe "a > X,b > X" "a > X", "b > X" separados por coma */
static void eon_child_selectors(eon_out *x, eon_span sel, const char *child)
{
	const char *s = sel.s, *se = sel.s + sel.n;
	bool first = true;
	while (s < se) {
		const char *c = s;
		int d = 0;
		eon_span one;
		while (c < se && (d > 0 || *c != ',')) {
			if (*c == '(') d++;
			else if (*c == ')') d--;
			c++;
		}
		one = eon_trim(s, c);
		if (one.n) {
			if (!first) eon_put(x, ",", 1);
			eon_putspan(x, one);
			eon_putz(x, " > ");
			eon_putz(x, child);
			first = false;
		}
		s = c < se ? c + 1 : c;
	}
}

/* reglas para los hijos de una grilla: ancho por columna con :nth-child */
static void eon_grid_rules(eon_out *x, eon_span sel, eon_tracks *t, const char *colgap, const char *rowgap)
{
	char buf[64];
	int i;
	if (t->autofill[0]) {
		eon_child_selectors(x, sel, "*");
		eon_putz(x, "{box-sizing:border-box;flex:1 1 ");
		eon_putz(x, t->autofill);
		eon_putz(x, ";min-width:");
		eon_putz(x, t->autofill);
		if (rowgap[0]) { eon_putz(x, ";margin-bottom:"); eon_putz(x, rowgap); }
		eon_putz(x, "}\n");
		return;
	}
	if (t->n == 0) return;
	{
		/* espacio libre para las columnas fr = 100% - fijas - gaps */
		char freeexp[400];
		double totalfr = 0;
		size_t fl;
		fl = (size_t)snprintf(freeexp, sizeof(freeexp), "100%%");
		for (i = 0; i < t->n; i++) {
			if (t->fixed[i][0]) fl += (size_t)snprintf(freeexp + fl, sizeof(freeexp) - fl, " - %s", t->fixed[i]);
			else totalfr += t->fr[i];
		}
		if (colgap[0] && t->n > 1)
			snprintf(freeexp + fl, sizeof(freeexp) - fl, " - %d * %s", t->n - 1, colgap);
		/* regla comun a todos los hijos */
		eon_child_selectors(x, sel, "*");
		eon_putz(x, "{box-sizing:border-box;flex:0 0 auto;");
		if (rowgap[0]) { eon_putz(x, "margin-bottom:"); eon_putz(x, rowgap); eon_putz(x, ";"); }
		if (t->n == 1) eon_putz(x, "width:100%;");
		eon_putz(x, "}\n");
		if (t->n == 1) return;
		/* libcss no siempre acepta calc(): primero un ancho en %, aproximando las
		 * medidas fijas y el gap como si el contenedor midiera 1200px */
		{
			double gpct = eon_len_pct(colgap), fixedpct = 0, avail;
			for (i = 0; i < t->n; i++) if (t->fixed[i][0]) fixedpct += eon_len_pct(t->fixed[i]);
			avail = 100.0 - fixedpct - gpct * (t->n - 1);
			if (avail < 10) avail = 10;
			for (i = 0; i < t->n; i++) {
				double pct = t->fixed[i][0] ? eon_len_pct(t->fixed[i]) :
					avail * t->fr[i] / (totalfr > 0 ? totalfr : 1);
				snprintf(buf, sizeof(buf), ":nth-child(%dn+%d)", t->n, i + 1);
				eon_child_selectors(x, sel, buf);
				snprintf(buf, sizeof(buf), "{width:%.3f%%;", pct);
				eon_putz(x, buf);
				if (gpct > 0 && i < t->n - 1) {
					snprintf(buf, sizeof(buf), "margin-right:%.3f%%;", gpct);
					eon_putz(x, buf);
				}
				if (t->fixed[i][0]) {
					eon_putz(x, "width:");
					eon_putz(x, t->fixed[i]);
					eon_putz(x, ";");
				}
				eon_putz(x, "}\n");
			}
		}
	}
}

/* reescribe un bloque de declaraciones; extra recibe reglas nuevas para los hijos */
static void eon_modern_block(eon_out *o, eon_out *extra, eon_span sel, const char *p, const char *end)
{
	bool is_grid = false;
	bool have_tracks = false;
	eon_tracks tracks;
	char colgap[80] = "";
	char rowgap[80] = "";

	while (p < end) {
		const char *ds = p, *colon = NULL, *de;
		int d = 0;
		char q = 0;
		eon_span prop, val, imp = { "", 0 };

		/* fin de la declaracion */
		for (de = p; de < end; de++) {
			if (q) { if (*de == q) q = 0; continue; }
			if (*de == '"' || *de == '\'') q = *de;
			else if (*de == '(') d++;
			else if (*de == ')') d--;
			else if (*de == ':' && colon == NULL && d == 0) colon = de;
			else if (*de == ';' && d == 0) break;
		}
		p = (de < end) ? de + 1 : end;
		if (colon == NULL) {
			eon_put(o, ds, (size_t)(de - ds));
			if (de < end) eon_put(o, ";", 1);
			continue;
		}
		prop = eon_trim(ds, colon);
		val = eon_trim(colon + 1, de);
		/* separar !important */
		{
			const char *bang = memchr(val.s, '!', val.n);
			if (bang != NULL) {
				imp = eon_trim(bang, val.s + val.n);
				val = eon_trim(val.s, bang);
			}
		}

		if (eon_span_is(prop, "inset")) {
			eon_span w[4];
			int n = eon_words(val, w, 4);
			if (n >= 1) {
				eon_span t = w[0], r = n > 1 ? w[1] : w[0], b = n > 2 ? w[2] : w[0], l = n > 3 ? w[3] : r;
				eon_decl(o, "top", t, imp);
				eon_decl(o, "right", r, imp);
				eon_decl(o, "bottom", b, imp);
				eon_decl(o, "left", l, imp);
			}
		} else if (eon_span_is(prop, "inset-inline")) {
			eon_span w[2];
			int n = eon_words(val, w, 2);
			if (n >= 1) {
				eon_decl(o, "left", w[0], imp);
				eon_decl(o, "right", n > 1 ? w[1] : w[0], imp);
			}
		} else if (eon_span_is(prop, "inset-block")) {
			eon_span w[2];
			int n = eon_words(val, w, 2);
			if (n >= 1) {
				eon_decl(o, "top", w[0], imp);
				eon_decl(o, "bottom", n > 1 ? w[1] : w[0], imp);
			}
		} else if (eon_span_is(prop, "inset-inline-start")) {
			eon_decl(o, "left", val, imp);
		} else if (eon_span_is(prop, "inset-inline-end")) {
			eon_decl(o, "right", val, imp);
		} else if (eon_span_is(prop, "inset-block-start")) {
			eon_decl(o, "top", val, imp);
		} else if (eon_span_is(prop, "inset-block-end")) {
			eon_decl(o, "bottom", val, imp);
		} else if (eon_span_is(prop, "display") &&
			   (eon_span_is(val, "grid") || eon_span_is(val, "inline-grid"))) {
			eon_span flex = { eon_span_is(val, "grid") ? "flex" : "inline-flex", 0 };
			eon_span wrap = { "wrap", 4 };
			flex.n = strlen(flex.s);
			eon_decl(o, "display", flex, imp);
			eon_decl(o, "flex-wrap", wrap, imp);
			is_grid = true;
		} else if (eon_span_is(prop, "grid-template-columns")) {
			eon_parse_tracks(val, &tracks);
			have_tracks = tracks.n > 0 || tracks.autofill[0];
		} else if (eon_span_is(prop, "grid-column") || eon_span_is(prop, "grid-area")) {
			/* 1 / -1 o span grande: ocupar toda la fila */
			if ((val.n >= 2 && eon_memmem(val.s, val.n, "-1", 2) != NULL) ||
			    (val.n > 5 && strncasecmp(val.s, "span ", 5) == 0 && atoi(val.s + 5) >= 2)) {
				/* !important: le gana al ancho por columna de :nth-child */
				eon_span full = { "100%", 4 }, impt = { "!important", 10 };
				eon_decl(o, "width", full, impt);
				eon_decl(o, "flex-basis", full, impt);
				eon_decl(o, "margin-right", (eon_span){ "0", 1 }, impt);
			}
		} else if (eon_span_is(prop, "gap") || eon_span_is(prop, "grid-gap")) {
			eon_span w[2];
			int n = eon_words(val, w, 2);
			if (n >= 1) {
				eon_span cg = n > 1 ? w[1] : w[0];
				eon_decl(o, "column-gap", cg, imp);
				snprintf(colgap, sizeof(colgap), "%.*s", (int)cg.n, cg.s);
				snprintf(rowgap, sizeof(rowgap), "%.*s", (int)w[0].n, w[0].s);
			}
		} else if (eon_span_is(prop, "row-gap") || eon_span_is(prop, "grid-row-gap")) {
			snprintf(rowgap, sizeof(rowgap), "%.*s", (int)val.n, val.s);
		} else if (eon_span_is(prop, "grid-column-gap") || eon_span_is(prop, "column-gap")) {
			eon_decl(o, "column-gap", val, imp);
			snprintf(colgap, sizeof(colgap), "%.*s", (int)val.n, val.s);
		} else if (eon_span_is(prop, "place-items")) {
			eon_span w[2];
			if (eon_words(val, w, 2) >= 1) eon_decl(o, "align-items", w[0], imp);
		} else if (eon_span_is(prop, "place-content")) {
			eon_span w[2];
			int n = eon_words(val, w, 2);
			if (n >= 1) {
				eon_decl(o, "align-content", w[0], imp);
				eon_decl(o, "justify-content", n > 1 ? w[1] : w[0], imp);
			}
		} else if (eon_span_is(prop, "place-self")) {
			eon_span w[2];
			if (eon_words(val, w, 2) >= 1) eon_decl(o, "align-self", w[0], imp);
		} else {
			eon_put(o, ds, (size_t)(de - ds));
			if (de < end) eon_put(o, ";", 1);
		}
	}

	/* reglas extra para los hijos de la grilla */
	if (sel.n && have_tracks) {
		eon_grid_rules(extra, sel, &tracks, colgap, rowgap);
	} else if (sel.n && is_grid && rowgap[0]) {
		eon_child_selectors(extra, sel, "*");
		eon_putz(extra, "{margin-bottom:");
		eon_putz(extra, rowgap);
		eon_putz(extra, "}\n");
	}
}

/* recorre la hoja: reglas, @media/@supports (anidadas) y el resto tal cual */
static const char *eon_modern_rules(eon_out *o, const char *p, const char *end, bool nested)
{
	while (p < end) {
		const char *start = p, *brace;
		char q = 0;

		while (p < end && isspace((unsigned char)*p)) eon_put(o, p++, 1);
		if (p >= end) break;
		if (*p == '}') {
			if (nested) return p + 1;
			eon_put(o, p++, 1);
			continue;
		}
		if (p[0] == '/' && p + 1 < end && p[1] == '*') {
			const char *c = strstr(p + 2, "*/");
			c = (c != NULL && c < end) ? c + 2 : end;
			eon_put(o, p, (size_t)(c - p));
			p = c;
			continue;
		}
		/* buscar '{' o ';' (at-rules sin bloque como @import) */
		for (brace = p; brace < end; brace++) {
			if (q) { if (*brace == q) q = 0; continue; }
			if (*brace == '"' || *brace == '\'') q = *brace;
			else if (*brace == '{' || *brace == ';' || *brace == '}') break;
		}
		if (brace >= end || *brace != '{') {
			/* @import ...; u otra cosa sin bloque */
			const char *e = (brace < end && *brace == ';') ? brace + 1 : brace;
			eon_put(o, p, (size_t)(e - p));
			p = e;
			continue;
		}
		if (*p == '@') {
			eon_span name = eon_trim(p, brace);
			eon_put(o, p, (size_t)(brace - p) + 1);
			if ((name.n >= 6 && strncasecmp(name.s, "@media", 6) == 0) ||
			    (name.n >= 9 && strncasecmp(name.s, "@supports", 9) == 0) ||
			    (name.n >= 6 && strncasecmp(name.s, "@layer", 6) == 0) ||
			    (name.n >= 10 && strncasecmp(name.s, "@container", 10) == 0)) {
				p = eon_modern_rules(o, brace + 1, end, true);
				eon_put(o, "}", 1);
			} else {
				/* @font-face, @keyframes...: copiar el bloque entero */
				const char *c = brace + 1;
				int d = 1;
				while (c < end && d > 0) {
					if (*c == '{') d++;
					else if (*c == '}') d--;
					c++;
				}
				eon_put(o, brace + 1, (size_t)(c - brace - 1));
				p = c;
			}
			continue;
		}
		/* regla comun: selector { declaraciones } */
		{
			eon_span sel = eon_trim(start, brace);
			const char *c = brace + 1, *close;
			eon_out extra = { NULL, 0, 0, true };
			q = 0;
			for (close = c; close < end; close++) {
				if (q) { if (*close == q) q = 0; continue; }
				if (*close == '"' || *close == '\'') q = *close;
				else if (*close == '}') break;
			}
			eon_put(o, p, (size_t)(brace - p) + 1);
			eon_modern_block(o, &extra, sel, c, close);
			eon_put(o, "}", 1);
			if (extra.len) {
				eon_put(o, "\n", 1);
				eon_put(o, extra.s, extra.len);
			}
			free(extra.s);
			p = close < end ? close + 1 : end;
		}
	}
	return p;
}

/* devuelve CSS modernizado (liberar con free) o NULL si no hizo falta / sin memoria */
static char *eon_css_modernize(const char *css, size_t len, size_t *outlen)
{
	eon_out o = { NULL, 0, 0, true };
	/* atajo: si no hay nada de esto, no tocar */
	if (eon_memmem(css, len, "grid", 4) == NULL && eon_memmem(css, len, "inset", 5) == NULL &&
	    eon_memmem(css, len, "gap", 3) == NULL && eon_memmem(css, len, "place-", 6) == NULL) {
		return NULL;
	}
	eon_modern_rules(&o, css, css + len, false);
	if (!o.ok) {
		free(o.s);
		return NULL;
	}
	*outlen = o.len;
	return o.s;
}
