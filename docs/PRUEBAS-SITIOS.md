# Prueba de sitios reales (30-10-2026)

EonSurf v1.2 (NetSurf 3.11 + QuickJS-ng 0.17) en RPCS3 0.0.43, 1280×720. Cada sitio se abrió en frío.
Los tiempos son los de la barra de estado ("Done") en el emulador; en la PS3 real pueden variar.

| Nivel | Sitio | Resultado | Tiempo | Scripts / errores JS | Qué pasa |
|---|---|---|---|---|---|
| 1 | DuckDuckGo HTML | ✅ Bien | 3,4 s | 0 / 0 | Perfecto |
| 1 | Hacker News | ✅ Bien | 4,1 s | 1 / 0 | Perfecto |
| 2 | Wikipedia (escritorio) | ✅ Bien | 9 s | 7 / 0 | Bien con imágenes; caracteres japoneses en cuadraditos |
| 2 | old.reddit.com | ❌ Falla | 2,5 s | — | Redirige a iniciar sesión y queda en blanco |
| 2 | BBC Mundo | ⚠️ A medias | 81 s | 46 / 5 | Menú sí, noticias no: el límite de 10 s corta sus scripts |
| 2 | GitHub | ⚠️ A medias | 4,5 s | 9 / 8 | Se ven cosas ocultas: módulos JS y componentes web |
| 2 | reddit.com | ⚠️ A medias | 4,9 s | 44 / 6 | Pasa el desafío anti-bots y muestra posts; diseño desarmado |
| 3 | Google búsqueda | ❌ Falla | 1,1 s | 10 / 3 | Pide captcha (reCAPTCHA) que no se dibuja |
| 3 | MercadoLibre | ⚠️ A medias | 50 s | 85 / 2 | Imágenes sí, diseño encimado (flexbox/grid) |
| 4 | YouTube | ❌ Falla | 17,6 s | 42 / 3 | Solo el esqueleto gris |
| 4 | X, Instagram | — | — | — | Sin probar todavía |

**Tope actual:** sitios de texto y HTML clásico andan bien. Los sitios modernos cargan pero se
ven mal o incompletos; los más pesados no funcionan.

## Causas encontradas y soluciones (por impacto)

1. **CSS moderno (flexbox, grid, variables `var()`, `position: sticky`, `:is()`)** — NetSurf 3.11 tiene
   flexbox parcial y nada de grid ni variables. Es lo que más "desarma" MercadoLibre, reddit y GitHub.
   *Solución:* mejorar libcss/layout de NetSurf (variables y grid primero) o normalizar CSS en un
   paso previo. Trabajo grande, el de mayor impacto visual.
2. **Módulos JS (`<script type="module">`, `import`/`export`, `import.meta`)** — GitHub, reddit, YouTube.
   QuickJS los soporta: falta que NetSurf ejecute los scripts `type="module"` y que `eon_qjs.c`
   los evalúe con `JS_EVAL_TYPE_MODULE` y cargue los `import` con un cargador de módulos propio.
   *Solución:* mediana, alto impacto.
3. **Componentes web (`customElements.define`, Shadow DOM, `<template>`)** — YouTube, reddit, GitHub.
   Hoy son stubs. *Solución:* implementar `customElements` de verdad (llamar `constructor` y
   `connectedCallback` al insertar) y un Shadow DOM aplanado (dibujar su contenido como hijos).
4. **Límite de ejecución de 10 s** — BBC Mundo: en el PPU emulado sus scripts tardan más y se cortan
   ("interrupted"). *Solución:* fácil: subir a 30 s y medir en la PS3 real; ceder el control entre scripts.
5. **APIs faltantes vistas en los errores:** `MessageChannel`, `Worker` (reCAPTCHA de Google),
   `CSSStyleSheet` constructible (MercadoLibre), `CharacterData` y `performance.timing`/
   `getEntriesByType('navigation')` (YouTube). *Solución:* fácil a mediana, agregar a `dom.js`.
6. **Bloqueos anti-bots** (Google, old.reddit) — detectan un navegador desconocido. *Solución:* con
   1, 2 y 5 resueltos el desafío de JS pasa (reddit.com ya lo pasa); además probar con un user agent
   de navegador común. Para Google, usar DuckDuckGo/Startpage como buscador por defecto.
7. **Velocidad** — BBC 81 s y MercadoLibre 50 s, con muchos scripts (46 y 85). *Solución:* compilar
   QuickJS con más optimización, cachear el bytecode de `dom.js`, y redibujo en vivo parcial en vez de
   rehacer toda la página en cada cambio.
8. **Fuentes sin caracteres CJK** (japonés en Wikipedia) — agregar una fuente Noto CJK como respaldo
   (pesa ~15 MB: va bien en la versión full).

## Ronda 2 (30-10-2026, mismo día)

Agregado: módulos JS + import maps, componentes web reales, Shadow DOM aplanado, `Intl`, variables
CSS, reglas HTML5 en el CSS base, ~60 interfaces estándar más, código HTTP real en fetch/XHR, límite
de 30 s, y un crash arreglado (pedidos de red liberados dos veces).

| Sitio | Antes | Ahora |
|---|---|---|
| GitHub | 8 errores JS, avisos ocultos a la vista | 0 errores; módulos y React (82 imports) cargan; ya no se ven los avisos ocultos; falta CSS del encabezado (48 s) |
| reddit.com | posts sin componentes | se arman los componentes (panel de registro, menús); diseño todavía desarmado (53 s) |
| YouTube | 3 errores, solo esqueleto | 0 errores; Polymer arranca y crea `yt-icon`, `ytd-logo`...; algunos componentes fallan por dentro; sigue sin videos |

Próximo: errores internos de los componentes de YouTube (ahora el registro muestra el mensaje),
CSS grid y posicionamiento (modales que deberían flotar), y velocidad (callbacks de 30 s en reddit).

## Transform y velocidad (31-10-2026)

- Red: más conexiones reutilizadas y DNS compartido: MercadoLibre de 74 s a 44 s.
- Transform: modal centrado y `transform` desde JS verificados en RPCS3 (página de prueba).
- **Problema abierto:** después de esos cambios MercadoLibre queda en blanco más de 5 minutos.
  La página se arma (DOM a cajas listo a los 56 s), pero después hay pausas de 4 a 18 s sin registro:
  NetSurf recalcula el diseño completo cada vez que llega una imagen, y en el PPU cada recálculo de
  una página grande cuesta segundos. Desactivar transform (CSS y JS) no lo cambió. Próximo paso:
  medir el tiempo de cada recálculo (layout), y agrupar los recálculos mientras llegan imágenes.

## Ronda 3 (31-10-2026): test completo con medidas

EonSurf con QuickJS, módulos, componentes web, grid/inset/transform/opacity en el traductor CSS y
red mejorada. RPCS3 0.0.43, sin otros programas pesados. Tiempo = "Done" de la barra de estado
(reloj emulado). Datos crudos: `docs/medidas-ronda3.jsonl`.

| # | Sitio | Resultado | Tiempo | Scripts | Errores JS | Imports | Pedidos | HTTP 4xx/5xx |
|---|---|---|---|---|---|---|---|---|
| 1 | DuckDuckGo HTML | ✅ perfecto | 4,6 s | 0 | 0 | 0 | 16 | 4 |
| 2 | Hacker News | ✅ perfecto | 4,0 s | 1 | 0 | 0 | 6 | 0 |
| 3 | Wikipedia | ✅ bien (íconos SVG como cuadrados, japonés en cuadraditos) | 14,3 s | 7 | 0 | 0 | 69 | 0 |
| 4 | old.reddit | ❌ manda a login con reCAPTCHA | 10,5 s | 31 | 26 | 0 | 28 | 0 |
| 5 | reddit.com | ⚠️ bien legible: posts y barra lateral a la derecha | 16,8 s | 51 | 136 | 1 | 49 | 1 |
| 6 | BBC Mundo | ❌ se congela | — | 51 | 1 | 0 | 160 | 0 |
| 7 | GitHub | ⚠️ contenido sí, encabezado desarmado | 34,1 s | 11 | 11 | 121 | 51 | 7 |
| 8 | Google | ❌ captcha (/sorry) | 1,2 s | 10 | 5 | 0 | 9 | 1 |
| 9 | MercadoLibre | ⚠️ banners y productos; botones sin texto, tarjeta encimada | 49,2 s | 89 | 2 | 11 | 427 | 109 |
| 10 | Infobae | ❌ casi vacía (solo fecha y botones) | 109,1 s | 61 | 13 | 0 | 385 | 8 |
| 11 | YouTube | ❌ sin videos (íconos y líneas) | 30,3 s | 44 | 88 | 0 | 106 | 1 |
| 12 | X (Twitter) | ⚠️ perfil, datos y posts visibles; todo apretado en una fila | 5,9 s | 21 | 12 | 0 | 16 | 0 |
| 13 | Instagram | ❌ solo el pie de página | 14,3 s | 50 | 1 | 0 | 14 | 0 |

Diseño (layout) medido: 1 a 513 ms por recálculo (máximo en X); no es el cuello de botella.

### Fallas encontradas y qué hacer

| # | Falla | Dónde | Causa probable | Arreglo propuesto |
|---|---|---|---|---|
| F1 | Se congela tras el consentimiento de cookies | BBC | script de `privacy-mgmt` en bucle o esperando algo; el reloj emulado hace que el corte de 30 s tarde minutos reales | medir con corte de 10 s y pila; revisar APIs que usa (postMessage, iframes) |
| F2 | 133× "TypeError: not a function" en promesas | reddit, old.reddit, MercadoLibre | falta alguna función del DOM o de JS | ahora el registro guarda la pila: identificar la función y agregarla |
| F3 | 83× rechazos "undefined" + "Error: Va" | YouTube | componentes de Polymer que fallan por dentro | igual que F2, con la pila nueva |
| F4 | "cannot read property 'remove' of undefined" (12×) | X | `classList` o similar indefinido en algún objeto | revisar qué objeto (probablemente `document.documentElement`/SVG) |
| F5 | "invalid 'instanceof' right operand" | Instagram | falta un constructor global que usa con `instanceof` | encontrar cuál y definirlo |
| F6 | "Failed to fetch" (8×) | GitHub | fetch con cabeceras/JSON/POST que nuestro `__ns.http` no soporta | enviar cabeceras y cuerpo reales (no solo urlencoded) |
| F7 | Diseño apretado en una sola fila | X (React Native Web) | flex con `flex-direction`/`flex-basis` o `min-width` que NetSurf no respeta | estudiar el CSS de RN Web y traducir en el modernizador |
| F8 | Botones sin texto | MercadoLibre | texto con color igual al fondo, o fuente por variable | revisar el CSS de esos botones |
| F9 | Página casi vacía | Infobae | contenido armado por JS (59 timers lentos, rechazos `[object Set]`) | depurar con la pila nueva |
| F10 | Íconos SVG como cuadrados negros | Wikipedia, YouTube, GitHub | libsvgtiny desactivado en la compilación | activar libsvgtiny (`<img src=.svg>` y SVG en línea) |
| F11 | Captchas (Google, old.reddit) | — | sin Worker, y el sitio desconfía del navegador | buscador por defecto DuckDuckGo; reCAPTCHA queda para más adelante |
| F12 | z-index no se respeta | página de prueba (modal tapado por la grilla) | NetSurf pinta en orden del documento | ordenar el pintado por z-index en el redraw |
