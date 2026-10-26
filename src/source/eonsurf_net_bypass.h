#ifndef EONSURF_NET_BYPASS_H
#define EONSURF_NET_BYPASS_H

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

/* ============================================================================
 * 1. PÁGINA INTERNA "eonsurf://portal" (CERO RED, CARGA INSTANTÁNEA)
 * ============================================================================ */

static const char *EONSURF_PORTAL_MARKDOWN =
    "# EonSurf Search & Web Portal\n\n"
    "Bienvenido al navegador web para PlayStation 3. Presiona **TRIANGULO** o haz clic en la barra superior para escribir una direccion o buscar en la Web.\n\n"
    "## Buscadores y Portales Web (Presiona X para navegar)\n"
    " - [Buscar en Google (DuckDuckGo Lite)](https://lite.duckduckgo.com/lite/?q=playstation+3)\n"
    " - [Wikipedia en Espanol - Portada](https://es.wikipedia.org/wiki/Wikipedia:Portada)\n"
    " - [Buscar en Wikipedia: PlayStation 3](https://es.wikipedia.org/w/index.php?search=playstation+3)\n"
    " - [Buscar en Wikipedia: Videojuegos](https://es.wikipedia.org/wiki/Videojuego)\n"
    " - [Hacker News (Noticias de Tecnologia)](https://news.ycombinator.com)\n\n"
    "## Scene PlayStation 3 & Homebrew\n"
    " - [webMAN-MOD Oficial (Guia y Manual)](https://raw.githubusercontent.com/aldostools/webMAN-MOD/master/README.md)\n"
    " - [SDK Abierto PSL1GHT](https://raw.githubusercontent.com/ps3dev/PSL1GHT/master/README.md)\n"
    " - [Explorador Local HDD (/dev_hdd0/)](file:///dev_hdd0/)\n\n"
    "## Controles del Navegador\n"
    " - **Triangulo**: Abrir Teclado en Pantalla para escribir cualquier direccion o busqueda.\n"
    " - **L1 / R1**: Cambiar entre pestanas abiertas.\n"
    " - **Stick Derecho / L2 / R2**: Desplazamiento vertical (Scroll fluido).\n"
    " - **Cruz (X)**: Hacer clic en enlaces, pestanas o arrastrar la barra de scroll.\n"
    " - **Circulo (O)**: Volver a la pagina anterior (Atras).";

/* 
 * Corrige eonsurf_build_search_url:
 * Respeta el protocolo interno "eonsurf://" para NO mandarlo a DuckDuckGo.
 */
static inline void eonsurf_build_search_url_fixed(const char *user_input, char *out_url, size_t max_len) {
    if (strncmp(user_input, "http://", 7) == 0 ||
        strncmp(user_input, "https://", 8) == 0 ||
        strncmp(user_input, "file://", 7) == 0 ||
        strncmp(user_input, "eonsurf://", 10) == 0) {
        strncpy(out_url, user_input, max_len - 1);
        out_url[max_len - 1] = '\0';
        return;
    }

    char encoded[192];
    size_t di = 0;
    for (size_t i = 0; user_input[i] != '\0' && di + 4 < sizeof(encoded); i++) {
        if (user_input[i] == ' ') {
            encoded[di++] = '+';
        } else if ((unsigned char)user_input[i] > 32 && user_input[i] != '"' && user_input[i] != '&') {
            encoded[di++] = user_input[i];
        }
    }
    encoded[di] = '\0';

    /* Usamos lite.duckduckgo.com o Wikipedia Search que son mucho más amigables que html.duckduckgo */
    snprintf(out_url, max_len, "https://lite.duckduckgo.com/lite/?q=%s", encoded);
}

/* ============================================================================
 * 2. CAMUFLAJE ANTI-CAPTCHA PARA LIBCURL (HEADERS DE FIREFOX REAL)
 * ============================================================================ */

/* 
 * Llamá a esta función antes de curl_easy_perform(curl).
 * Devuelve la lista de cabeceras (que después liberás con curl_slist_free_all).
 */
static inline struct curl_slist* eonsurf_apply_stealth_browser_headers(CURL *curl) {
    /* User-Agent idéntico a Mozilla Firefox de escritorio moderno */
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:131.0) Gecko/20100101 Firefox/131.0");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: es-AR,es;q=0.9,en-US;q=0.8,en;q=0.7");
    headers = curl_slist_append(headers, "Referer: https://lite.duckduckgo.com/");
    headers = curl_slist_append(headers, "Upgrade-Insecure-Requests: 1");
    headers = curl_slist_append(headers, "Sec-Fetch-Dest: document");
    headers = curl_slist_append(headers, "Sec-Fetch-Mode: navigate");
    headers = curl_slist_append(headers, "Sec-Fetch-Site: same-origin");
    headers = curl_slist_append(headers, "Sec-Fetch-User: ?1");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    return headers;
}

/* ============================================================================
 * 3. DETECTOR DE CAPTCHA Y FALLBACK AUTOMÁTICO SIN BLOQUEOS
 * ============================================================================ */

/* Detecta si DuckDuckGo devolvió el desafío de los patitos (HTTP 202 / Code 4458) */
static inline int eonsurf_is_captcha_blocked(int http_code, const char *raw_html) {
    if (!raw_html) return 0;
    if (http_code == 202 || http_code == 403 || http_code == 429) return 1;
    if (strstr(raw_html, "bots use DuckDuckGo") != NULL) return 1;
    if (strstr(raw_html, "squares containing a duck") != NULL) return 1;
    return 0;
}

/* 
 * Si DuckDuckGo bloquea por IP con CAPTCHA, redirige la búsqueda en 0.1s
 * al buscador HTML limpio de Wikipedia en Español (0% CAPTCHAs garantizado).
 */
static inline void eonsurf_build_fallback_wiki_search(const char *original_query, char *out_url, size_t max_len) {
    /* Si venía de ".../?q=termino", extraemos solo el término */
    const char *q = strstr(original_query, "?q=");
    const char *term = q ? (q + 3) : original_query;
    snprintf(out_url, max_len,
             "https://es.wikipedia.org/w/index.php?search=%s&title=Especial:Buscar&profile=default&fulltext=1",
             term);
}

#endif /* EONSURF_NET_BYPASS_H */
