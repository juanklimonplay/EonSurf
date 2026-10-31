# Tarea: `intl.js`, un `Intl` liviano para EonSurf (QuickJS-ng no trae Intl)

Escribí `port/ps3/quickjs/intl.js`: JavaScript ES2023 para QuickJS-ng, sin dependencias, un IIFE que
define `globalThis.Intl` **solo si no existe**. Objetivo: que los sitios que usan Intl no rompan y den
resultados razonables en español (`es`, `es-AR`) e inglés (`en`, `en-US`); otros idiomas caen a `en`.

Implementar (constructores que funcionen con y sin `new`, con `resolvedOptions()` y `supportedLocalesOf`):
- `Intl.DateTimeFormat(locale, opts)`: `format(date)`, `formatToParts(date)`, `formatRange`; opciones
  `dateStyle`/`timeStyle` (full, long, medium, short), `year`, `month` (numeric, 2-digit, long, short, narrow),
  `day`, `weekday`, `hour`, `minute`, `second`, `hour12`/`hourCycle`, `timeZone` ("UTC" o la local; aceptar
  cualquier string sin fallar), `timeZoneName`. Nombres de meses y días en es/en.
- `Intl.NumberFormat(locale, opts)`: `format`, `formatToParts`; `style` decimal/percent/currency/unit,
  `currency` + `currencyDisplay`, `minimumFractionDigits`, `maximumFractionDigits`, `minimumIntegerDigits`,
  `useGrouping`, `notation` "compact" (1,2 mil / 1.2K), `signDisplay`, `unit` básicas. Separadores:
  es → "1.234,5", en → "1,234.5".
- `Intl.Collator`: `compare` (localeCompare simple, opción `sensitivity` y `numeric`).
- `Intl.PluralRules`: `select` (one/other para es y en; `type: "ordinal"` en inglés: one/two/few/other).
- `Intl.RelativeTimeFormat`: `format(valor, unidad)` ("hace 3 días", "in 3 days"), `numeric: "auto"`
  ("ayer", "mañana", "yesterday", "tomorrow").
- `Intl.ListFormat`: "a, b y c" / "a, b, and c" (`type` conjunction/disjunction).
- `Intl.Segmenter`: `segment(str)` iterable con `{segment, index, input, isWordLike}` (granularity
  grapheme por code point, word por regex `\p{L}`/`\p{N}`, sentence por `.!?`).
- `Intl.Locale`: `language`, `region`, `baseName`, `toString`, `maximize`, `minimize`.
- `Intl.DisplayNames`: `of(code)` para algunos idiomas y regiones comunes, si no devuelve el código.
- `Intl.getCanonicalLocales`, `Intl.supportedValuesOf` (listas cortas).
- Además: `Date.prototype.toLocaleDateString/toLocaleTimeString/toLocaleString`,
  `Number.prototype.toLocaleString` y `String.prototype.localeCompare` usando lo anterior.

Reglas: un solo archivo, comentarios breves en castellano, nada de TODOs. Al terminar verificá con
`node --check port/ps3/quickjs/intl.js` y corregí errores. No toques otros archivos.
