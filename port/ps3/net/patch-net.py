#!/usr/bin/env python3
"""Red mas rapida para la PS3 (lo llama tweak-sources.sh desde netsurf-all-3.11).

Medido en RPCS3 (MercadoLibre): los archivos llegaban de a uno cada ~1,5 s
porque cada conexion nueva hace el saludo TLS completo, muy pesado para el PPU.
 - mas conexiones por servidor y mas handles guardados para reutilizar
   conexiones ya abiertas;
 - cache de DNS compartida por todas las descargas (CURLSH en el molde).
MercadoLibre: de 74 s a 44 s. Compartir tambien las sesiones TLS
(CURL_LOCK_DATA_SSL_SESSION) lo empeoraba con libcurl 7.64 + mbedTLS: no usar.
"""

NS = 'netsurf/'


def patch(path, old, new):
    s = open(path).read()
    assert old in s, (path, old[:60])
    open(path, 'w').write(s.replace(old, new, 1))


patch(NS + 'content/fetchers/curl.c',
      '\tcode = curl_easy_setopt(fetch_blank_curl, CURLOPT_SSL_CTX_FUNCTION, NULL);',
      '''\t/* EonSurf: cache de DNS compartida por todas las descargas */
\t{
\t\tstatic CURLSH *eon_share = NULL;
\t\tif (eon_share == NULL) {
\t\t\teon_share = curl_share_init();
\t\t\tif (eon_share != NULL) {
\t\t\t\tcurl_share_setopt(eon_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
\t\t\t}
\t\t}
\t\tif (eon_share != NULL) {
\t\t\tcurl_easy_setopt(fetch_blank_curl, CURLOPT_SHARE, eon_share);
\t\t}
\t}

\tcode = curl_easy_setopt(fetch_blank_curl, CURLOPT_SSL_CTX_FUNCTION, NULL);''')

patch(NS + 'desktop/options.h', 'NSOPTION_INTEGER(max_fetchers_per_host, 5)',
      'NSOPTION_INTEGER(max_fetchers_per_host, 8)')
patch(NS + 'desktop/options.h', 'NSOPTION_INTEGER(max_cached_fetch_handles, 6)',
      'NSOPTION_INTEGER(max_cached_fetch_handles, 16)')
print('EonSurf: red mas rapida')
