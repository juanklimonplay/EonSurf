# EonSurf — navegador nativo para PS3 (especificación del proyecto)

**Nombre:** **EonSurf** (en el menú de la PS3: "EonSurf"; nombre completo: "EonSurf para PS3").
Corto para que entre en el menú, mantiene "Eon" de tus proyectos y "Surf" en homenaje a NetSurf, su motor.
**Página de inicio y buscador:** Google (https://www.google.com).
**Consola de destino:** PS3 **Super Slim** (CECH-4xxx, a confirmar con la etiqueta) con **PS3HEN 3.60** sobre HFW.
Fecha: 18/10/2026. Estado: **diseño**, nada compilado ni probado todavía.

## 0. Decisiones tomadas después del primer borrador
- **Google exige JavaScript desde enero de 2025** (si no hay JS, muestra "Activá JavaScript para seguir buscando").
  Por eso **QuickJS pasa a la versión 1**: sin JavaScript, Google no anda.
  Respaldo automático: si Google no carga, ofrecer DuckDuckGo en su versión sin JavaScript (html.duckduckgo.com/html).
- **Super Slim = solo HEN** (no admite CFW). El navegador tiene que funcionar bajo HEN, en HFW 4.83 a 4.93.
- **Modo "sitios livianos" (user agent móvil) activado por defecto:** muchos sitios mandan una versión para
  celulares que pesa mucho menos, lo que ayuda con la memoria de la PS3. Se puede cambiar a "escritorio" desde la barra.
- **Medidor de memoria** en la barra de estado y cierre ordenado si se llega al límite (en vez de colgarse).
- **Recuperar sesión** después de un cierre inesperado.
- **Actualizaciones** publicadas como .pkg en GitHub Releases, con número de versión visible en Ajustes.

---

## 1. Objetivo
Un navegador que corra **dentro de la PS3**, sin PC, instalado como **.pkg** (aparece en la columna Juego),
con **HTTPS moderno**, **CSS moderno** y **JavaScript moderno**, manejable con el **joystick**,
con la interfaz en **español e inglés**.

## 2. ¿Ya existe algo así?
- **Para PS3: no.** Búsqueda en GitHub y en la web en inglés, español, chino y ruso (18/10/2026): no hay ports de
  NetSurf, Links ni otro navegador casero para PS3.
- **Para otras consolas sí, y son el molde:**
  - **NetSurf para Nintendo Switch** — https://github.com/etonedemid/netsurf-switch (julio–septiembre 2026).
    Usa **SDL2 + curl + mbedTLS + QuickJS + FreeType + ffmpeg**, y está verificado abriendo sitios reales por HTTPS.
    Trae CSS moderno (flexbox, grid, variables, gradientes), JavaScript con DOM/fetch/eventos, SVG, canvas 2D,
    video y audio dentro de la página, marcadores y teclado del sistema.
    Incluye una **compilación nativa para Linux sin pantalla** que saca capturas (sirve para probar sin consola).
  - **NetSurf para 3DS** — https://github.com/coderman64/netsurf-3ds (en desarrollo, 2025). Muestra que NetSurf
    funciona con **64 a 80 MB** de memoria y ya da TLS 1.2.
- Ojo con la licencia: NetSurf es **GPL v2**; el port de Switch no declara licencia en GitHub. Antes de copiar código
  de ahí hay que confirmar los términos con el autor o usar solo el NetSurf original como base.

## 3. Comparación con el navegador que trae la PS3
| | Navegador de fábrica (PS3) | Eon Navegador (objetivo) |
|---|---|---|
| Motor | WebKit viejo (de la época de 2012) | NetSurf 3.12 actualizado |
| HTTPS | Solo certificados y cifrados viejos: muchos sitios fallan | mbedTLS con TLS 1.2 (y TLS 1.3 si se actualiza a mbedTLS 3.x), lista de certificados actualizable |
| CSS | Antiguo | Flexbox, grid, variables, gradientes, media queries modernas |
| JavaScript | Muy viejo y lento | QuickJS (JavaScript moderno, ES2023) con DOM, fetch y eventos |
| Actualizable | No (Sony lo abandonó) | Sí, lo mantenemos nosotros |
| Necesita HEN | No | Sí (como toda app casera) |
| Memoria | La del sistema | La de una app casera (~200 MB) |

## 4. ¿Y los navegadores de PS4 y PS5?
- **No se pueden adaptar.** Son WebKit modernos, de código cerrado en su integración, hechos para procesadores x86-64
  y para **8 a 16 GB** de memoria. La PS3 tiene 256 MB y un procesador PowerPC/Cell: no entra ni de cerca.
- **Ideas que sí copiamos de ellos:** teclado en pantalla cómodo con el joystick, zoom con los gatillos,
  marcadores e historial a mano, pantalla completa, modo lectura, página de inicio con accesos directos.

## 5. Base técnica
- **Motor:** NetSurf (C, liviano, ya funcionó en máquinas big-endian como Amiga y Atari: importante porque el
  procesador de la PS3 también lo es).
- **Kit de desarrollo:** ps3toolchain + PSL1GHT (abiertos, mantenidos en 2026).
- **Librerías que ya existen para PS3** (ps3libraries, octubre 2026): SDL2, FreeType, libpng, jpeg, zlib, libxml2,
  **libcurl 7.64** y **mbedTLS 2.28**.
- **Hay que portar nosotros:** QuickJS (JavaScript), libwebp, y las librerías propias de NetSurf (hubbub, libcss,
  libdom, libnsfb...).
- **Frontend:** el de "framebuffer" de NetSurf con SDL2, igual que en Switch.

## 6. Funciones
### Versión 1 (lo mínimo para que sea útil)
- HTML5 + CSS moderno, imágenes PNG/JPEG/GIF/WebP/SVG.
- **JavaScript con QuickJS** (necesario para Google). Activable/desactivable desde la barra.
- HTTPS con lista de certificados incluida y actualizable.
- **Controles con el DualShock 3:**
  - Palanca izquierda: puntero · Palanca derecha: desplazar
  - ✕: clic · ○: atrás · △: barra de direcciones · □: menú (marcadores, historial, ajustes)
  - L1/R1: pestaña anterior/siguiente (si hay pestañas) o página arriba/abajo
  - L2/R2: zoom menos/más · START: pantalla completa · SELECT: modo lectura
- **Teclado en pantalla:** usar el teclado del sistema de la PS3 si PSL1GHT lo permite (a verificar);
  si no, uno propio con predicción simple. Teclados USB y Bluetooth también.
- **Interfaz en español e inglés**, según el idioma de la PS3 (cambiable en ajustes).
- Marcadores, historial y cookies guardados en el disco de la PS3.
- Página de inicio propia con accesos rápidos y buscador liviano.
- Zoom y escala adaptada a 720p y 1080p.
- Registro de errores en un archivo para diagnosticar.

### Versión 2
- Descargas al disco interno o a un pendrive, con aviso de espacio libre.
- Varias pestañas (con límite según la memoria).
- Modo oscuro y modo lectura mejorado.

### Versión 3 (opcional)
- Audio y video dentro de la página. Es lo más pesado: el procesador de la PS3 puede quedarse corto para video
  en alta resolución. Evaluar después de medir.

## 7. Límites que hay que aceptar desde el principio
- **Memoria (~200 MB):** sitios muy pesados se van a cortar o cerrar. Hay que limitar caché, imágenes y pestañas.
- **Aplicaciones web grandes** (la web de YouTube, Google Docs, redes sociales completas) probablemente no funcionen
  bien. Lo realista: Wikipedia, noticias, foros, buscadores, blogs, sitios simples.
- **Video adaptativo** (el de YouTube) no está ni en el port de Switch.
- **Big-endian:** QuickJS y otras librerías pueden tener errores en procesadores big-endian. Hay que probarlo temprano.
- **mbedTLS 2.28** solo llega a TLS 1.2. Para TLS 1.3 habría que portar mbedTLS 3.x (mejora recomendada).

## 8. Cómo lo vamos a probar (sin tener la PS3 todavía)
1. **Compilación nativa en Linux** (como hace el port de Switch): el motor saca capturas de páginas sin pantalla.
   Sirve para probar el diseño de páginas en la nube.
2. **RPCS3** (el emulador de PS3) en la PC: corre apps caseras. Necesita el firmware oficial de Sony y una PC con
   buen procesador; hay que ver si la tuya alcanza.
3. **La PS3 real**, el día que la traigas.

## 9. Etapas del trabajo (sesión en la nube)
1. Preparar el kit (ps3toolchain + PSL1GHT + ps3libraries) y compilar un "hola mundo" con SDL2 en .pkg.
2. Compilar las librerías de NetSurf y el motor, primero sin JavaScript.
3. Frontend SDL2 adaptado a la PS3: pantalla, joystick, teclado, rutas de archivos.
4. Interfaz en español e inglés, página de inicio, marcadores, ajustes.
5. Empaquetar en .pkg (ícono, nombre, versión) con GitHub Actions.
6. Probar en Linux sin pantalla y en RPCS3.
7. QuickJS integrado (ya en la versión 1), probado con Google en la compilación de Linux.
8. Versión 2: descargas, pestañas, modo oscuro.

## 10. Pendiente del dueño
- Ícono: lo genera la sesión (simple, legible en el menú de la PS3) y el dueño lo aprueba o lo cambia.
- Confirmar con la etiqueta el modelo exacto (CECH-4xxx) y el firmware de la PS3.
