# Estado de EonSurf (18/10/2026)

**Importante: nada se probó todavía en una PS3 real.** Lo que sigue se probó solo compilando y en Linux.

## Qué anda
- **Kit de PS3 compilado** (versiones en [TOOLCHAIN.md](TOOLCHAIN.md)): compilador, PSL1GHT, SDL2, libcurl, mbedTLS, FreeType, etc.
- **Hola mundo para PS3** (`hola/`): dibuja texto con SDL2 y muestra el estado del DualShock 3 (palanca izquierda y botones).
  Se compila y se empaqueta como `.pkg` (título "EonSurf" en PARAM.SFO, ícono provisorio). Comprobado: el ejecutable es
  PowerPC de 64 bits big-endian y el PARAM.SFO tiene el título correcto. **No se instaló ni ejecutó en una PS3 ni en RPCS3.**
- **NetSurf nativo en Linux** (`port/`): NetSurf 3.11 original (GPL v2) con su frontend framebuffer en superficie `ram`,
  y un parche propio que guarda una captura sin pantalla. Con eso se cargaron por HTTPS **Wikipedia en español** y
  **Google** (ver `docs/capturas/`). Es el motor de escritorio con JavaScript Duktape, no es todavía el programa de PS3.

## Cómo se probó
- Hola mundo: `make -C hola pkg` termina sin errores.
- NetSurf: `port/native/build.sh` y `port/native/shot.sh <url> salida.png`.

## Qué no se probó
- Nada en PS3 real ni en RPCS3 (ni siquiera el hola mundo).
- El workflow de GitHub Actions (`.github/workflows/build.yml`) está escrito pero no se ejecutó.

## Próximos pasos
1. Compilar las librerías de NetSurf (libparserutils, hubbub, libcss, libdom...) con PSL1GHT.
2. Escribir la superficie SDL2 de libnsfb para la PS3 (hoy solo hay SDL 1.2) y los controles del DualShock 3.
3. Red con libcurl + mbedTLS y archivo de certificados incluido; cuidado con big-endian.
4. QuickJS (quickjs-ng) para Google y verificar su compilación en big-endian.
5. Idioma español/inglés, página de inicio, user agent móvil, rutas en `/dev_hdd0/game/EONSURF01/USRDIR`.

## Bloqueos y notas
- GitHub bloquea tarballs, FTP y ghcr.io en este entorno; ver TOOLCHAIN.md.
- El git de NetSurf (git.netsurf-browser.org) no es accesible; se usa el tarball oficial `netsurf-all-3.11`.
- El port de Switch no se copió: no declara licencia. El único concepto tomado es la idea de capturar sin pantalla;
  el código es propio sobre NetSurf original.
