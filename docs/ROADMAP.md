# EonSurf: hoja de ruta

EonSurf es la versión **nativa**: todo corre en la PS3, sin servidores.

## Ahora (versión nativa)
1. **JavaScript propio**
   - Etapa 1 (hecha, 30-10-2026): Duktape activado, redibujo en vivo, `fetch`, `XMLHttpRequest`,
     `Promise`, `localStorage` en disco, `querySelector`/`matches`/`closest` (port/ps3/patch-js.py,
     port/ps3/js/). Prueba: startpage/js-test.html.
   - Pendiente de la etapa 1: código HTTP real (hoy 200 o error), cabeceras propias, POST con JSON,
     guardado diferido de localStorage, redibujo parcial en vez de rehacer toda la página.
   - Etapa 2: QuickJS (JavaScript moderno, ES2023) con un generador de bindings propio.
2. **Pestañas** (L1/R1) y **menú START** (marcadores, historial, opciones).
3. **Pantalla completa** (□) y **modo lector** (SELECT).
4. **Descargas**: `.pkg` a `/dev_hdd0/packages/`, el resto a `/dev_hdd0/downloads/`.
5. Cursor con 4 velocidades (R3), modo escritorio/móvil (L3), cruceta izq/der = atrás/adelante.
6. Idioma español/inglés según la consola.
7. **Atajos para sitios grandes** sin servidor:
   - YouTube: buscar y reproducir con un reproductor propio (ffmpeg) en la PS3.
   - Reddit / X / Instagram: redirigir a versiones livianas (old.reddit, Redlib, Nitter...).
   - Correo: lector IMAP propio (Gmail ya no tiene versión HTML básica).
8. Extras del spec: EonPush (QR), EonAI, `gemini://`.
9. Probar en PS3 real.

## Después: EonSurf TurboMode (proyecto aparte)
Un Chrome real corre en una PC, Raspberry Pi o servidor, abre la página completa y le manda a la
PS3 solo lo que hay que mostrar (estilo Opera Mini). Referencias libres: Browservice y WRP.
EonSurf elegiría solo: páginas livianas directo, pesadas por TurboMode. Incluir un instalador
de un clic "EonSurf Server" para Windows y Raspberry Pi.
