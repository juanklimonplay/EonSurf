#ifndef EONSURF_PAGE_SEARCH_H
#define EONSURF_PAGE_SEARCH_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

static char g_search_query[64] = "";
static int  g_search_match_count = 0;
static int  g_search_active = 0;

/* Busca coincidencias insensibles a mayúsculas/minúsculas dentro del texto de la página */
static inline int eonsurf_find_text_matches(const char *page_body, const char *query) {
    if (!page_body || !query || strlen(query) == 0) {
        g_search_match_count = 0;
        return 0;
    }

    g_search_match_count = 0;
    size_t qlen = strlen(query);
    const char *p = page_body;

    while (*p) {
        if (strncasecmp(p, query, qlen) == 0) {
            g_search_match_count++;
            p += qlen;
        } else {
            p++;
        }
    }
    return g_search_match_count;
}

#endif /* EONSURF_PAGE_SEARCH_H */
