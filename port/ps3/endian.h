#ifndef _ENDIAN_H
#define _ENDIAN_H 1

/*
 * EonSurf - Definición de Endianness para PlayStation 3 (Cell PPU)
 * El procesador Cell de PS3 es Big-Endian nativo.
 */

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __BYTE_ORDER    __BIG_ENDIAN
#define BYTE_ORDER      __BIG_ENDIAN
#define BIG_ENDIAN      __BIG_ENDIAN
#define LITTLE_ENDIAN   __LITTLE_ENDIAN

#define __BYTE_ORDER__  __ORDER_BIG_ENDIAN__

#endif /* _ENDIAN_H */
