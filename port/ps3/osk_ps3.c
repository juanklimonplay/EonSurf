/*
 * EonSurf - Interfaz de teclado nativo de PS3
 * Stub seguro y desacoplado para compatibilidad con compiladores PSL1GHT.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int eonsurf_abrir_teclado(const char *titulo, const char *texto_inicial, char *resultado, size_t max_len)
{
    (void)titulo;
    if (texto_inicial && resultado && max_len > 0) {
        strncpy(resultado, texto_inicial, max_len - 1);
        resultado[max_len - 1] = '\0';
        return 0;
    }
    return -1;
}
