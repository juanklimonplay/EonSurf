/*
 * EonSurf: CSS de una hoja listo para libcss: variables resueltas
 * (eon_cssvars.c) y propiedades modernas traducidas (eon_cssmodern.c).
 */
static char *eon_css_take(const void *sheet, size_t *len)
{
	size_t mlen = 0;
	char *css = eon_css_take_vars(sheet, len);
	char *m;
	if (css == NULL) return NULL;
	NSLOG(netsurf, INFO, "EonSurf: css modernize %u bytes", (unsigned)*len);
	m = eon_css_modernize(css, *len, &mlen);
	NSLOG(netsurf, INFO, "EonSurf: css modernize done");
	if (m != NULL) {
		free(css);
		css = m;
		*len = mlen;
	}
	return css;
}
