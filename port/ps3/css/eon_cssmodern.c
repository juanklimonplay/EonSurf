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
 *   gap                       -> column-gap (el flex de EonSurf lo usa en los dos ejes)
 *   place-items/place-content -> align-items/justify-content
 *   calc() min() max() clamp() -> el valor ya calculado (libcss no los tiene)
 *   padding-inline, margin-block-start, inline-size... -> propiedades fisicas
 *   @layer, oklch(), :where()/:is(), media queries con rangos (Tailwind 4)
 *
 * No es grid de verdad, pero las grillas de tarjetas, columnas y diseños de
 * dos o tres columnas quedan parecidas en vez de apiladas o encimadas.
 *
 * Se incluye desde content/handlers/css/css.c despues de eon_cssvars.c.
 */

#include <strings.h>
#include <math.h>

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
		/* se pueden encoger: el flex les saca el lugar de los gaps */
		eon_putz(x, "{box-sizing:border-box;flex:0 1 auto;");
		if (rowgap[0]) { eon_putz(x, "margin-bottom:"); eon_putz(x, rowgap); eon_putz(x, ";"); }
		if (t->n == 1) eon_putz(x, "width:100%;");
		eon_putz(x, "}\n");
		if (t->n == 1) return;
		/* libcss no siempre acepta calc(): primero un ancho en %, aproximando las
		 * medidas fijas y el gap como si el contenedor midiera 1200px */
		{
			double gpct = 0, fixedpct = 0, avail; /* el gap lo pone el flex */
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

/* ---------------------------------------------------------------- transform */

/* translate(x, y), translateX(x), translateY(y), translate3d(x, y, z) dentro de
 * un valor de transform; devuelve false si no hay traslacion */
static bool eon_parse_translate(eon_span v, char *x, size_t xn, char *y, size_t yn)
{
	const char *p = v.s, *end = v.s + v.n;
	bool found = false;
	x[0] = y[0] = '\0';
	while (p < end) {
		const char *f = p, *open;
		while (p < end && (isalpha((unsigned char)*p) || isdigit((unsigned char)*p))) p++;
		if (p >= end || *p != '(') { p++; continue; }
		open = p;
		{
			eon_span name = eon_trim(f, open), args[3];
			const char *close = memchr(open, ')', (size_t)(end - open));
			int n = 0;
			const char *a = open + 1;
			if (close == NULL) break;
			while (a < close && n < 3) {
				const char *c = memchr(a, ',', (size_t)(close - a));
				if (c == NULL) c = close;
				args[n++] = eon_trim(a, c);
				a = c + 1;
			}
			if (eon_span_is(name, "translate") || eon_span_is(name, "translate3d")) {
				if (n >= 1) snprintf(x, xn, "%.*s", (int)args[0].n, args[0].s);
				if (n >= 2) snprintf(y, yn, "%.*s", (int)args[1].n, args[1].s);
				found = true;
			} else if (eon_span_is(name, "translateX")) {
				if (n >= 1) snprintf(x, xn, "%.*s", (int)args[0].n, args[0].s);
				found = true;
			} else if (eon_span_is(name, "translateY")) {
				if (n >= 1) snprintf(y, yn, "%.*s", (int)args[0].n, args[0].s);
				found = true;
			}
			p = close + 1;
		}
	}
	return found;
}

static bool eon_is_zero(const char *v)
{
	return v[0] == '\0' || ((v[0] == '0' || v[0] == '-') && strtod(v, NULL) == 0.0);
}

/* lo que el bloque dice de posicion: para elegir como traducir transform */
typedef struct {
	char position[16];
	char top[24], left[24], height[24];
	char tx[48], ty[48];
	bool has_translate;
	bool has_mask; /* mask-image: url(...) -> se dibuja como fondo */
} eon_blockinfo;

static void eon_block_scan(const char *p, const char *end, eon_blockinfo *bi)
{
	memset(bi, 0, sizeof(*bi));
	while (p < end) {
		const char *ds = p, *colon = NULL, *de;
		int d = 0;
		for (de = p; de < end; de++) {
			if (*de == '(') d++;
			else if (*de == ')') d--;
			else if (*de == ':' && colon == NULL && d == 0) colon = de;
			else if (*de == ';' && d == 0) break;
		}
		p = de < end ? de + 1 : end;
		if (colon == NULL) continue;
		{
			eon_span prop = eon_trim(ds, colon), val = eon_trim(colon + 1, de);
			const char *bang = memchr(val.s, '!', val.n);
			if (bang) val = eon_trim(val.s, bang);
			if (eon_span_is(prop, "position")) snprintf(bi->position, sizeof(bi->position), "%.*s", (int)val.n, val.s);
			else if (eon_span_is(prop, "top")) snprintf(bi->top, sizeof(bi->top), "%.*s", (int)val.n, val.s);
			else if (eon_span_is(prop, "left")) snprintf(bi->left, sizeof(bi->left), "%.*s", (int)val.n, val.s);
			else if (eon_span_is(prop, "height")) snprintf(bi->height, sizeof(bi->height), "%.*s", (int)val.n, val.s);
			else if ((eon_span_is(prop, "mask-image") || eon_span_is(prop, "-webkit-mask-image") ||
				  eon_span_is(prop, "mask") || eon_span_is(prop, "-webkit-mask")) &&
				 eon_memmem(val.s, val.n, "url(", 4) != NULL)
				bi->has_mask = true;
			else if (eon_span_is(prop, "transform"))
				bi->has_translate = eon_parse_translate(val, bi->tx, sizeof(bi->tx), bi->ty, sizeof(bi->ty));
		}
	}
}

/* centrado clasico: left:50% + translateX(-50%) (igual con top) */
static bool eon_center_x(const eon_blockinfo *bi) { return bi->has_translate && strcmp(bi->tx, "-50%") == 0 && strcmp(bi->left, "50%") == 0; }
static bool eon_center_y(const eon_blockinfo *bi) { return bi->has_translate && strcmp(bi->ty, "-50%") == 0 && strcmp(bi->top, "50%") == 0; }

/* transform -> algo que NetSurf entiende */
static void eon_emit_translate(eon_out *o, const eon_blockinfo *bi, eon_span imp)
{
	bool abs = strcasecmp(bi->position, "absolute") == 0 || strcasecmp(bi->position, "fixed") == 0;
	bool cx = eon_center_x(bi), cy = eon_center_y(bi);
	char buf[96];
	if (cx) {
		eon_putz(o, "left:0;right:0;margin-left:auto;margin-right:auto;");
	}
	if (cy) {
		/* con alto conocido se centra; si no, top/bottom 0 lo estiraria */
		/* tecnica clasica: top 50% y margen negativo de medio alto
		 * (NetSurf no centra con margenes automaticos verticales) */
		char *unit = NULL;
		double h = bi->height[0] ? strtod(bi->height, &unit) : 0;
		if (h > 0 && unit != NULL && (strncasecmp(unit, "px", 2) == 0 ||
		    strncasecmp(unit, "em", 2) == 0 || strncasecmp(unit, "rem", 3) == 0 ||
		    strncasecmp(unit, "vh", 2) == 0 || *unit == '%')) {
			/* En NetSurf "fixed" se mide contra toda la pagina (y vh tampoco
			 * sirve): se usa la mitad de la pantalla de la PS3 en pixeles,
			 * 668 px visibles debajo de la barra. */
			if (strcasecmp(bi->position, "fixed") == 0 && strncasecmp(unit, "px", 2) == 0) {
				double top = 334.0 - h / 2;
				snprintf(buf, sizeof(buf), "top:%.0fpx;", top > 0 ? top : 0);
			} else {
				snprintf(buf, sizeof(buf), "top:50%%;margin-top:-%.3f%s;", h / 2, unit);
			}
			eon_putz(o, buf);
		} else {
			/* sin alto conocido: cerca de arriba de la pantalla */
			eon_putz(o, strcasecmp(bi->position, "fixed") == 0 ? "top:60px;" : "top:10%;");
		}
	}
	if (abs) {
		/* ya posicionado: correrlo con margenes */
		if (!cx && !eon_is_zero(bi->tx)) { snprintf(buf, sizeof(buf), "margin-left:%s;", bi->tx); eon_putz(o, buf); }
		if (!cy && !eon_is_zero(bi->ty)) { snprintf(buf, sizeof(buf), "margin-top:%s;", bi->ty); eon_putz(o, buf); }
	} else if (!eon_is_zero(bi->tx) || !eon_is_zero(bi->ty)) {
		/* en el flujo: posicion relativa, que tampoco empuja a los vecinos */
		if (bi->position[0] == '\0' || strcasecmp(bi->position, "static") == 0) eon_putz(o, "position:relative;");
		if (!eon_is_zero(bi->tx)) { snprintf(buf, sizeof(buf), "left:%s;", bi->tx); eon_putz(o, buf); }
		if (!eon_is_zero(bi->ty)) { snprintf(buf, sizeof(buf), "top:%s;", bi->ty); eon_putz(o, buf); }
	}
	(void)imp;
}

/* ---------------------------------------------------------------- calc() */

/* libcss (NetSurf 3.11) no tiene calc(), min(), max() ni clamp() y tira la
 * declaracion entera. Se calculan aca: las medidas se juntan en px (1rem = 16px,
 * pantalla de 1280x720), y % y em por separado. Si algo no se entiende (var()
 * sin resolver, env(), unidades raras) se deja como estaba. */
typedef struct {
	double px, pct, em, num;
	bool len; /* es una medida (si no, un numero solo) */
	bool ok;
} eon_cval;

static const eon_cval eon_cbad = { 0, 0, 0, 0, false, false };

static const char *eon_cskip(const char *p, const char *e)
{
	while (p < e && isspace((unsigned char)*p)) p++;
	return p;
}

static eon_cval eon_cscale(eon_cval v, double k)
{
	v.px *= k; v.pct *= k; v.em *= k; v.num *= k;
	return v;
}

/* para comparar en min()/max()/clamp(): % de un contenedor de 1200px */
static double eon_cflat(eon_cval v)
{
	return v.len ? v.px + v.pct * 12.0 + v.em * 16.0 : v.num;
}

static eon_cval eon_cexpr(const char **pp, const char *e, int depth);

static eon_cval eon_cminmax(const char **pp, const char *e, int depth, int kind)
{
	eon_cval a[8], r;
	int n = 0, i, best = 0;
	const char *p = *pp;
	for (;;) {
		if (n >= 8) return eon_cbad;
		a[n] = eon_cexpr(&p, e, depth + 1);
		if (!a[n].ok) return eon_cbad;
		if (n > 0 && a[n].len != a[0].len) return eon_cbad;
		n++;
		p = eon_cskip(p, e);
		if (p < e && *p == ',') { p++; continue; }
		if (p < e && *p == ')') { p++; break; }
		return eon_cbad;
	}
	*pp = p;
	if (kind == 2) {
		/* clamp(min, val, max) = max(min, min(val, max)) */
		if (n != 3) return eon_cbad;
		r = eon_cflat(a[1]) < eon_cflat(a[2]) ? a[1] : a[2];
		return eon_cflat(a[0]) > eon_cflat(r) ? a[0] : r;
	}
	for (i = 1; i < n; i++) {
		if (kind == 0 ? eon_cflat(a[i]) < eon_cflat(a[best]) : eon_cflat(a[i]) > eon_cflat(a[best]))
			best = i;
	}
	return a[best];
}

static eon_cval eon_cprimary(const char **pp, const char *e, int depth)
{
	const char *p = eon_cskip(*pp, e);
	eon_cval v = { 0, 0, 0, 0, false, true };
	if (depth > 16 || p >= e) return eon_cbad;
	if (*p == '(' || (e - p > 5 && strncasecmp(p, "calc(", 5) == 0)) {
		p += *p == '(' ? 1 : 5;
		v = eon_cexpr(&p, e, depth + 1);
		p = eon_cskip(p, e);
		if (!v.ok || p >= e || *p != ')') return eon_cbad;
		*pp = p + 1;
		return v;
	}
	if (e - p > 4 && strncasecmp(p, "min(", 4) == 0) { p += 4; v = eon_cminmax(&p, e, depth, 0); *pp = p; return v; }
	if (e - p > 4 && strncasecmp(p, "max(", 4) == 0) { p += 4; v = eon_cminmax(&p, e, depth, 1); *pp = p; return v; }
	if (e - p > 6 && strncasecmp(p, "clamp(", 6) == 0) { p += 6; v = eon_cminmax(&p, e, depth, 2); *pp = p; return v; }
	if (e - p > 4 && strncasecmp(p, "env(", 4) == 0) {
		/* env(safe-area-inset-*, respaldo): en la PS3 no hay zona segura */
		const char *c = p + 4;
		int d = 1;
		while (c < e && d > 0) {
			if (*c == '(') d++;
			else if (*c == ')') d--;
			else if (*c == ',' && d == 1) {
				const char *f = c + 1;
				v = eon_cexpr(&f, e, depth + 1);
				f = eon_cskip(f, e);
				if (!v.ok || f >= e || *f != ')') return eon_cbad;
				*pp = f + 1;
				return v;
			}
			c++;
		}
		if (d != 0) return eon_cbad;
		v.len = true;
		*pp = c;
		return v;
	}
	if (*p == '-' && p + 1 < e && !isdigit((unsigned char)p[1]) && p[1] != '.') {
		p++;
		v = eon_cprimary(&p, e, depth + 1);
		*pp = p;
		return eon_cscale(v, -1);
	}
	if (isdigit((unsigned char)*p) || *p == '.' ||
	    ((*p == '-' || *p == '+') && p + 1 < e && (isdigit((unsigned char)p[1]) || p[1] == '.'))) {
		char num[48], *q;
		const char *u;
		size_t k = 0, ul;
		double x;
		while (p < e && k + 1 < sizeof(num) &&
		       (isdigit((unsigned char)*p) || *p == '.' || ((*p == '-' || *p == '+') && k == 0) ||
			((*p == 'e' || *p == 'E') && k > 0 && p + 1 < e && (isdigit((unsigned char)p[1]) || p[1] == '-' || p[1] == '+')))) {
			if ((*p == 'e' || *p == 'E') && p + 1 < e && (p[1] == '-' || p[1] == '+')) num[k++] = *p++;
			num[k++] = *p++;
		}
		num[k] = '\0';
		x = strtod(num, &q);
		if (q == num) return eon_cbad;
		u = p;
		while (p < e && (isalpha((unsigned char)*p) || *p == '%')) p++;
		ul = (size_t)(p - u);
#define EON_U(lit) (ul == strlen(lit) && strncasecmp(u, lit, ul) == 0)
		if (ul == 0) v.num = x;
		else {
			v.len = true;
			if (EON_U("px")) v.px = x;
			else if (EON_U("%")) v.pct = x;
			else if (EON_U("rem")) v.px = x * 16;
			else if (EON_U("em")) v.em = x;
			else if (EON_U("vw") || EON_U("dvw") || EON_U("svw") || EON_U("lvw") || EON_U("vmax")) v.px = x * 12.8;
			else if (EON_U("vh") || EON_U("dvh") || EON_U("svh") || EON_U("lvh") || EON_U("vmin")) v.px = x * 7.2;
			else if (EON_U("pt")) v.px = x * 4.0 / 3.0;
			else if (EON_U("pc")) v.px = x * 16;
			else if (EON_U("in")) v.px = x * 96;
			else if (EON_U("cm")) v.px = x * 37.8;
			else if (EON_U("mm")) v.px = x * 3.78;
			else if (EON_U("ch") || EON_U("ex")) v.px = x * 8;
			/* unidades de contenedor: como % del bloque que lo contiene */
			else if (EON_U("cqw") || EON_U("cqi") || EON_U("cqmin") || EON_U("cqmax")) v.pct = x;
			else if (EON_U("cqh") || EON_U("cqb")) v.px = x * 7.2;
			else return eon_cbad;
		}
#undef EON_U
		*pp = p;
		return v;
	}
	return eon_cbad;
}

static eon_cval eon_cterm(const char **pp, const char *e, int depth)
{
	const char *p = *pp;
	eon_cval v = eon_cprimary(&p, e, depth);
	while (v.ok) {
		const char *q = eon_cskip(p, e);
		eon_cval r;
		if (q >= e || (*q != '*' && *q != '/')) break;
		p = q + 1;
		r = eon_cprimary(&p, e, depth);
		if (!r.ok) return eon_cbad;
		if (*q == '*') {
			if (!r.len) v = eon_cscale(v, r.num);
			else if (!v.len) v = eon_cscale(r, v.num);
			else return eon_cbad;
		} else {
			if (r.len || r.num == 0) return eon_cbad;
			v = eon_cscale(v, 1.0 / r.num);
		}
	}
	*pp = p;
	return v;
}

static eon_cval eon_cexpr(const char **pp, const char *e, int depth)
{
	const char *p = *pp;
	eon_cval v = eon_cterm(&p, e, depth);
	while (v.ok) {
		const char *q = eon_cskip(p, e);
		eon_cval r;
		double k;
		if (q >= e || (*q != '+' && *q != '-')) break;
		k = *q == '-' ? -1 : 1;
		p = q + 1;
		r = eon_cterm(&p, e, depth);
		if (!r.ok) return eon_cbad;
		/* calc(0 + 5px): el 0 sin unidad vale como medida */
		if (v.len != r.len) {
			if (!v.len && v.num == 0) v.len = true;
			else if (!r.len && r.num == 0) r.len = true;
			else return eon_cbad;
		}
		v.px += k * r.px; v.pct += k * r.pct; v.em += k * r.em; v.num += k * r.num;
	}
	*pp = p;
	return v;
}

static void eon_cnum(eon_out *o, double x, const char *unit)
{
	char buf[48];
	size_t l;
	if (fabs(x) < 0.0005) x = 0;
	l = (size_t)snprintf(buf, sizeof(buf), "%.3f", x);
	while (l > 0 && buf[l - 1] == '0') buf[--l] = '\0';
	if (l > 0 && buf[l - 1] == '.') buf[--l] = '\0';
	eon_putz(o, buf);
	eon_putz(o, unit);
}

static void eon_cwrite(eon_out *o, eon_cval v)
{
	if (!v.len) { eon_cnum(o, v.num, ""); return; }
	v.px += v.num; /* un 0 sin unidad que se sumo */
	if (v.pct != 0) {
		/* % mezclado con px: los px como % de un contenedor de 1200px */
		eon_cnum(o, v.pct + (v.px + v.em * 16.0) / 12.0, "%");
	} else if (v.em != 0 && v.px == 0) {
		eon_cnum(o, v.em, "em");
	} else {
		eon_cnum(o, v.px + v.em * 16.0, "px");
	}
}

/* reemplaza calc()/min()/max()/clamp() que se puedan calcular; false si no cambio nada */
static bool eon_fold_calc(eon_out *o, eon_span v)
{
	const char *p = v.s, *e = v.s + v.n, *last = v.s;
	bool changed = false;
	while (p < e) {
		size_t fl = 0;
		if ((p == v.s || !(isalnum((unsigned char)p[-1]) || p[-1] == '-' || p[-1] == '_'))) {
			if (e - p > 5 && strncasecmp(p, "calc(", 5) == 0) fl = 5;
			else if (e - p > 4 && (strncasecmp(p, "min(", 4) == 0 || strncasecmp(p, "max(", 4) == 0)) fl = 4;
			else if (e - p > 6 && strncasecmp(p, "clamp(", 6) == 0) fl = 6;
			else if (e - p > 4 && strncasecmp(p, "env(", 4) == 0) fl = 4;
		}
		if (fl) {
			const char *q = p;
			eon_cval c = eon_cprimary(&q, e, 0);
			if (c.ok) {
				eon_put(o, last, (size_t)(p - last));
				eon_cwrite(o, c);
				p = last = q;
				changed = true;
				continue;
			}
		}
		if (e - p > 4 && strncasecmp(p, "url(", 4) == 0) {
			const char *c = memchr(p, ')', (size_t)(e - p));
			p = c != NULL ? c + 1 : e;
			continue;
		}
		/* unidades nuevas sueltas: 100dvh -> 100vh, 50cqw -> 50% */
		if ((isdigit((unsigned char)*p) || *p == '.') &&
		    (p == v.s || !(isalnum((unsigned char)p[-1]) || p[-1] == '_' || p[-1] == '#' || p[-1] == '.'))) {
			const char *u = p, *ue;
			while (u < e && (isdigit((unsigned char)*u) || *u == '.')) u++;
			ue = u;
			while (ue < e && isalpha((unsigned char)*ue)) ue++;
			if (ue - u == 3 && (ue == e || !isalnum((unsigned char)*ue))) {
				const char *rep = NULL;
				if (strncasecmp(u, "dvh", 3) == 0 || strncasecmp(u, "svh", 3) == 0 || strncasecmp(u, "lvh", 3) == 0) rep = "vh";
				else if (strncasecmp(u, "dvw", 3) == 0 || strncasecmp(u, "svw", 3) == 0 || strncasecmp(u, "lvw", 3) == 0) rep = "vw";
				else if (strncasecmp(u, "cqw", 3) == 0 || strncasecmp(u, "cqi", 3) == 0) rep = "%";
				else if (strncasecmp(u, "cqh", 3) == 0 || strncasecmp(u, "cqb", 3) == 0) rep = "vh";
				if (rep != NULL) {
					eon_put(o, last, (size_t)(u - last));
					eon_putz(o, rep);
					p = last = ue;
					changed = true;
					continue;
				}
			}
			p = ue > p ? ue : p + 1;
			continue;
		}
		p++;
	}
	if (changed) eon_put(o, last, (size_t)(e - last));
	return changed;
}

/* ---------------------------------------------------------------- propiedades logicas */

/* padding-inline, margin-block-start, border-inline-end-color... -> fisicas
 * (todo de izquierda a derecha y de arriba a abajo) */
static bool eon_logical(eon_out *o, eon_span prop, eon_span val, eon_span imp)
{
	static const struct { const char *l, *a, *b; } two[] = {
		{ "padding-inline", "padding-left", "padding-right" },
		{ "padding-block", "padding-top", "padding-bottom" },
		{ "margin-inline", "margin-left", "margin-right" },
		{ "margin-block", "margin-top", "margin-bottom" },
		{ "border-inline", "border-left", "border-right" },
		{ "border-block", "border-top", "border-bottom" },
	};
	static const struct { const char *l, *p; } one[] = {
		{ "inline-size", "width" }, { "block-size", "height" },
		{ "min-inline-size", "min-width" }, { "max-inline-size", "max-width" },
		{ "min-block-size", "min-height" }, { "max-block-size", "max-height" },
	};
	char name[64];
	size_t i;
	for (i = 0; i < sizeof(one) / sizeof(one[0]); i++) {
		if (eon_span_is(prop, one[i].l)) { eon_decl(o, one[i].p, val, imp); return true; }
	}
	for (i = 0; i < sizeof(two) / sizeof(two[0]); i++) {
		size_t ll = strlen(two[i].l);
		const char *rest;
		bool border = two[i].l[0] == 'b';
		if (prop.n < ll || strncasecmp(prop.s, two[i].l, ll) != 0) continue;
		rest = prop.s + ll;
		if (prop.n == ll) {
			if (border) {
				/* border-inline: 1px solid -> los dos lados igual */
				eon_decl(o, two[i].a, val, imp);
				eon_decl(o, two[i].b, val, imp);
			} else {
				eon_span w[2];
				int n = eon_words(val, w, 2);
				if (n < 1) return false;
				eon_decl(o, two[i].a, w[0], imp);
				eon_decl(o, two[i].b, n > 1 ? w[1] : w[0], imp);
			}
			return true;
		}
		if (*rest != '-') continue;
		{
			eon_span r = { rest, prop.n - ll };
			if (eon_span_is(r, "-start") || eon_span_is(r, "-end")) {
				eon_decl(o, r.n == 6 ? two[i].a : two[i].b, val, imp);
				return true;
			}
			if (border && (eon_span_is(r, "-width") || eon_span_is(r, "-color") || eon_span_is(r, "-style"))) {
				snprintf(name, sizeof(name), "%s%.*s", two[i].a, (int)r.n, r.s);
				eon_decl(o, name, val, imp);
				snprintf(name, sizeof(name), "%s%.*s", two[i].b, (int)r.n, r.s);
				eon_decl(o, name, val, imp);
				return true;
			}
			if (border && r.n > 7 && (strncasecmp(r.s, "-start-", 7) == 0 || strncasecmp(r.s, "-end-", 5) == 0)) {
				bool start = strncasecmp(r.s, "-start-", 7) == 0;
				const char *suf = r.s + (start ? 6 : 4);
				snprintf(name, sizeof(name), "%s%.*s", start ? two[i].a : two[i].b,
					 (int)(r.s + r.n - suf), suf);
				eon_decl(o, name, val, imp);
				return true;
			}
		}
	}
	return false;
}

/* valores de alineacion modernos que libcss no conoce */
static eon_span eon_align_value(eon_span prop, eon_span v)
{
	bool justify = eon_span_is(prop, "justify-content");
	if (eon_span_is(v, "start") || eon_span_is(v, "self-start") || (justify && eon_span_is(v, "left")))
		return (eon_span){ "flex-start", 10 };
	if (eon_span_is(v, "end") || eon_span_is(v, "self-end") || (justify && eon_span_is(v, "right")))
		return (eon_span){ "flex-end", 8 };
	if (eon_span_is(v, "normal"))
		return justify ? (eon_span){ "flex-start", 10 } : (eon_span){ "stretch", 7 };
	if (v.n > 5 && strncasecmp(v.s, "safe ", 5) == 0) return eon_trim(v.s + 5, v.s + v.n);
	if (v.n > 7 && strncasecmp(v.s, "unsafe ", 7) == 0) return eon_trim(v.s + 7, v.s + v.n);
	return v;
}

/* reescribe un bloque de declaraciones; extra recibe reglas nuevas para los hijos */
/* la declaracion como venia, o con el valor nuevo si se calculo algun calc() */
static void eon_orig(eon_out *o, const char *ds, const char *de, bool changed, eon_span prop, eon_span val, eon_span imp)
{
	if (!changed) {
		eon_put(o, ds, (size_t)(de - ds));
		eon_put(o, ";", 1);
		return;
	}
	eon_putspan(o, prop);
	eon_put(o, ":", 1);
	eon_putspan(o, val);
	if (imp.n) { eon_put(o, " ", 1); eon_putspan(o, imp); }
	eon_put(o, ";", 1);
}

static void eon_modern_block(eon_out *o, eon_out *extra, eon_span sel, const char *p, const char *end)
{
	bool is_grid = false;
	bool have_tracks = false;
	eon_tracks tracks;
	char colgap[80] = "";
	char rowgap[80] = "";
	eon_blockinfo bi;
	eon_out folded = { NULL, 0, 0, true };
	bool changed = false;

	eon_block_scan(p, end, &bi);

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
		/* calc() & cia ya calculados: desde aca se usa el valor nuevo */
		folded.len = 0;
		changed = false;
		if (val.n && (memchr(val.s, '(', val.n) != NULL || eon_memmem(val.s, val.n, "vh", 2) != NULL ||
			      eon_memmem(val.s, val.n, "vw", 2) != NULL || eon_memmem(val.s, val.n, "cq", 2) != NULL) &&
		    eon_fold_calc(&folded, val) && folded.ok) {
			val.s = folded.s;
			val.n = folded.len;
			changed = true;
		}

		if ((eon_span_is(prop, "mask-image") || eon_span_is(prop, "-webkit-mask-image") ||
		     eon_span_is(prop, "mask") || eon_span_is(prop, "-webkit-mask")) &&
		    eon_memmem(val.s, val.n, "url(", 4) != NULL) {
			/* NetSurf no tiene mascaras: el icono de la mascara se usa como fondo
			 * (si no, se ve un cuadrado del color de fondo) */
			const char *u = eon_memmem(val.s, val.n, "url(", 4), *ue = u;
			int d = 0;
			for (; ue < val.s + val.n; ue++) {
				if (*ue == '(') d++;
				else if (*ue == ')' && --d == 0) { ue++; break; }
			}
			eon_put(o, "background-image:", 17);
			eon_put(o, u, (size_t)(ue - u));
			eon_putz(o, ";background-repeat:no-repeat;background-position:center;background-size:contain;");
		} else if (bi.has_mask && (eon_span_is(prop, "background-color") || eon_span_is(prop, "background"))) {
			/* el color de fondo era el "color del icono": se saca */
			eon_putz(o, "background-color:transparent;");
		} else if (eon_span_is(prop, "transform") && bi.has_translate) {
			/* se deja el original (no molesta) y se agrega el equivalente */
			eon_orig(o, ds, de, changed, prop, val, imp);
			eon_emit_translate(o, &bi, imp);
		} else if (eon_span_is(prop, "left") && eon_center_x(&bi)) {
			eon_putz(o, "left:0;");
		} else if (eon_span_is(prop, "top") && eon_center_y(&bi)) {
			/* lo pone eon_emit_translate */
		} else if (eon_span_is(prop, "opacity") && strtod(val.s, NULL) <= 0.05 &&
			   val.n && (isdigit((unsigned char)val.s[0]) || val.s[0] == '.')) {
			/* NetSurf no tiene opacidad: algo invisible se veria (y las capas
			 * de pantalla completa con opacity:0 taparian la pagina) */
			eon_orig(o, ds, de, changed, prop, val, imp);
			eon_decl(o, "visibility", (eon_span){ "hidden", 6 }, imp);
		} else if (eon_span_is(prop, "opacity") && strtod(val.s, NULL) >= 0.5) {
			/* volver a mostrar lo que otra regla escondio con opacity:0 */
			eon_orig(o, ds, de, changed, prop, val, imp);
			eon_decl(o, "visibility", (eon_span){ "visible", 7 }, imp);
		} else if (eon_span_is(prop, "transform") && (eon_memmem(val.s, val.n, "scale(0)", 8) ||
			   eon_memmem(val.s, val.n, "scale(0,", 8) || eon_memmem(val.s, val.n, "scaleY(0)", 9) ||
			   eon_memmem(val.s, val.n, "scaleX(0)", 9))) {
			/* encogido a cero = escondido */
			eon_orig(o, ds, de, changed, prop, val, imp);
			eon_decl(o, "visibility", (eon_span){ "hidden", 6 }, imp);
		} else if (eon_span_is(prop, "aspect-ratio")) {
			/* libcss no lo tiene: va como column-count (ver patch-flex.py) */
			eon_span w[3];
			int n = eon_words(val, w, 3), i;
			double a = 0, b = 1;
			char buf[48];
			const char *sl;
			eon_orig(o, ds, de, changed, prop, val, imp);
			for (i = 0; i < n; i++) {
				if (eon_span_is(w[i], "auto")) continue;
				a = strtod(w[i].s, NULL);
				sl = memchr(w[i].s, '/', w[i].n);
				if (sl != NULL) b = strtod(sl + 1, NULL);
				else if (i + 2 < n && eon_span_is(w[i + 1], "/")) b = strtod(w[i + 2].s, NULL);
				break;
			}
			if (a > 0 && b > 0 && a / b > 0.01 && a / b < 100) {
				snprintf(buf, sizeof(buf), "column-count:%d;", 100000 + (int)(a / b * 1000 + 0.5));
				eon_putz(o, buf);
			}
		} else if (eon_span_is(prop, "position") && eon_span_is(val, "sticky")) {
			/* sticky: lo mas parecido sin scroll ligado es relative */
			eon_decl(o, "position", (eon_span){ "relative", 8 }, imp);
		} else if (eon_span_is(prop, "inset")) {
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
				/* el flex de EonSurf usa column-gap en los dos ejes */
				eon_span cg = n > 1 ? w[1] : w[0];
				eon_decl(o, "column-gap", cg, imp);
			}
		} else if (eon_span_is(prop, "row-gap") || eon_span_is(prop, "grid-row-gap")) {
			snprintf(rowgap, sizeof(rowgap), "%.*s", (int)val.n, val.s);
		} else if (eon_span_is(prop, "grid-column-gap") || eon_span_is(prop, "column-gap")) {
			eon_decl(o, "column-gap", val, imp);
			snprintf(colgap, sizeof(colgap), "%.*s", (int)val.n, val.s);
		} else if (eon_span_is(prop, "place-items")) {
			eon_span w[2];
			if (eon_words(val, w, 2) >= 1) eon_decl(o, "align-items", eon_align_value(prop, w[0]), imp);
		} else if (eon_span_is(prop, "place-content")) {
			eon_span w[2];
			int n = eon_words(val, w, 2);
			if (n >= 1) {
				eon_decl(o, "align-content", eon_align_value(prop, w[0]), imp);
				eon_decl(o, "justify-content", eon_align_value((eon_span){ "justify-content", 15 }, n > 1 ? w[1] : w[0]), imp);
			}
		} else if (eon_span_is(prop, "place-self")) {
			eon_span w[2];
			if (eon_words(val, w, 2) >= 1) eon_decl(o, "align-self", eon_align_value(prop, w[0]), imp);
		} else if (eon_span_is(prop, "justify-content") || eon_span_is(prop, "align-items") ||
			   eon_span_is(prop, "align-self") || eon_span_is(prop, "align-content")) {
			char pn[24];
			snprintf(pn, sizeof(pn), "%.*s", (int)prop.n, prop.s);
			eon_decl(o, pn, eon_align_value(prop, val), imp);
		} else if (eon_span_is(prop, "text-align") && (eon_span_is(val, "start") || eon_span_is(val, "end") ||
			   eon_span_is(val, "match-parent"))) {
			eon_decl(o, "text-align", eon_span_is(val, "end") ? (eon_span){ "right", 5 } : (eon_span){ "left", 4 }, imp);
		} else if (eon_logical(o, prop, val, imp)) {
			/* ya escrita */
		} else if (changed) {
			eon_orig(o, ds, de, changed, prop, val, imp);
		} else {
			eon_put(o, ds, (size_t)(de - ds));
			if (de < end) eon_put(o, ";", 1);
		}
	}
	free(folded.s);

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

/* ---------------------------------------------------------------- CSS 2024+ (Tailwind 4 y cia) */

/* media queries con rangos: (width >= 40rem) -> (min-width: 40rem). Tambien
 * sin espacios, como sale de Tailwind 4: (width>=512px), y "not all and (...)"
 * de una sola condicion, que se invierte. */
#define EON_IS_DIM(t) (eon_span_is(t, "width") || eon_span_is(t, "height"))
#define EON_GE(t) (eon_span_is(t, ">=") || eon_span_is(t, ">"))
#define EON_LE(t) (eon_span_is(t, "<=") || eon_span_is(t, "<"))

static int eon_range_tokens(const char *p, const char *e, eon_span *tok, int max)
{
	int n = 0;
	for (;;) {
		const char *s;
		while (p < e && isspace((unsigned char)*p)) p++;
		if (p >= e) return n;
		if (n >= max) return -1;
		s = p;
		if (*p == '<' || *p == '>' || *p == '=') {
			while (p < e && (*p == '<' || *p == '>' || *p == '=')) p++;
		} else {
			while (p < e && !isspace((unsigned char)*p) && *p != '<' && *p != '>' && *p != '=') p++;
		}
		tok[n].s = s;
		tok[n].n = (size_t)(p - s);
		n++;
	}
}

/* (min-|max-)dim: val; con < o > estricto se corre un poquito */
static void eon_range_emit(eon_out *o, eon_span dim, eon_span val, bool ge, bool strict)
{
	eon_putz(o, ge ? "(min-" : "(max-");
	eon_putspan(o, dim);
	eon_putz(o, ": ");
	if (strict) {
		char num[48], *u, buf[80];
		double v;
		snprintf(num, sizeof(num), "%.*s", (int)val.n, val.s);
		v = strtod(num, &u);
		if (u != num) {
			snprintf(buf, sizeof(buf), "%g%s", ge ? v + 0.02 : v - 0.02, u);
			eon_putz(o, buf);
		} else {
			eon_putspan(o, val);
		}
	} else {
		eon_putspan(o, val);
	}
	eon_putz(o, ")");
}

/* escribe la condicion traducida; false (sin escribir nada) si no es de rango */
static bool eon_range_cond(eon_out *o, const char *p, const char *e, bool neg)
{
	eon_span t[5];
	int n = eon_range_tokens(p, e, t, 5);
	if (n == 3 && (EON_GE(t[1]) || EON_LE(t[1])) && (EON_IS_DIM(t[0]) || EON_IS_DIM(t[2]))) {
		bool left = EON_IS_DIM(t[0]);
		bool ge = EON_GE(t[1]);
		bool strict = t[1].n == 1;
		if (!left) ge = !ge;
		if (neg) { ge = !ge; strict = !strict; }
		eon_range_emit(o, left ? t[0] : t[2], left ? t[2] : t[0], ge, strict);
		return true;
	}
	if (n == 5 && !neg && EON_IS_DIM(t[2]) &&
	    ((EON_LE(t[1]) && EON_LE(t[3])) || (EON_GE(t[1]) && EON_GE(t[3])))) {
		bool up = EON_LE(t[1]);
		eon_range_emit(o, t[2], t[0], up, t[1].n == 1);
		eon_putz(o, " and ");
		eon_range_emit(o, t[2], t[4], !up, t[3].n == 1);
		return true;
	}
	return false;
}

static void eon_media_header(eon_out *o, const char *p, const char *end)
{
	eon_span h = eon_trim(p + 6, end); /* despues de "@media" */
	p = h.s;
	end = h.s + h.n;
	eon_putz(o, "@media ");
	if (h.n > 12 && strncasecmp(p, "not all and", 11) == 0) {
		eon_span r = eon_trim(p + 11, end);
		const char *close = (r.n && r.s[0] == '(') ? memchr(r.s, ')', r.n) : NULL;
		if (close != NULL && eon_trim(close + 1, end).n == 0) {
			eon_out tmp = { NULL, 0, 0, true };
			if (eon_range_cond(&tmp, r.s + 1, close, true) && tmp.ok) {
				eon_put(o, tmp.s, tmp.len);
				free(tmp.s);
				return;
			}
			free(tmp.s);
		}
	}
	while (p < end) {
		const char *open = memchr(p, '(', (size_t)(end - p)), *close;
		if (open == NULL) { eon_put(o, p, (size_t)(end - p)); return; }
		close = memchr(open, ')', (size_t)(end - open));
		if (close == NULL) { eon_put(o, p, (size_t)(end - p)); return; }
		eon_put(o, p, (size_t)(open - p));
		if (!eon_range_cond(o, open + 1, close, false))
			eon_put(o, open, (size_t)(close - open) + 1);
		p = close + 1;
	}
}
#undef EON_IS_DIM
#undef EON_GE
#undef EON_LE

/* selectores: :where(X) / :is(X) con un solo selector -> X (libcss no los tiene) */
static void eon_selector(eon_out *o, const char *p, const char *end)
{
	while (p < end) {
		size_t skip = 0;
		if (end - p > 7 && strncmp(p, ":where(", 7) == 0) skip = 7;
		else if (end - p > 4 && strncmp(p, ":is(", 4) == 0) skip = 4;
		else if (end - p > 5 && strncmp(p, ":any(", 5) == 0) skip = 5;
		if (skip) {
			const char *a = p + skip, *c = a;
			int d = 1;
			bool comma = false;
			while (c < end && d > 0) {
				if (*c == '(') d++;
				else if (*c == ')') d--;
				else if (*c == ',' && d == 1) comma = true;
				if (d > 0) c++;
			}
			if (c < end && !comma) {
				eon_put(o, a, (size_t)(c - a));
				p = c + 1;
				continue;
			}
		}
		eon_put(o, p, 1);
		p++;
	}
}

/* oklch(L C H [/ A]) -> rgb()/rgba(): matematica de Oklab (Bjorn Ottosson) */
static double eon_srgb_gamma(double c)
{
	c = c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
	return c < 0 ? 0 : c > 1 ? 1 : c;
}
static bool eon_oklch_one(const char *a, const char *e, char *buf, size_t bn)
{
	double v[4] = { 0, 0, 0, 1 };
	int i = 0;
	const char *p = a;
	while (p < e && i < 4) {
		char *q;
		while (p < e && (isspace((unsigned char)*p) || *p == ',' || *p == '/')) {
			if (*p == '/') i = 3;
			p++;
		}
		if (p >= e) break;
		if (strncmp(p, "none", 4) == 0) { v[i++] = 0; p += 4; continue; }
		v[i] = strtod(p, &q);
		if (q == p) return false;
		if (q < e && *q == '%') { v[i] = (i == 0 || i == 3) ? v[i] / 100.0 : v[i] * 0.004; q++; }
		while (q < e && isalpha((unsigned char)*q)) q++; /* deg */
		p = q;
		i++;
	}
	{
		double L = v[0], C = v[1], H = v[2] * 3.14159265358979 / 180.0;
		double A = C * cos(H), B = C * sin(H);
		double l = L + 0.3963377774 * A + 0.2158037573 * B;
		double m = L - 0.1055613458 * A - 0.0638541728 * B;
		double s3 = L - 0.0894841775 * A - 1.2914855480 * B;
		double r, g, b;
		l = l * l * l; m = m * m * m; s3 = s3 * s3 * s3;
		r = eon_srgb_gamma(4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s3);
		g = eon_srgb_gamma(-1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s3);
		b = eon_srgb_gamma(-0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s3);
		if (v[3] < 1)
			snprintf(buf, bn, "rgba(%d,%d,%d,%.3f)", (int)(r * 255 + .5), (int)(g * 255 + .5), (int)(b * 255 + .5), v[3]);
		else
			snprintf(buf, bn, "rgb(%d,%d,%d)", (int)(r * 255 + .5), (int)(g * 255 + .5), (int)(b * 255 + .5));
	}
	return true;
}
static char *eon_oklch_pass(const char *css, size_t len, size_t *outlen)
{
	eon_out o = { NULL, 0, 0, true };
	const char *p = css, *end = css + len, *hit;
	if (eon_memmem(css, len, "oklch(", 6) == NULL) return NULL;
	while ((hit = eon_memmem(p, (size_t)(end - p), "oklch(", 6)) != NULL) {
		const char *a = hit + 6, *c = memchr(a, ')', (size_t)(end - a));
		char buf[64];
		if (c == NULL) break;
		/* oklch(from ...) o con var() adentro: dejarlo como esta */
		if (eon_memmem(a, (size_t)(c - a), "(", 1) == NULL && eon_memmem(a, (size_t)(c - a), "from", 4) == NULL &&
		    eon_oklch_one(a, c, buf, sizeof(buf))) {
			eon_put(&o, p, (size_t)(hit - p));
			eon_putz(&o, buf);
		} else {
			eon_put(&o, p, (size_t)(c + 1 - p));
		}
		p = c + 1;
	}
	eon_put(&o, p, (size_t)(end - p));
	if (!o.ok) { free(o.s); return NULL; }
	*outlen = o.len;
	return o.s;
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
			if (name.n >= 6 && strncasecmp(name.s, "@layer", 6) == 0) {
				/* libcss no conoce @layer y tiraba todo lo de adentro (todas las
				 * utilidades de Tailwind 4): se desenvuelve */
				p = eon_modern_rules(o, brace + 1, end, true);
				continue;
			}
			if (name.n >= 6 && strncasecmp(name.s, "@media", 6) == 0) {
				eon_media_header(o, p, brace);
				eon_put(o, "{", 1);
			} else {
				eon_put(o, p, (size_t)(brace - p) + 1);
			}
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
			eon_selector(o, p, brace);
			eon_put(o, "{", 1);
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
	    eon_memmem(css, len, "gap", 3) == NULL && eon_memmem(css, len, "place-", 6) == NULL &&
	    eon_memmem(css, len, "translate", 9) == NULL && eon_memmem(css, len, "sticky", 6) == NULL &&
	    eon_memmem(css, len, "opacity", 7) == NULL && eon_memmem(css, len, "scale", 5) == NULL &&
	    eon_memmem(css, len, "mask", 4) == NULL && eon_memmem(css, len, "@layer", 6) == NULL &&
	    eon_memmem(css, len, "oklch(", 6) == NULL && eon_memmem(css, len, ":where(", 7) == NULL &&
	    eon_memmem(css, len, ":is(", 4) == NULL && eon_memmem(css, len, "width >", 7) == NULL &&
	    eon_memmem(css, len, "width <", 7) == NULL && eon_memmem(css, len, "width>", 6) == NULL &&
	    eon_memmem(css, len, "width<", 6) == NULL && eon_memmem(css, len, "calc(", 5) == NULL &&
	    eon_memmem(css, len, "min(", 4) == NULL && eon_memmem(css, len, "max(", 4) == NULL &&
	    eon_memmem(css, len, "clamp(", 6) == NULL && eon_memmem(css, len, "-inline", 7) == NULL &&
	    eon_memmem(css, len, "-block", 6) == NULL && eon_memmem(css, len, "-size", 5) == NULL &&
	    eon_memmem(css, len, "justify-", 8) == NULL && eon_memmem(css, len, "align-", 6) == NULL &&
	    eon_memmem(css, len, "text-align", 10) == NULL && eon_memmem(css, len, "aspect-ratio", 12) == NULL &&
	    eon_memmem(css, len, "dvh", 3) == NULL && eon_memmem(css, len, "svh", 3) == NULL && eon_memmem(css, len, "cqw", 3) == NULL) {
		return NULL;
	}
	eon_modern_rules(&o, css, css + len, false);
	if (!o.ok) {
		free(o.s);
		return NULL;
	}
	{
		size_t l2 = 0;
		char *k = eon_oklch_pass(o.s, o.len, &l2);
		if (k != NULL) { free(o.s); o.s = k; o.len = l2; }
	}
	*outlen = o.len;
	return o.s;
}
