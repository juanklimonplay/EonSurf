# Estado de EonSurf (18/10/2026)

**Importante: nada se probó todavía en una PS3 real.** Lo que sigue se probó solo compilando y en Linux.

## Qué anda
- **Kit de PS3 compilado** (versiones en [TOOLCHAIN.md](TOOLCHAIN.md)): compilador, PSL1GHT, SDL2, libcurl, mbedTLS, FreeType, etc.
- **Hola mundo para PS3**, dos versiones (se compilan y empaquetan como `.pkg`; PARAM.SFO con título "EonSurf"; ícono provisorio):
  - `hola/` (EONSURF01, **nativa**): framebuffer de PSL1GHT (RSX) + `libio` para el mando, sin SDL. Es la versión principal desde el informe de RPCS3.
  - `hola-sdl/` (EONSURF02, "EonSurf SDL diagnostico"): la versión original con SDL2, ahora con `printf` en cada paso (salen en TTY.log).
  - Comprobado: ejecutables PowerPC de 64 bits big-endian y PARAM.SFO correcto.
- **NetSurf nativo en Linux** (`port/`): NetSurf 3.11 original (GPL v2) con su frontend framebuffer en superficie `ram`,
  y un parche propio que guarda una captura sin pantalla. Con eso se cargaron por HTTPS **Wikipedia en español** y
  **Google** (ver `docs/capturas/`). Es el motor de escritorio con JavaScript Duktape, no es todavía el programa de PS3.

## Informe de RPCS3 (usuario, 18/10/2026)
RPCS3 0.0.32-16719, firmware 4.91, Windows 11. El `hola.pkg` **SDL** se instala y arranca, inicializa video y mando, y se cae unos
10 ms después con `Access violation writing location 0x9917afc0` dentro de `SDL_FillRect4` (relleno por software de SDL),
en la primera pantalla. Mando no probado (RPCS3 sin mando).

**Qué investigué (solo leyendo código y desensamblando; no puedo correr RPCS3 acá):**
- `addr2line` sobre `hola.elf`: la dirección 0x5d690 es el inicio de `SDL_FillRect4` (SDL_fillrect.c).
- El renderer de SDL_PSL1GHT (`src/render/psl1ght/SDL_PSL1GHTrender.c`, rama sdl2_master, último commit 2020) dibuja por
  software sobre dos superficies en memoria del RSX. `SDL_RenderClear` termina en `SDL_FillRect` sobre esa superficie.
- **Bug real encontrado en SDL_PSL1GHT:** `PSL1GHT_QueueFillRects` convierte los `SDL_FRect` a `SDL_Rect` y después hace
  `SDL_memcpy(outRects, rects, outLen)`, que pisa lo convertido con los bytes crudos de los floats (valores enteros enormes).
  Eso afecta a `SDL_RenderFillRect` (mi texto), pero no explica por sí solo un fallo en el primer `SDL_RenderClear`.
- **Causa exacta: no confirmada.** Es probable que sea del port SDL_PSL1GHT 2.0.13 (código poco mantenido) y/o de su
  interacción con RPCS3. No sé si pasaría en una PS3 real.

**Qué cambié:** `hola/` ahora no usa SDL (patrón de los ejemplos de PSL1GHT), y `hola-sdl/` queda como diagnóstico.
Además agregué `SYS_PROCESS_PARAM(1001, 0x100000)` a ambas (el original no lo tenía).

**Pedido para la próxima prueba en RPCS3** (instalar los dos `.pkg` del artefacto `EonSurf-hola-mundo-pkg`):
1. `hola.pkg` (nativo, "EonSurf"): ¿dibuja el texto y el cuadrado naranja? Pegar TTY.log (líneas "EonSurf:").
2. `hola-sdl.pkg` ("EonSurf SDL diagnostico"): pegar TTY.log (líneas "EonSurf-sdl:") para ver hasta dónde llega.
Si el nativo anda y el SDL no, la etapa NetSurf usará una superficie propia de libnsfb sobre el framebuffer nativo, sin SDL_PSL1GHT.

## Cómo se probó
- Hola mundo: `make -C hola pkg` termina sin errores.
- NetSurf: `port/native/build.sh` y `port/native/shot.sh <url> salida.png`.

## Qué no se probó
- Nada en PS3 real. En RPCS3 solo lo probó el usuario con la versión SDL (se cae, ver arriba); la versión nativa nueva no se probó en ningún lado.
- El workflow de GitHub Actions (`.github/workflows/build.yml`) está escrito pero no se ejecutó.

## Próximos pasos
1. Compilar las librerías de NetSurf (libparserutils, hubbub, libcss, libdom...) con PSL1GHT.
2. Escribir la superficie de libnsfb para la PS3 (sobre el framebuffer nativo o SDL2, según el resultado de RPCS3) y los controles del DualShock 3.
3. Red con libcurl + mbedTLS y archivo de certificados incluido; cuidado con big-endian.
4. QuickJS (quickjs-ng) para Google y verificar su compilación en big-endian.
5. Idioma español/inglés, página de inicio, user agent móvil, rutas en `/dev_hdd0/game/EONSURF01/USRDIR`.

## Bloqueos y notas
- GitHub bloquea tarballs, FTP y ghcr.io en este entorno; ver TOOLCHAIN.md.
- El git de NetSurf (git.netsurf-browser.org) no es accesible; se usa el tarball oficial `netsurf-all-3.11`.
- El port de Switch no se copió: no declara licencia. El único concepto tomado es la idea de capturar sin pantalla;
  el código es propio sobre NetSurf original.
