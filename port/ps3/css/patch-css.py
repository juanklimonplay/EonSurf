#!/usr/bin/env python3
"""Variables CSS en NetSurf (lo llama tweak-sources.sh desde netsurf-all-3.11).

El CSS de cada hoja se junta completo y pasa por eon_cssvars.c, que resuelve
var(--x) antes de que libcss lo lea.
"""
import os
import shutil

CSS = 'netsurf/content/handlers/css/'
here = os.path.dirname(os.path.abspath(__file__))
shutil.copy(os.path.join(here, 'eon_cssvars.c'), CSS + 'eon_cssvars.c')


def patch(path, old, new):
    s = open(path).read()
    assert old in s, (path, old[:60])
    open(path, 'w').write(s.replace(old, new, 1))


patch(CSS + 'css.c', '#include "css/internal.h"\n',
      '#include "css/internal.h"\n#include "eon_cssvars.c" /* EonSurf: variables CSS */\n')

patch(CSS + 'css.c', '''	return css_stylesheet_append_data(c->sheet,
			(const uint8_t *) data, size);''',
      '''	/* EonSurf: se guarda todo y se resuelven las variables al final */
	return eon_css_buffer(c->sheet, data, size) ? CSS_OK : CSS_NOMEM;''')

patch(CSS + 'css.c', '''static css_error nscss_convert_css_data(struct content_css_data *c)
{
	css_error error;
''', '''static css_error nscss_convert_css_data(struct content_css_data *c)
{
	css_error error;
	size_t eon_len = 0;
	char *eon_css = eon_css_take(c->sheet, &eon_len);

	if (eon_css != NULL) {
		error = css_stylesheet_append_data(c->sheet, (const uint8_t *)eon_css, eon_len);
		free(eon_css);
		if (error != CSS_OK && error != CSS_NEEDDATA) {
			return error;
		}
	}
''')

patch(CSS + 'css.c', '''static void nscss_destroy_css_data(struct content_css_data *c)
{
	uint32_t i;
''', '''static void nscss_destroy_css_data(struct content_css_data *c)
{
	uint32_t i;

	eon_cssbuf_free(c->sheet);
''')
print('EonSurf: variables CSS')
