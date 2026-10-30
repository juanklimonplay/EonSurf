# EonSurf: especificación de `dom.js` (API web sobre QuickJS)

`dom.js` se ejecuta una vez por página, al crear el contexto de QuickJS, **antes** de que exista
el documento. Corre en QuickJS-ng 0.17 (ES2023 completo: clases, campos privados, `Map`, `WeakMap`,
`Proxy`, `Symbol`, generadores, `async`). No hay `require`, ni `import`, ni APIs de Node.js.

Todo se arma sobre el objeto global oculto **`__ns`**, escrito en C. `dom.js` debe:
1. Definir las clases y globales de la API web descritos abajo.
2. Implementar los **ganchos** que llama el C (`__ns.setup`, `__ns.onTimer`, etc.).
3. No romper nunca: cada gancho atrapa sus excepciones y las informa con `__ns.log`.

## 1. API nativa disponible (`__ns.*`, escrita en C)

Un **nodo** es un objeto opaco que da el C. Cada nodo de libdom tiene **un solo** objeto JS
(identidad estable), así que se pueden guardar propiedades JS en él y usarlo como clave de `WeakMap`.

| Función | Devuelve | Notas |
|---|---|---|
| `document()` | nodo \| null | el documento, o null si todavía no existe |
| `type(n)` | número | 1 elemento, 3 texto, 8 comentario, 9 documento, 11 fragmento |
| `name(n)` | string | nodeName en MAYÚSCULAS para elementos HTML (`"DIV"`), `"#text"`, `"#document"` |
| `value(n)` / `setValue(n, s)` | string \| null | nodeValue (texto y comentarios) |
| `text(n)` / `setText(n, s)` | string | textContent |
| `parent(n)` `first(n)` `last(n)` `next(n)` `prev(n)` | nodo \| null | navegación |
| `children(n)` | arreglo de nodos | **todos** los hijos (childNodes, no solo elementos) |
| `append(p, c)` | nodo | appendChild |
| `insert(p, c, ref)` | nodo | insertBefore; `ref` null = al final |
| `remove(p, c)` | nodo | removeChild |
| `replace(p, nuevo, viejo)` | nodo | replaceChild |
| `clone(n, deep)` | nodo | cloneNode |
| `createElement(tag)` `createText(s)` `createComment(s)` `createFragment()` | nodo | |
| `getAttr(n, k)` | string \| null | |
| `setAttr(n, k, v)` `removeAttr(n, k)` | | |
| `attrNames(n)` | arreglo de strings | |
| `byId(id)` | nodo \| null | |
| `byTag(raiz, tag)` | arreglo | `tag` `"*"` = todos los descendientes elementos, en orden de documento |
| `setInnerHTML(n, html)` | | parsea HTML y reemplaza los hijos |
| `formGet(n, prop)` | | `"value"` (input, textarea, select), `"checked"` (input), `"selected"` (option), `"selectedIndex"` (select) |
| `formSet(n, prop, v)` | | `"value"` (input, textarea, select), `"checked"` (input) |
| `rect(n)` | `[x, y, ancho, alto]` \| null | posición en la página en px; null si no se dibuja |
| `href()` | string | URL actual de la página |
| `navigate(url)` | | ir a una URL (relativa a la página) |
| `history(d)` | | -1 atrás, 1 adelante, 0 recargar |
| `cookie()` / `cookie(s)` | string | leer las cookies de la página / guardar una (`"k=v; path=/"`) |
| `log(s)` | | escribe en el registro de EonSurf |
| `setTimer(id, ms)` / `clearTimer(id)` | | dispara `__ns.onTimer(id)` **una sola vez** a los `ms` |
| `listen(type)` | | pide que el C le pase a `__ns.onEvent` los eventos de ese tipo que despacha NetSurf |
| `http(id, method, url, body)` | | pedido de red; responde con `__ns.onHttp(id, status, texto, contentType)`; status 200 = bien, 0 = error |
| `storage(origen)` / `storage(origen, texto)` | string \| undefined | leer / guardar en el disco |
| `keep` | arreglo | uso interno del C: no tocar |

## 2. Ganchos que llama el C (dom.js los define en `__ns`)

| Gancho | Cuándo | Qué hace |
|---|---|---|
| `setup(obj, type, name)` | al crear el objeto de un nodo | `Object.setPrototypeOf(obj, ...)` según tipo y etiqueta (tabla en §4) |
| `onTimer(id)` | vence un temporizador | ejecuta la función guardada; si era `setInterval`, la vuelve a programar con `setTimer` |
| `onEvent(target, type)` | NetSurf despacha un evento DOM (p. ej. `click` por el usuario) | crea el `Event` adecuado y lo propaga en JS (§5); devuelve `true` si se llamó `preventDefault()` |
| `onFire(type, target)` | NetSurf dispara `"load"` (target null) | si es load: primero `DOMContentLoaded` en `document`, después `readyState = "complete"` y `load` en `window` (y `onload` del body); devuelve si se canceló |
| `onNewElement(el)` | aparece un elemento con atributos `on*` | por cada atributo `onclick="..."` arma la función manejadora (§5) |
| `onHttp(id, status, text, type)` | terminó un pedido de red | resuelve el XHR / fetch correspondiente |
| `onClose()` | la página se cierra | dispara `pagehide`/`unload`, limpia |

## 3. Globales

- `window`, `self`, `globalThis`, `frames`, `parent`, `top` = el objeto global.
- `document`: **getter** que devuelve el `Document` (`__ns.document()`); cachearlo cuando no sea null.
- `location`: objeto con `href` (get = `__ns.href()`, set = navegar), `protocol`, `host`, `hostname`,
  `port`, `pathname`, `search`, `hash`, `origin`, `assign(u)`, `replace(u)`, `reload()`, `toString()`.
  Usar una clase `URL` propia (§7) para descomponer.
- `history`: `back()`, `forward()`, `go(n)`, `length` (1), `state` (null), `pushState()`/`replaceState()`
  (guardan el estado y cambian solo el valor de `location.href` que se informa, sin navegar).
- `navigator`: `userAgent` = `"Mozilla/5.0 (PlayStation 3; EonSurf) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36 EonSurf/1.0"`,
  `language` `"es-AR"`, `languages`, `platform` `"PLAYSTATION 3"`, `onLine` true, `cookieEnabled` true,
  `vendor`, `maxTouchPoints` 0, `hardwareConcurrency` 2, `clipboard` (stub con promesas).
- `screen`: `width` 1280, `height` 720, `availWidth`, `availHeight`, `colorDepth` 24.
- `innerWidth` 1280, `innerHeight` 668, `outerWidth`, `outerHeight`, `devicePixelRatio` 1,
  `scrollX`/`scrollY`/`pageXOffset`/`pageYOffset` 0, `scrollTo()`, `scrollBy()`, `scroll()` (no hacen nada).
- `console`: `log`, `info`, `warn`, `error`, `debug`, `trace`, `dir`, `table`, `group*`, `time*`, `assert`, `count`
  → `__ns.log` con los argumentos formateados (objetos con un `JSON.stringify` seguro).
- Temporizadores: `setTimeout`, `setInterval`, `clearTimeout`, `clearInterval`, `setImmediate`,
  `requestAnimationFrame` (16 ms, pasa `performance.now()`), `cancelAnimationFrame`,
  `requestIdleCallback` (1 ms, pasa un objeto con `timeRemaining()` = 50 y `didTimeout` false), `queueMicrotask`.
  Aceptar string como primer argumento (evaluarlo con `(0, eval)`). Pasar los argumentos extra.
- `performance.now()`, `performance.timeOrigin`, `performance.mark()`/`measure()` (stubs), `performance.getEntriesByType()` → `[]`.
- `alert`, `confirm` (false), `prompt` (null): solo `__ns.log`.
- `getComputedStyle(el)`: un `CSSStyleDeclaration` con lo que tenga `el.style` y `getPropertyValue` (devuelve '' si falta).
- `matchMedia(q)`: `{ matches, media, onchange, addListener, removeListener, addEventListener, removeEventListener }`;
  `matches` evalúa `min-width`/`max-width`/`min-height`/`max-height` contra 1280×668, `prefers-color-scheme: dark` false,
  `screen`/`all` true, `print` false.
- `atob`, `btoa`, `TextEncoder`, `TextDecoder` (UTF-8), `structuredClone` (por JSON), `crypto.getRandomValues`, `crypto.randomUUID`.
- `localStorage`, `sessionStorage` (§8), `fetch`, `XMLHttpRequest`, `Headers`, `Request`, `Response`,
  `FormData`, `URLSearchParams`, `URL`, `AbortController`, `AbortSignal`, `Blob` (solo texto),
  `MutationObserver` (§6), `IntersectionObserver` y `ResizeObserver` (llaman una vez con "visible" y nada más),
  `CustomEvent`, `Event`, `MouseEvent`, `KeyboardEvent`, `FocusEvent`, `InputEvent`, `UIEvent`,
  `EventTarget`, `Node`, `Element`, `HTMLElement` y las subclases de §4, `Text`, `Comment`, `Document`,
  `DocumentFragment`, `NodeList`, `HTMLCollection`, `DOMTokenList`, `CSSStyleDeclaration`, `Image` (= crea `<img>`),
  `customElements` (stub: `define`, `get`, `whenDefined`).

## 4. Clases del DOM

`EventTarget` ← `Node` ← (`Element` ← `HTMLElement` ← subclases) | `CharacterData` ← (`Text`, `Comment`) | `Document` | `DocumentFragment`.

`__ns.setup(obj, type, name)`: tipo 1 → por etiqueta (`name` en mayúsculas):
`A`→`HTMLAnchorElement`, `IMG`→`HTMLImageElement`, `INPUT`→`HTMLInputElement`, `TEXTAREA`→`HTMLTextAreaElement`,
`SELECT`→`HTMLSelectElement`, `OPTION`→`HTMLOptionElement`, `FORM`→`HTMLFormElement`, `BUTTON`→`HTMLButtonElement`,
`SCRIPT`→`HTMLScriptElement`, `STYLE`→`HTMLStyleElement`, `LINK`→`HTMLLinkElement`, `IFRAME`→`HTMLIFrameElement`,
`CANVAS`→`HTMLCanvasElement` (getContext devuelve null), `BODY`→`HTMLBodyElement`, `HTML`→`HTMLHtmlElement`,
`HEAD`→`HTMLHeadElement`, `TITLE`→`HTMLTitleElement`, `DIV`→`HTMLDivElement`, `SPAN`→`HTMLSpanElement`,
`P`→`HTMLParagraphElement`, `UL`/`OL`/`LI`, `TABLE`/`TR`/`TD`/`TH`, `LABEL`, `VIDEO`/`AUDIO` (`HTMLMediaElement`:
`play()` devuelve promesa rechazada, `pause()`), `TEMPLATE` (`content` = fragmento con los hijos), resto → `HTMLElement`.
Tipo 3 → `Text`, 8 → `Comment`, 9 → `Document`, 11 → `DocumentFragment`.

**Node**: `nodeType`, `nodeName`, `nodeValue`, `textContent` (get/set), `parentNode`, `parentElement`,
`childNodes` (NodeList), `firstChild`, `lastChild`, `nextSibling`, `previousSibling`, `ownerDocument`,
`isConnected`, `appendChild`, `insertBefore`, `removeChild`, `replaceChild`, `cloneNode`, `contains`,
`hasChildNodes`, `getRootNode`, `isSameNode`, `isEqualNode`, `compareDocumentPosition`, `normalize` (stub),
constantes `ELEMENT_NODE`=1 … `DOCUMENT_POSITION_*`.
Si se inserta un `DocumentFragment`, se insertan sus hijos (el C ya lo hace, pero verificar).

**ParentNode / ChildNode** (en Element, Document, DocumentFragment): `children` (HTMLCollection de elementos),
`firstElementChild`, `lastElementChild`, `childElementCount`, `append(...nodosOStrings)`, `prepend`,
`replaceChildren`, `querySelector`, `querySelectorAll` (NodeList estática), y en Element/CharacterData:
`before`, `after`, `replaceWith`, `remove`, `nextElementSibling`, `previousElementSibling`.

**Element**: `tagName`, `localName` (minúsculas), `id`, `className`, `classList` (DOMTokenList viva sobre `class`),
`getAttribute`, `setAttribute`, `removeAttribute`, `hasAttribute`, `toggleAttribute`, `getAttributeNames`,
`attributes` (arreglo de `{name, value}` con `length` y `item(i)`), `dataset` (Proxy sobre `data-*` con camelCase),
`innerHTML` (get = serialización propia de los hijos; set = `__ns.setInnerHTML`), `outerHTML` (get; set = reemplazar),
`insertAdjacentHTML(pos, html)`, `insertAdjacentElement`, `insertAdjacentText`, `matches`, `closest`,
`getElementsByTagName`, `getElementsByClassName`, `getBoundingClientRect()` (de `__ns.rect`; `{x, y, left, top, right,
bottom, width, height}`, ceros si es null), `getClientRects()`, `offsetWidth`/`offsetHeight`/`offsetTop`/`offsetLeft`,
`clientWidth`/`clientHeight`, `scrollWidth`/`scrollHeight`, `scrollTop`/`scrollLeft` (0, asignables),
`scrollIntoView()` y `focus()`/`blur()`/`click()` (click despacha un `MouseEvent` "click" en JS y, si no se canceló:
en `<a href>` navega; en submit dentro de form hace `form.submit()`; en checkbox/radio cambia `checked`).
`attachShadow()` → devuelve un DocumentFragment (stub).

**HTMLElement**: `style` (CSSStyleDeclaration sobre el atributo `style`), `hidden`, `title`, `lang`, `dir`,
`tabIndex`, `innerText` (= textContent), `outerText`, `onclick` y demás `on*` como propiedades (§5), `dataset`.
Atributos reflejados (get/set de `getAttribute`/`setAttribute`) según la subclase: `href`, `src` (resueltos a URL
absoluta con `new URL(attr, location.href)`), `alt`, `name`, `type`, `rel`, `target`, `action`, `method`,
`placeholder`, `width`, `height`, `htmlFor` (`for`), `disabled`/`readOnly`/`required`/`multiple` (booleanos), etc.
Formularios: `value` y `checked` con `__ns.formGet/formSet`, `defaultValue`, `selectedIndex`, `options`,
`selectedOptions`, `form`, `elements`, `submit()` (navega con GET armando la query de los campos con nombre;
POST: `__ns.navigate` con la query también, por ahora), `reset()`, `requestSubmit()`, `checkValidity()` true.
`HTMLAnchorElement`: `href`, `protocol`, `host`, `pathname`, `search`, `hash` como `URL`.
`HTMLImageElement`: `src`, `complete` (true), `naturalWidth`/`naturalHeight` (de rect o 0), `decode()` (promesa).
`HTMLScriptElement`, `HTMLStyleElement`, `HTMLLinkElement`: atributos reflejados y `sheet` null.

**Document**: `documentElement`, `head`, `body`, `title` (get/set sobre `<title>`), `URL`, `documentURI`,
`location`, `domain`, `referrer` (''), `cookie` (get/set con `__ns.cookie`), `readyState` ("loading" →
"interactive" al terminar dom.js si ya hay body, "complete" en onFire load), `visibilityState` "visible", `hidden` false,
`defaultView` (window), `activeElement` (body o el último enfocado), `characterSet` "UTF-8", `compatMode`,
`getElementById`, `getElementsByTagName`, `getElementsByClassName`, `getElementsByName`, `createElement`,
`createElementNS` (ignorar el namespace), `createTextNode`, `createComment`, `createDocumentFragment`,
`createEvent(tipo)` (con `initEvent`), `createRange()` (stub mínimo), `createTreeWalker` (stub sencillo que recorre),
`querySelector(All)`, `implementation.createHTMLDocument()` (stub), `hasFocus()` true,
`write()`/`writeln()` (si hay body, agrega al final con insertAdjacentHTML; si no, ignorar), `open`/`close` (stubs),
`forms`, `images`, `links`, `scripts` (HTMLCollection), `fonts.ready` (promesa resuelta).

**NodeList / HTMLCollection**: indexables con `[i]`, `length`, `item(i)`, iterables (`for...of`), `forEach`,
`entries`, `keys`, `values`; HTMLCollection también `namedItem`. `childNodes` y `children` son **vivas**:
usar un `Proxy` que recalcule con `__ns.children` en cada acceso.

**DOMTokenList**: `add`, `remove`, `toggle(t, force)`, `contains`, `replace`, `item`, `length`, `value`, iterable.

**CSSStyleDeclaration**: lee y escribe el atributo `style` (`"color: red; display: none"`); propiedades en
camelCase y con guiones (`el.style.backgroundColor` y `el.style['background-color']`), `cssText`,
`getPropertyValue`, `setProperty`, `removeProperty`, `length`, `item`. Implementar con `Proxy`.

**Selectores** (`querySelector`, `matches`, `closest`): portar el motor de `eonsurf-polyfill.js` adjunto
(funciones `parse`, `compound`, `matchSimple`, `matchSteps`, `matches`, `qsa`) usando la API de esta página
(`__ns.byTag(raiz, "*")`, etc.). Agregar `:first-of-type`, `:last-of-type`, `:nth-of-type`, `:is()`, `:where()`,
`:has()` (descendiente simple), `:focus` (false), `:hover` (false), `:link`/`:visited` (a con href) y atributos con
comillas, mayúsculas/minúsculas en etiquetas indistintas.

**Serialización** (`innerHTML`/`outerHTML` get): elementos en minúsculas con atributos `k="v"` escapando `& < > "`,
texto escapando `& < >` (no escapar dentro de `script`/`style`), comentarios `<!--x-->`, sin cierre para los
elementos vacíos (`area base br col embed hr img input link meta source track wbr`).

## 5. Eventos

- Cada `EventTarget` (nodos, `window`, `document`, XHR, `AbortSignal`, `MediaQueryList`) guarda sus escuchadores
  en un `WeakMap` → `Map(tipo → [{fn, capture, once, passive, signal}])`. `addEventListener(tipo, fn|{handleEvent}, opciones)`
  acepta booleano u objeto `{capture, once, passive, signal}`; no duplicar el mismo (fn, capture).
- Al agregar el **primer** escuchador de un tipo **sobre un nodo** (no window), llamar `__ns.listen(tipo)`
  (el C ignora repetidos). Llamarlo también al poner una propiedad `onX` o un atributo `onX`.
- `dispatchEvent(ev)`: camino = [target, padres..., document, window] (window solo si el target está en el documento);
  fase de captura de afuera hacia adentro, en el target (escuchadores de captura y luego los demás), y burbujeo si
  `ev.bubbles`. Respetar `stopPropagation`, `stopImmediatePropagation`, `once`, `cancelable`/`preventDefault`,
  `defaultPrevented`, `eventPhase`, `currentTarget`, `target`, `composedPath()`, `timeStamp`, `isTrusted`.
  Excepciones de un escuchador: informar con `__ns.log` y seguir. Devuelve `!defaultPrevented`.
- Manejadores `onX`: como propiedad (`el.onclick = fn`, `window.onload = fn`, `document.onreadystatechange`) y como
  atributo (`onclick="return false"`): el atributo se compila con `new Function("event", código)` con `this` = el
  elemento; si devuelve `false`, `preventDefault()`. Las propiedades `on*` se registran como un escuchador más
  (orden de registro), sin duplicarse al reasignar.
- `__ns.onEvent(target, type)`: crear `MouseEvent` para `click`, `dblclick`, `mouse*`; `KeyboardEvent` para `key*`;
  `FocusEvent` para `focus`/`blur`; `Event` para el resto. `bubbles` true salvo `focus`, `blur`, `load`, `scroll`,
  `mouseenter`, `mouseleave`. `cancelable` true para `click`, `submit`, `key*`, `mouse*`. `isTrusted` true.
  Despachar y devolver `defaultPrevented`.
- Clases: `Event(tipo, {bubbles, cancelable, composed})`, `CustomEvent` (`detail`), `UIEvent` (`view`, `detail`),
  `MouseEvent` (`clientX/Y`, `pageX/Y`, `screenX/Y`, `button`, `buttons`, `ctrlKey`, `shiftKey`, `altKey`, `metaKey`,
  `relatedTarget`), `KeyboardEvent` (`key`, `code`, `keyCode`, `which`, `repeat`, modificadores), `FocusEvent`,
  `InputEvent` (`data`, `inputType`), `initEvent()` legado.

## 6. MutationObserver

Implementación sencilla por comparación: `observe(target, {childList, attributes, characterData, subtree})`.
Las funciones de §4 que modifican el DOM (appendChild, insertBefore, removeChild, replaceChild, setAttribute,
removeAttribute, textContent, innerHTML, data, classList, style...) registran un registro en los observadores
cuyo objetivo es el nodo o un ancestro (con `subtree`). Entregar en un microtask (`queueMicrotask`) agrupados.
`disconnect()`, `takeRecords()`.

## 7. URL y URLSearchParams

`URL(rel, base)` propia: parsear `protocolo://usuario:clave@host:puerto/ruta?query#hash`, resolver relativas
(`/x`, `x`, `../x`, `?q`, `#h`, `//host/x`) contra la base, normalizar `.` y `..`. Propiedades `href`, `origin`,
`protocol`, `username`, `password`, `host`, `hostname`, `port`, `pathname`, `search`, `searchParams` (vivo),
`hash`, `toString`, `toJSON`; `URL.canParse`. `URLSearchParams` desde string, objeto o pares: `get`, `getAll`,
`set`, `append`, `delete`, `has`, `sort`, `toString` (codificar con `encodeURIComponent`, espacios como `+`),
iterable, `forEach`, `size`.

## 8. Red y almacenamiento

Portar desde `eonsurf-polyfill.js` adjunto (que hoy usa `__eonHttp`/`__eonStorage`) a la API nueva:
`__ns.http(id, method, url, body)` + `__ns.onHttp`, y `__ns.storage(origen[, texto])`.
`XMLHttpRequest` completo (`open`, `setRequestHeader`, `send` con string/FormData/URLSearchParams, `abort`,
`readyState`, `status`, `statusText`, `responseText`, `response` según `responseType` "", "text", "json", `responseURL`,
`getResponseHeader('content-type')`, `getAllResponseHeaders`, `onreadystatechange`, `onload`, `onerror`, `onloadend`,
`onprogress`, `ontimeout`, `upload` (EventTarget vacío), `addEventListener`, constantes).
`fetch(input, init)` con `Request`, `Response` (`ok`, `status`, `statusText`, `headers`, `url`, `text()`, `json()`,
`blob()`, `arrayBuffer()`, `clone()`, `body` null), `Headers` (case-insensitive, iterable), soporte de `init.signal`
(AbortController → rechazar con `DOMException` "AbortError"). El cuerpo de FormData/URLSearchParams se envía como
`a=1&b=2`. `localStorage` persistente con `__ns.storage(location.origin)` y guardado diferido (agrupar escrituras en
un `setTimeout` de 500 ms); `sessionStorage` solo en memoria. Ambos con `getItem`, `setItem`, `removeItem`, `clear`,
`key`, `length` y acceso por propiedad (`localStorage.x`) usando `Proxy`. `DOMException(mensaje, nombre)`.

## 9. Reglas

- Un solo archivo `dom.js`, ES2023, autocontenido, sin dependencias, envuelto en un bloque `{ ... }` o IIFE
  para no ensuciar el global salvo lo pedido. Comentarios en castellano, breves.
- Las clases deben tener `Symbol.toStringTag` correcto y `instanceof` debe funcionar (`el instanceof HTMLElement`).
- Los constructores de nodos (`new Element()`) lanzan `TypeError("Illegal constructor")`; `new Text("x")`,
  `new Comment()`, `new DocumentFragment()`, `new Image()`, `new Event()` sí funcionan.
- Las propiedades de los nodos se calculan siempre desde `__ns` (nada de copias que queden viejas).
- Rendimiento: el PPU de la PS3 es lento; evitar recorrer el árbol entero salvo en `querySelector*` y
  `getElementsBy*`; cachear prototipos; no usar `Proxy` donde no haga falta.
- Al final, si `__ns.document()` ya existe y tiene body, `document.readyState = "interactive"`.
