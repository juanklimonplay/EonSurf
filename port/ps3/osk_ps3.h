#ifndef EONSURF_OSK_H
#define EONSURF_OSK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int eonsurf_abrir_teclado(const char *titulo, const char *texto_inicial, char *resultado, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* EONSURF_OSK_H */
