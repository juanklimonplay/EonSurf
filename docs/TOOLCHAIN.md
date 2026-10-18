# Kit de desarrollo (toolchain) de EonSurf

Compilado desde el código el 18/10/2026 en un Linux x86-64 (Ubuntu 24.04). La imagen Docker de ghcr.io
**no se pudo usar** (pide autenticación), así que se compiló todo con `tools/build-kit.sh`.

## Versiones
| Pieza | Versión |
|---|---|
| ps3toolchain | commit `de85d7d` (11/10/2026) |
| Compilador PPU (`powerpc64-ps3-elf-gcc`) | GCC 13.2.0 + newlib 1.20.0 |
| binutils PPU | 2.42 |
| Compilador SPU | GCC 9.5.0 + binutils 2.22 |
| gdb | 8.3.1 |
| PSL1GHT | commit `f649a08` (29/06/2026) |
| ps3libraries | commit `b2efde5` (12/10/2026) |
| SDL2 (SDL_PSL1GHT, rama sdl2_master) | 2.0.13, commit `b81fbad` |
| libcurl | 7.64.1 |
| mbedTLS | 2.28.10 (solo hasta TLS 1.2) |
| FreeType | 12.1.6 (2.4.3) · libpng 1.4.4 · jpeg 8b · zlib 1.2.13 · libxml2 2.7.8 |
| pthread-emb-ps3 | commit `6431711` |

Se instala en `/usr/local/ps3dev` (`PS3DEV` y `PSL1GHT` apuntan ahí; hay que agregar `bin`, `ppu/bin` y `spu/bin` al PATH).

## Problemas encontrados y cómo se resolvieron
1. **ghcr.io** pide autenticación → se compila desde el código.
2. **FTP bloqueado:** el gcc SPU 9.5 baja gmp/mpfr/mpc/isl por `ftp://`. `tools/build-kit.sh` lo cambia a `https://`.
3. **Tarballs de github.com bloqueados** (`/tarball/master`, `/archive/...`) pero `git clone` anda. Para PSL1GHT, SDL2 y
   pthread-emb se clonó con git y se armó el tarball con `git archive` en `archives/`. En GitHub Actions normalmente
   no hace falta.
4. Faltaba `libtool-bin` en el sistema anfitrión.
5. El ps3libraries completo no se compiló: solo los pasos necesarios (zlib, libpng, jpeg, freetype, libxml2, SDL2, mbedTLS, libcurl, pthread-emb).

## Compilar el kit
`sudo -E bash tools/build-kit.sh` (1–2 horas). Después: `make -C hola pkg`.
