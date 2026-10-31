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
