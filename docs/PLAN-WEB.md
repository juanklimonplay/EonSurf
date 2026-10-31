# EonSurf: plan para que la web moderna funcione

Lista de trabajo armada con el test de sitios de la ronda 3 (`docs/PRUEBAS-SITIOS.md`) y una
búsqueda de soluciones existentes (31-10-2026). Donde no existe nada hecho para NetSurf o para
QuickJS en un solo hilo, lo creamos nosotros.

Orden: primero lo que destraba más sitios con menos trabajo.

| # | Tarea | Fallas que ataca | ¿Existe hecho? | Plan |
|---|---|---|---|---|
| T1 | **CSSOM**: `style.sheet`, `insertRule`, `deleteRule`, `cssRules`, `document.styleSheets` | F8, F9, F3 y cualquier sitio con CSS-en-JS | No para NetSurf. emotion y styled-components usan `insertRule` en producción ("speedy") | `sheet` devuelve un `CSSStyleSheet` atado al `<style>`; cada `insertRule` reescribe el texto del `<style>` y NetSurf lo vuelve a leer (pasa por el traductor CSS) |
| T2 | **SVG** con libsvgtiny | F10 (íconos en Wikipedia, YouTube, GitHub) | Sí: libsvgtiny viene con NetSurf; nuestro parche 0004 lo desactivó | volver a compilarlo (`gperf` en el kit) y `NETSURF_USE_NSSVG=YES` |
| T3 | **EonFetch**: HTTP completo para `fetch`/XHR | F6 (GitHub "Failed to fetch"), APIs JSON | No: la cache de NetSurf solo hace GET y POST urlencoded | cliente propio con libcurl multi (asíncrono, sondeado por el planificador): métodos, cabeceras, cuerpo JSON/binario, código y cabeceras de respuesta, cookies de urldb |
| T4 | **z-index** | F12 (modales tapados) | No para NetSurf | en el redibujo, pintar primero los hijos en flujo y después los posicionados ordenados por z-index |
| T5 | **Depurar con pilas** | F2, F3, F4, F5, F9 | — | con la pila de los rechazos, encontrar y agregar cada función/constructor que falta |
| T6 | **BBC se congela** | F1 | — | corte de ejecución de 10 s solo para callbacks, pila al cortar, revisar `privacy-mgmt` |
| T7 | **X apretado** (React Native Web) | F7 | No | estudiar sus clases CSS (`r-*`: `display:flex`, `flex-direction:column`, `flex-basis`) y traducir |
| T8 | **Worker en un solo hilo** | F11 (reCAPTCHA) y sitios que lo usan para tareas | No (solo simulaciones vacías) | cada Worker en un contexto QuickJS propio del mismo runtime, script bajado con curl, `postMessage` por cola con temporizador |
| T9 | **Velocidad** | tiempos de 30-110 s | — | cifrado TLS más liviano (x25519/ChaCha si mbedTLS lo trae), caché del bytecode de `dom.js`, menos objetos JS por nodo |
| T10 | **Fuente CJK** | japonés en cuadraditos | Sí (Noto Sans CJK) | sumar una fuente de respaldo en la versión full |

## Fuentes consultadas
- emotion `@emotion/sheet`: en producción inserta reglas con `insertRule` (https://www.npmjs.com/package/@emotion/sheet).
- libsvgtiny en NetSurf framebuffer 3.11: paquetes de Arch (netsurf-fb) y Gentoo (ebuild 3.11).
- Workers en QuickJS de un solo hilo: solo hay simulaciones vacías (wasm-rquickjs).
- z-index en NetSurf: no se encontró implementación.
