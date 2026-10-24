# EonSurf — El Navegador Web Definitivo para PlayStation 3

> **Created by JuAnKLiMoN**  
> *En memoria y homenaje a 20 años de historia de la PlayStation 3 (2006–2026).*  
> *Agradecimientos técnicos: NetSurf Core Team, PSL1GHT Homebrew SDK, librerías mbedTLS & cURL.*

---

## 1. Misión del Proyecto
El navegador oficial de Sony (basado en NetFront de 2006) quedó obsoleto: certificados SSL caídos ("Not Supported"), errores de memoria constantes y sin soporte para webs modernas.

**EonSurf** nace como un navegador moderno, ultraligero y con aceleración RSX nativa directa, pensado para volar en los 256 MB de RAM de la PS3 y ser el navegador que la consola siempre mereció.

---

## 2. Bloque de Créditos Integrado (En UI y Acerca de)
- **Autor Principal / Creador:** `JuAnKLiMoN`
- **Renderizado Gráfico:** Motor RSX Direct Framebuffer (sin SDL2) a 60 FPS estables.
- **Seguridad:** Pila TLS moderna con soporte de certificados Let's Encrypt y Cloudflare.
- **Teclado:** Integración con el OSK nativo de Sony (`sysutil/osk`).

---

## 3. Características Nativas Confirmadas (Tanda 1)

1. **AdBlocker Ultra-Ligero Nativo (Hosts/DNS Filter - Totalmente Opcional):**
   - Bloqueo en tiempo real de dominios publicitarios y scripts de rastreo antes de descargarlos.
   - Reduce el consumo de RAM hasta un 60% y triplica la velocidad de carga.
   - **Interruptor Opcional (Toggle):** En Ajustes o con un atajo rápido, el usuario puede activarlo o desactivarlo globalmente o en páginas puntuales que exijan apagar el bloqueador para permitir la lectura.
2. **Modo Lectura Limpio ("Reader Mode" con Botón □):**
   - Extrae el texto y fotos principales de artículos, guías de juegos y Wikipedia.
   - Oculta menús molestos y lo presenta en tipografía limpia y grande para TV.
3. **Descargador Directo a USB / Disco Interno:**
   - Detecta archivos `.pkg`, `.zip`, `.mp3`, `.mp4`, `.pdf` e imágenes.
   - Permite elegir destino: `/dev_usb000/` (Pendrive) o `/dev_hdd0/downloads/` (HDD interno).
4. **Modo Ahorro de Datos / Imágenes por Demanda (Opcional):**
   - Interruptor activable para conexiones lentas o foros pesados: muestra cuadros de reserva (*placeholders*) y permite descargar imágenes individuales con un clic o cargar todas juntas.
4. **Modo Oscuro Universal & Modo Claro (Toggle con L3 o Select):**
   - Interfaz en grafito oscuro predeterminada para no cansar la vista en la TV.
   - Inyección CSS inteligente para convertir páginas claras en oscuras respetando imágenes.
5. **Zoom Adaptativo para TV (L2 / R2):**
   - Niveles de zoom rápidos (100%, 125%, 150%, 200%) mediante escalado en el RSX.
6. **Selector de Modo Móvil / Escritorio (User-Agent Switch con R3):**
   - Cambia entre versión de teléfono (más rápida y compacta) y versión de PC completa.
7. **Página de Inicio con Marcadores Rápidos (Speed Dial):**
   - Accesos directos a YouTube liviano (Yo! Player/Invidious), Wikipedia, buscadores ligeros y foros de la comunidad.
8. **Cursor Inteligente ("Magnetic Cursor"):**
   - El puntero se imanta suavemente a enlaces y botones para no renegar apuntando con el stick analógico.
9. **Captura de Pantalla Directa (L1 + R1):**
   - Guarda lo que estás viendo en la web directamente en formato PNG en el disco o pendrive.
10. **Vaciado de Memoria en 1 Toque ("Flush RAM"):**
    - Libera el búfer de páginas anteriores y caché de imágenes al instante para mantener 200+ MB libres.
11. **Guardado de Páginas Offline:**
    - Guarda guías y trucos en el disco para leerlas sin conexión a internet.

---

## 4. Tanda 2: Funciones Innovadoras Únicas para la Comunidad PS3

1. **Instalador de PKGs con 1 Clic ("Direct PKG Installer"):**
   - Si descargas un archivo `.pkg` desde una web o repositorio homebrew, EonSurf lo guarda en `/dev_hdd0/packages/` y lo deja listo para instalar en Package Manager al salir, sin tener que moverlo con exploradores externos.
2. **Historial y Pestañas en Tarjetas Visuales (Visual Tab Switcher):**
   - Vista en miniaturas navegables con la cruceta (estilo carrusel) sin mantener páginas pesadas en RAM.
3. **Buscador de Texto en Página ("Find in Page"):**
   - Presionando triángulo o desde el menú, busca palabras clave dentro de guías largas de juegos resaltándolas en pantalla.
4. **Reproductor de Música / Radio Web de Fondo (Background Audio Streamer):**
   - Capacidad de sintonizar radios online o pistas MP3 livianas mientras se lee o navega.
5. **Generador y Lector de Códigos QR en Pantalla:**
   - ¿Encontraste un enlace largo o un video que querés ver en tu celular? EonSurf muestra un código QR en la pantalla de la TV para escanearlo con el teléfono al instante.
6. **Monitor de Hardware en Tiempo Real (OSD de Rendimiento):**
   - Muestra en una esquina discreta (opcional): FPS (60 clavados), Temperatura del CELL/RSX y Memoria RAM libre en MB.

---

## 5. Tanda 3: Integración de Consola, Accesorios y Experiencia de Usuario

1. **Soporte Nativo de Teclado y Mouse (USB / Bluetooth):**
   - Si conectas un teclado o mouse a los puertos USB de la PS3, EonSurf los detecta automáticamente:
     - `Ctrl + L` o `F4`: Enfocar barra de direcciones.
     - `Ctrl + R` o `F5`: Recargar página.
     - `Ctrl + W`: Cerrar pestaña actual.
     - `Ctrl + + / -`: Zoom en pantalla.
     - Rueda del mouse para scroll suave.
2. **Atajos Maestros de Mando (DualShock 3):**
   - **Start:** Abrir barra de direcciones / Teclado OSK de Sony.
   - **Select:** Menú rápido de marcadores y configuración.
   - **L1 / R1:** Página Atrás / Adelante.
   - **L2 / R2:** Zoom dinámico (- / +).
   - **Stick Derecho:** Paneo libre y scroll rápido.
   - **R3:** Alternar modo móvil / escritorio.
   - **L3:** Alternar tema oscuro / claro.
3. **Control del Ventilador Dinámico (Cooling Guard):**
   - Opción para aumentar un 5% el régimen del ventilador de la consola mientras se navega, evitando que el chip CELL caliente de más si se leen páginas largas en verano.
4. **Modo Cine / Pantalla Completa (F11 / Botón Cuadrado prolongado):**
   - Oculta la barra de direcciones y bordes para aprovechar el 100% de la TV al ver fotos o leer contenido.
5. **Modo Navegación Privada / Modo Incógnito:**
   - Navega sin guardar historial, cookies ni rastro en el disco duro interno de la PS3.

---

## 6. Tanda 4: Funciones Legendarias No Obvias (Creadas para EonSurf)

1. **"Resucitador de Links Caídos" (Archive.org / Wayback Machine):**
   - Si una página da Error 404 o el servidor no responde, EonSurf muestra una advertencia clara:
     `"Sitio fuera de línea. ¿Desea consultar la última copia histórica en Wayback Machine (Archive.org)?"`
   - No abre nada automático: solo si el usuario confirma, consulta la copia archivada.
2. **Centro de Control webMAN MOD Integrado:**
   - Detecta si la consola corre webMAN (`http://127.0.0.1:80`) y ofrece un panel directo para montar juegos ISO, ver temperaturas en vivo y regular ventilación con 1 botón.
3. **Soporte Nativo de Gemini (`gemini://` - The Smolnet):**
   - Primer navegador en consolas PlayStation en soportar la red Gemini. Permite leer miles de blogs, artículos y enciclopedias sin publicidad ni JavaScript a velocidad instantánea (2 MB de RAM).
4. **Descompresor Nativo ZIP / 7z con Soporte de Contraseña:**
   - Motor ligero en C (`miniz` / `libarchive`). Al descargar parches, mods o traducciones comprimidas:
     - Pregunta: `¿Desea extraer en este directorio?`
     - Si el archivo tiene clave, abre el teclado OSK oficial de Sony para ingresar la contraseña y descomprime directo en el disco interno.
     - **Visor de Contraseña:** Opción para alternar entre asteriscos `****` y texto visible para no equivocarse al escribirla con el joystick.
     - **Pegar Clave:** Botón directo para pegar una contraseña que se haya copiado previamente de un foro o página web mientras se navegaba.
5. **Sistema de Selección, Copiado y Pegado de Texto:**
   - Manteniendo presionado $\times$ sobre el texto y arrastrando el stick analógico, se resalta la frase deseada.
   - Menú contextual: *Copiar al portapapeles*, *Buscar en Google/DuckDuckGo* o *Pegar en la barra de direcciones / cuadros de texto / campos de clave*.
6. **"EonPush" / "Send to PS3" (Mandar pestañas desde Celular / PC a la PS3):**
   - EonSurf incorpora un mini-servidor local ultraligero en segundo plano (puerto `8088`).
   - Escaneas un código QR en la TV con tu celular o abres una mini-web en tu PC, pegas cualquier enlace y al tocar **"Enviar"**, la PS3 abre esa página de inmediato.
   - Sin cables, sin escribir links largos con el joystick y sin necesidad de servicios de terceros.

---

## 7. Tanda 5: Innovaciones No Obvias de Próxima Generación

1. **"EonPull" (Lo inverso a EonPush):**
   - ¿Estás leyendo una guía o viendo una imagen en la PS3 y tenés que apagar la consola o irte a otra habitación?
   - Presionas un botón y la PS3 te genera un código QR temporal en pantalla. Lo escaneas con el celular y continuas leyendo la misma página web exactamente donde la dejaste en la TV.
2. **Generador de Portadas y Miniaturas para Juegos (Cover Grabber Automático):**
   - Si estás buscando carátulas o fondos para juegos de PS3, PS2 o PS1 en internet, con pararte sobre cualquier imagen y tocar $\triangle$, te da la opción:
     `"Guardar como carátula de juego"` $\rightarrow$ la convierte automáticamente al formato y tamaño exacto para multiMAN/webMAN (`ICON0.PNG` o `COVER.JPG`) y la guarda en la carpeta del juego.
3. **Traductor Instantáneo de Páginas en 1 Toque:**
   - Para leer foros o wikis en inglés, japonés o francés: utiliza una API ligera y gratuita de traducción de texto (como LibreTranslate/Google Translate Lite) que traduce el artículo a español sin recargar ni romper la estructura de la web.
4. **Caché Persistente en Disco Anti-Cuelgues ("Crash Resume"):**
   - Si la consola se llega a apagar de golpe, se corta la luz o cierras la app, EonSurf guarda el estado de la última página activa en `/dev_hdd0/game/EONSURF01/USRDIR/session.json`. Al volver a abrir el navegador te pregunta:
     `"¿Desea restaurar la sesión anterior?"` y vuelve exactamente a la página en la que estabas.
5. **Manejador Nativo de Descargas Directas (MediaFire, MEGA, Google Drive, Archive.org):**
   - En lugar de cargar páginas web pesadas llenas de pop-ups y captchas imposibles para una consola:
     - **MediaFire:** EonSurf detecta la URL e intercepta automáticamente el enlace directo al archivo real (`.pkg`, `.zip`, `.iso`), iniciando la descarga directa a USB o disco sin publicidad ni botones falsos.
     - **MEGA.nz:** Integra un descifrador ligero directo en C (`megatools / REST API`) para descargar links públicos sin exigir el motor pesado de MEGA en navegador.
     - **Archive.org / GitHub Releases / 1Fichier:** Descarga con barra de velocidad en MB/s y porcentaje en tiempo real.
6. **Lanzador / Acceso a Emuladores Nativos vs Web:**
   - La emulación en navegadores modernos (WebAssembly/HTML5 Canvas) requiere navegadores de PC con gigabytes de RAM que colapsarían la PS3.
   - **La solución inteligente en EonSurf:** Si entras a una web de ROMs o juegos retro, EonSurf te permite descargar el juego en 1 clic y enviarlo automáticamente a la carpeta de emuladores nativos de la PS3 (RetroArch / FB Neo / emuladores de PS1-PS2) para jugarlo a 60 FPS fluidos por hardware real en vez de por una web trabada.
7. **"EonAI" — Asistente de Inteligencia Artificial Gemini Integrado:**
   - La web oficial de `gemini.google.com` requiere Chrome con Angular, WebGL y 4 GB de RAM.
   - **La solución en EonSurf:** En lugar de abrir la web pesada, EonSurf incorpora un cliente nativo ultra-liviano que se conecta directamente a la API oficial de Google Gemini (`gemini-2.5-flash` / endpoint REST):
     - Abre una ventana lateral o pestaña limpia con fondo oscuro: preguntas con el teclado de la PS3 o pegando texto, y Gemini responde en segundos.
     - Cero consumo de RAM excesivo (apenas 1 MB para manejar el JSON y el texto).
     - La primera consola retro en la historia en tener acceso nativo a Inteligencia Artificial conversacional.



---

## 10. Tanda 6: Usabilidad TV, D-Pad y Manejo de Medios

1. **Navegación Espacial Directa con Cruceta (D-Pad Link Jumping - Opcional):**
   - Alternativa al cursor de ratón libre: la cruceta salta de un enlace al siguiente de forma lógica resaltando la caja en neón.
   - Activación opcional en ajustes para quienes prefieran navegar botón a botón en vez de mover el ratón virtual.
2. **Sugerencias y Autocompletado Predictivo:**
   - Mientras se escribe en la barra de búsqueda o en el Startpage, despliega sugerencias de dominios populares y páginas ya visitadas en el historial.
3. **Detector y Reproductor Nativo de Videos (Video Pop-out):**
   - Si la página web contiene un video (`.mp4`, `.mkv`, stream `m3u8`), EonSurf lo detecta y muestra una barra limpia:
     `[▶ Reproducir a pantalla completa]` o `[💾 Descargar a USB/Disco]`.
   - Aísla el video de los scripts de la página para reproducirlo suavemente por hardware RSX.
4. **Memoria de Preferencias por Sitio Web (Opcional):**
   - Recuerda individualmente por cada web: nivel de zoom preferido (125%, 150%), modo móvil o modo oscuro, para no tener que reconfigurarlo cada vez que se entra.
5. **Aceleración y Modos de Velocidad del Cursor con R3 (Configurable):**
   - El stick analógico cuenta con un selector de velocidades escalonadas con un clic en **R3**:
     - *Modo 1 (Precisión / Lento):* Mueve el cursor píxel por píxel para seleccionar texto fino o hacer clic en botones diminutos.
     - *Modo 2 (Normal - Predeterminado):* Velocidad estándar balanceada.
     - *Modo 3 (Rápido):* Para televisores grandes de 55 pulgadas o más.
     - *Modo 4 (Ultra Rápido):* Cruza toda la pantalla en una fracción de segundo.
     - Un pequeño indicador temporal en una esquina muestra el modo activo: `[Velocidad: Normal / Rápida / Ultra]`.
6. **"Air Mouse" Sixaxis con Calibración de Zona Muerta (Inspirado en EonCT):**
   - Basado en los aprendizajes de `EonCT` (con giroscopio y control de apuntado fino):
     - **Zona Muerta Anti-Temblor (Deadzone):** Evita que el pulso natural de la mano mueva el cursor cuando el joystick está quieto.
     - **Sensibilidad Ajustable:** En el menú de opciones (Baja, Media, Alta).
     - **Interruptor Rápido (L3 prolongado):** Activa o congela el modo Air Mouse en cualquier momento para apoyar el control en la mesa sin que el puntero se dispare.

7. **Teclado Sony OSK Oficial:**
   - Confirmado: El teclado oficial de Sony ya cuenta con atajos nativos de terminaciones (`.com`, `.net`, `.org`) en la barra de operaciones rápida al ingresar URLs.


Inspirado en la popular aplicación **Downloader by AFTVnews** de Android TV / Fire TV, adaptado nativamente para resolver el problema de escribir URLs largas con el mando de la PS3:

### ¿Cómo funciona la compatibilidad con códigos de Downloader?
1. **Detección Automática Inteligente (Sin límite superior de dígitos):**
   - El usuario abre la barra de búsqueda y en lugar de una URL larga como `https://github.com/.../mi-app.pkg`, escribe simplemente un número (ej. `75077`, `123456`, `8492019`, etc.).
   - EonSurf analiza la entrada: si detecta una secuencia de **4 dígitos en adelante** compuesta exclusivamente por números (sin letras, puntos de dominio ni barras), reconoce automáticamente que es un código numérico de redirección.
   - **Diseño a prueba de futuro:** No tiene límite de 6 dígitos; a medida que la base de datos crezca a 7, 8 o más cifras con el paso de los años, el sistema seguirá funcionando a la perfección de por vida.
2. **Uso de la misma Base de Datos existente (AFTVnews Redirector):**
   - EonSurf consulta la cabecera HTTP de `https://go.aftvnews.com/[código]`.
   - El servidor responde con una redirección estándar `302 Found` entregando la URL real de destino en milisegundos.
   - Cero necesidad de crear servidores propios; **aprovecha los millones de códigos que ya existen en internet y en tutoriales de YouTube**.
3. **Acción de Descarga o Navegación:**
   - Si el enlace final es un archivo instalable o comprimido (`.pkg`, `.zip`, `.mp4`): EonSurf salta a la pantalla de descarga directa preguntando si guardar en USB o Disco.
   - Si el enlace final es una página web: EonSurf carga la web directamente en pantalla.
4. **Modo Teclado Numérico Exclusivo (Opcional con Select):**
   - Al activar el modo numérico, el teclado OSK de la PS3 se abre directamente en el bloque de números (0-9), haciendo que ingresar un código tome literalmente 3 segundos.

Inspirada en las mejores interfaces de Android TV / Smart TV modernas, pensada para leerse a 3 metros del sillón con tipografía grande y navegación 100% fluida con la cruceta del DualShock 3:

```text
+-------------------------------------------------------------------------------+
|  EonSurf                [ 21:30 | Vie 24 Oct ]               [ 22°C Buenos Aires ]
+-------------------------------------------------------------------------------+
|                                                                               |
|            [ 🔍  Buscar en internet o escribir dirección web... (Start) ]      |
|                                                                               |
|   ACCESOS RÁPIDOS (Speed Dial - Editables):                                   |
|   +---------------+  +---------------+  +---------------+  +---------------+  |
|   |  ▶ Yo! Player |  |  W Wikipedia  |  |  📻 Radios    |  |  🌐 Reddit    |  |
|   |  (YouTube TV) |  |  (Enciclopedia|  |  (Streaming)  |  |  (Foros/PS3)  |  |
|   +---------------+  +---------------+  +---------------+  +---------------+  |
|   +---------------+  +---------------+  +---------------+  +---------------+  |
|   |  ☁ MediaFire  |  |  💾 Disco PS3 |  |  ⚙ webMAN     |  |  ➕ Agregar   |  |
|   |  (Descargas)  |  |  (/dev_usb000)|  |  (Control OSD)|  |  (Marcador)   |  |
|   +---------------+  +---------------+  +---------------+  +---------------+  |
|                                                                               |
+-------------------------------------------------------------------------------+
```

### Características del Dashboard:
1. **Fila Superior de Estado (Widgets en Vivo):**
   - **Hora y Fecha en Tiempo Real:** Grande y elegante, sincronizada con el reloj de la PS3.
   - **Widget del Clima:** Muestra icono de estado, temperatura actual y nombre de tu ciudad.
2. **Configuración de Ubicación Personalizada:**
   - En el menú de Configuración $\rightarrow$ *"Ubicación / Clima"*:
     - Campo editable donde el usuario escribe su ciudad o localidad (ej. *"Buenos Aires"*, *"Madrid"*, *"Bogotá"*).
     - Se guarda en el archivo de preferencias `/dev_hdd0/game/EONSURF01/USRDIR/eonsurf.cfg` para que nunca se pierda.
     - Consulta la API ligera de `wttr.in` en segundo plano cada 30 minutos sin gastar recursos.
3. **Barra de Búsqueda Central Destacada:**
   - Caja grande y visible con esquinas redondeadas.
   - Al pararse en ella con el mando o tocar **START**, se abre el teclado oficial de Sony para buscar al instante (con DuckDuckGo / Google Lite).
4. **Tarjetas de Acceso Rápido (Speed Dial) 100% Editables:**
   - Vienen preinstaladas con lo mejor que definimos:
     - **Yo! Player:** YouTube sin anuncios adaptado para TV.
     - **Wikipedia:** Enciclopedia rápida.
     - **Radios:** Selector de streaming de música y noticias.
     - **Reddit / PSX-Place (Opcional):** Foros limpios con formato `.json`.
     - **MediaFire / Nube:** Acceso a repositorios de descarga directa.
     - **Disco PS3 / USB:** Explorador de archivos local.
     - **webMAN MOD:** Centro de control de la consola.
   - **Personalización total:** El usuario puede pulsar **$\triangle$** sobre cualquier tarjeta para borrarla, editar la URL o moverla, o pulsar **[+]** para guardar cualquier página web favorita.
5. **Animación de Foco Clara para TV:**
   - Al mover la cruceta o el stick sobre las tarjetas, la tarjeta seleccionada aumenta ligeramente de tamaño con un borde neón cian brillante para que nunca te pierdas en la pantalla.

---

## 11. Ecosistema Homebrew, HEN / CFW y Explotación Segura

### Compatibilidad y Lanzador de Exploits (HEN / HAN / Toolset):
* **El dilema de los exploits en navegadores alternativos:**
  - Los instaladores web de HEN (como PS3Toolset de bguerville o los scripts de ps3xploit) no son páginas web normales: dependen de vulnerabilidades de memoria muy específicas del motor viejo de Sony (NetFront / Flash 9 / WebKit antiguo) para inyectar payloads en el kernel Lv2.
* **La Solución Maestra en EonSurf ("Exploit Pass-Through / Auto-Updater"):**
  - **Detección de Sitios de Hack:** Si el usuario ingresa a una web de instalación de HEN o PS3Toolset (`ps3xploit.me`, `ps3toolset.com`, etc.):
    - EonSurf lo reconoce y le ofrece:  
      `"Se ha detectado una herramienta de modificación del sistema. ¿Desea ejecutarla mediante el motor del sistema o descargar el instalador offline PKG?"`
  - **Descargador e Instalador Offline de HEN:** EonSurf permite descargar directamente los paquetes oficiales actualizados (`Latest_HEN_Installer_signed.pkg` y `PS3HEN.BIN`) y dejarlos listos en `/dev_hdd0/packages/` sin depender de que la web del exploit esté caída o dé error de SSL.
  - **Actualizador de Homebrew en 1 Clic:** Detecta si hay una nueva versión de HEN, webMAN MOD o multiMAN y permite actualizarla con 1 toque.

---

## 12. Perspectiva Histórica: La Lección de PSP (NetFront High-Memory)
* **La PSP (PlayStation Portable):** Tenía el mismo motor NetFront que la PS3, pero Sony limitó por software el uso de memoria a solo 4 MB, haciendo que el 90% de las webs dieran el error de "Memoria Insuficiente".
* **Lo que aprendimos del hack "High Memory Mod" de PSP:** Liberar los 32/64 MB completos de la PSP cambió la historia. En EonSurf aplicamos la misma filosofía:
  - Liberamos **el 100% de la memoria disponible de la PS3** (sin las restricciones artificiales del navegador original de Sony), usando memoria RSX dedicada para video y almacenamiento en disco para la caché pesada.

---

---

## 14. La Brecha de 2026: Lo que Chrome/Firefox tienen de fábrica y NetSurf NO trae
Para que EonSurf no se quede como un navegador "retro congelado en 2006", implementamos de forma nativa las 7 comodidades esenciales que todo usuario moderno da por sentadas:

1. **Gestor de Descargas Moderno ("Download Shelf" emergente):**
   - NetSurf original solo guarda un archivo y se olvida.
   - **En EonSurf:** Al terminar una descarga, aparece una notificación abajo a la derecha:
     - **[Abrir archivo]:** Si es una foto o documento de texto, lo abre en el navegador. Si es un `.pkg`, pregunta si instalarlo.
     - **[Mostrar en carpeta]:** Te lleva directamente al directorio del disco (`/dev_hdd0/downloads/` o USB) donde quedó guardado.
     - Historial de descargas accesible desde el menú principal con tamaño y fecha.
2. **Scroll Suave con Inercia Física ("Smooth Inertial Scrolling"):**
   - NetSurf clásico hace un salto brusco línea por línea que marea en una TV de 50".
   - **En EonSurf:** El stick derecho y los botones de scroll mueven la pantalla con desaceleración física suave a 60 FPS fijos, como deslizar el dedo en un celular o tablet moderna.
3. **Visor Nativo de Imágenes y Documentos (Photo & Text Viewer):**
   - Si haces clic en una foto (`.png`, `.jpg`, `.webp`) o un archivo de texto/código (`.txt`, `.log`, `.nfo`), EonSurf lo abre en pantalla completa con controles de zoom y paneo sin descargarlo como un archivo extraño.
4. **Buscador de Texto en Página Visual ("Find in Page"):**
   - Al tocar **$\triangle$** $\rightarrow$ Buscar palabra: resalta todas las coincidencias en amarillo brillante y te permite saltar entre la siguiente y la anterior con **L1 / R1**.
5. **Historial Completo con Buscador y Agrupación por Fecha:**
   - Organizado por *"Hoy"*, *"Ayer"*, *"Esta semana"*, con opción de borrar historial de la última hora o completo en 1 toque.
6. **Autocompletado de Formularios y URLs Inteligente (Omnibox):**
   - Al empezar a tipear en la barra, sugiere tanto sitios web del historial como búsquedas populares, sin necesidad de tipear la dirección completa.
7. **Barra de Progreso y Estado de Carga Discreta:**
   - Una línea cian delgada superior que avanza suavemente mostrando el porcentaje real de carga (10%... 50%... 100%), para saber en todo momento si la conexión está trabajando o si la página ya terminó.

---

## 15. Soporte Multi-Idioma Nativo (Internacionalización - i18n)
EonSurf se diseña para una audiencia global con detección automática del idioma de la consola PS3 mediante la API oficial de Sony (`sysUtilGetSystemParam`):

### Idiomas Soportados de Fábrica:
1. 🇪🇸 **Español:** Idioma principal nativo (Argentina / España / Latinoamérica).
2. 🇺🇸 **Inglés:** Estándar internacional para la comunidad anglosajona.
3. 🇧🇷 **Portugués (Brasil / Portugal):** Comunidad masiva de PS3 en Brasil y Portugal.
4. 🇫🇷 **Francés:** Amplia base homebrew europea.
5. 🇩🇪 **Alemán:** Comunidad técnica europea.
6. 🇮🇹 **Italiano:** Escena activa de emulación y mods.
7. 🇷🇺 **Ruso:** Una de las comunidades más grandes del mundo en PS3 HEN y preservación.
8. 🇯🇵 **Japonés:** Idioma de origen de la consola.

- **Arquitectura ligera en JSON:** Las cadenas de texto se cargan desde un archivo plano de apenas 20 KB (`lang_es.json`, `lang_en.json`), permitiendo a la comunidad traducir el navegador a más idiomas sin recompilar el código.

---

## 16. Portabilidad a Otras Consolas y Sistemas Obsoletos
Gracias a que el núcleo de EonSurf está escrito en C puro desacoplado mediante una superficie de dibujo abstracta (`surface_ps3.c`), el 90% del código del navegador es reutilizable para compilar en otras plataformas legendarias:

1. **Xbox Clásica (2001 - 64 MB de RAM):**
   - El gran dolor de cabeza histórico: solo existía *LinksBoks* (modo texto tosco).
   - EonSurf en Xbox Clásica corre holgadamente en sus 64 MB de RAM, trayendo por primera vez navegación gráfica real con soporte de mandos Duke / S.
2. **PlayStation Vita (VitaSDK / LiveArea):**
   - La comunidad de Vita depende de parches (`iTLS-Enso`) porque el navegador de fábrica colapsa con SSL.
   - EonSurf en Vita ofrece navegación táctil fluida, descargas directas a `ux0:data/` y compatibilidad nativa con GitHub/MediaFire.
3. **PlayStation Portable (PSP):**
   - El entorno NetFront original de PSP crasheaba por falta de memoria. EonSurf optimizado para pantalla 480x272 con bajo consumo revive la navegación en PSP.
4. **Raspberry Pi 1 / Pi Zero (512 MB de RAM o menos):**
   - Donde Chromium y Firefox se cuelgan al arrancar, EonSurf (versión Linux framebuffer/X11) consume 25 MB de RAM y vuela a 60 FPS.
5. **Netbooks antiguas / PCs con procesador Intel Atom y 1 GB de RAM:**
   - La solución ligera definitiva para computadoras recicladas que ya no soportan Windows 10 ni navegadores modernos pesados.

---

## 19. Lecciones Técnicas de Compilación y Solución de Errores (Post-Mortem)
Registro crítico de problemas encontrados durante el desarrollo de EonSurf para evitar reincidencias en futuras compilaciones y versiones:

### 1. Error de Cuelgue en RPCS3: `Illegal opcode 0x00000000 at 0x7c0802a4`
* **Causa:** Uso de `make_self_npdrm` al firmar el binario `EBOOT.BIN`. Esta herramienta cifra los segmentos de código con licencias DRM de Sony para distribución comercial cerrada. Los emuladores como RPCS3 no pueden desencriptar estos bloques y leen ceros binarios en memoria, provocando el cierre inmediato con el cartel: *"The PS3 application has likely crashed"*.
* **Solución Obligatoria:** Firmar siempre con **`make_fself`** (o `fself` del SDK PSL1GHT). El formato FSELF produce binarios de depuración libres de encriptación que arrancan de forma instantánea tanto en emulador RPCS3 como en consolas reales con HEN o Custom Firmware (CFW).

### 2. Iconos Antiguos Visibles en RPCS3 tras Actualizar la Aplicación
* **Causa:** RPCS3 mantiene un directorio de caché compiled (`cache/EONSURF01/` y bases de datos locales) donde congela los iconos `ICON0.PNG` y títulos previos.
* **Solución Obligatoria:** 
  1. Copiar `ICON0.PNG` (320x176) y `PIC1.PNG` (1920x1080) tanto dentro del paquete como directamente en `dev_hdd0/game/EONSURF01/`.
  2. Vaciar el directorio `cache/EONSURF01/` para forzar a RPCS3 a regenerar la miniatura gráfica.

### 3. Registro de Superficie en `libnsfb`:
* **Causa:** Usar la estructura antigua `nsfb_surface_rt_t` produce errores de compilador en GCC.
* **Solución Obligatoria:** Utilizar la estructura moderna `nsfb_surface_rtns_t` con macro `NSFB_SURFACE_DEF(ps3, NSFB_SURFACE_RAM, &ps3_rt_surface)`.


### 1. Actualizador Automático Nativo (Inspirado en webMAN MOD & PKGi):
* **El problema de las primeras versiones:** Lanzar la v1.0 y que los usuarios tengan que usar una PC y un pendrive cada vez que arreglamos un detalle o agregamos una función arruina la adopción.
* **El mecanismo de EonUpdate (100% Automático):**
  1. **Comprobación Silenciosa al Iniciar:**  
     Al abrir EonSurf, realiza una consulta HTTP GET ligera (2 KB) a la API de GitHub Releases (`/repos/juanklimonplay/EonSurf/releases/latest`).
  2. **Comparación Semántica de Versión:**  
     Compara la versión local del archivo `PARAM.SFO` (ej. `v1.0.0`) con el `"tag_name"` de GitHub (ej. `v1.0.1`).
  3. **Aviso en Pantalla Amigable:**  
     Si hay una versión nueva, muestra una ventana emergente:  
     `"¡Nueva versión disponible de EonSurf (v1.0.1)! ¿Deseas actualizar ahora?"`  
     `[Sí, actualizar] — [Más tarde]`
  4. **Descarga e Instalación en 1 Toque:**  
     Descarga el `.pkg` nuevo directamente a `/dev_hdd0/packages/` y mediante una llamada de sistema (syscall de HEN/CFW) lo instala automáticamente y reinicia la app sin necesidad de pendrive ni PC.
  5. **Modo Privado / Repositorio Público:**  
     Compatible tanto con repositorios públicos de GitHub como con un archivo JSON de respaldo alojado en la nube.

---

### 2. "EonFeedback / Safe Crash Recovery" (Diagnóstico para Mejoras Continuas):
* **Caja de Arena Anti-Cuelgues (Sandbox Safe Mode):**  
  Si el navegador se cierra de forma inesperada por una página web maliciosa o saturada de memoria, al volver a abrir muestra una opción de arranque a prueba de fallos:  
  `"EonSurf se cerró inesperadamente. ¿Deseas iniciar en Modo Seguro (sin pestañas previas ni caché)?"`
* **Log de Diagnóstico Local (`crash.log`):**  
  Guarda en `/dev_hdd0/game/EONSURF01/USRDIR/debug.log` los últimos errores sin mandar nada a Sony, permitiendo al usuario ver exactamente qué pasó o reportar el error en el repositorio para que lo solucionemos de inmediato.
* **Canal Directo de Novedades y Changelog ("Qué hay de nuevo"):**  
  Una pestaña en Ajustes que muestra la lista de mejoras añadidas en la última actualización para que el usuario conozca las nuevas funciones apenas actualice.


---

## 20. Blindaje T�cnico de Bajo Nivel y Optimizaci�n Extrema (7 Pilares de Rendimiento PS3)

Para garantizar estabilidad absoluta de nivel comercial y prevenir cuellos de botella del Kernel Lv2 y el procesador Cell:

### 1. Pila de Red y Sockets en Lv2 (\sys_net\ / \libcurl\)
- **L�mites Estrictos de Conexi�n (\eonsurf.cfg\):** Configurar \max_fetchers: 8\, \max_fetchers_per_host: 4\ y \max_cached_fetch_handles: 4\ (\CURLMOPT_MAX_HOST_CONNECTIONS\). Previene el agotamiento de descriptores de socket en el kernel Lv2.
- **Agrandar Buffer TCP (\SO_RCVBUF\):** Aplicar \setsockopt()\ con \SO_RCVBUF\ a 256 KB (frente a los 32 KB por defecto de Sony) para maximizar descargas de archivos pesados.
- **Reutilizaci�n de Sesiones SSL (\CURLOPT_SSL_SESSIONID_CACHE\):** Habilitado en 1L para guardar el Session ID/Ticket y evitar renegociar TLS en p�ginas del mismo dominio.

### 2. Memoria XDR y Asignador Custom Anti-Fragmentaci�n (TLSF)
- **El problema de Newlib:** El \malloc()\ b�sico fragmenta el Heap al crear y destruir miles de nodos DOM/CSS peque�os.
- **Soluci�n:** Integraci�n de la librer�a **TLSF** (*Two-Level Segregated Fit*) de tiempo constante \(1)\$.
- **Pool Dedicado:** Reserva al iniciar de un bloque �nico de **64 MB** en RAM XDR con \sysMemAllocate()\. Todas las asignaciones de \libdom\, \libcss\ y \libhubbub\ se realizan dentro de este pool y se resetean at�micamente por pesta�a (\rena_destroy()\).

### 3. Descargador Directo a \/dev_hdd0/packages\ a 60 FPS
- **Ring Buffer en RAM (1 a 2 MB):** Evita la saturaci�n del bus SATA I de la PS3. Se acumulan fragmentos de red en RAM y se realiza un �nico \sysFsWrite()\ secuencial desde un hilo secundario con prioridad Lv2 baja (\1001\).
- **Pre-asignaci�n con \sysFsFtruncate\:** Comprueba el encabezado \Content-Length\ y el espacio libre en disco antes de descargar.
- **Descargas At�micas (\.part\):** Guarda como \rchivo.pkg.part\ y renombra a \.pkg\ �nicamente tras validar el tama�o final o suma de verificaci�n (checksum).

### 4. Fuentes Nativas del Firmware y Atlas de Glifos en XDR
- **Sin Inflar el PKG:** Carga directa de las fuentes TrueType oficiales del firmware en \/dev_flash/data/font/SCE-PS3-VR-R-LATIN.TTF\ (y \ROD-R\).
- **Atlas de Glifos Rasterizados:** Cada car�cter se renderiza una �nica vez con FreeType2 en una textura atlas en RAM XDR; el scroll realiza blitting directo por DMA al RSX sin recalcular curvas vectoriales en la CPU.

### 5. AdBlocker Ultraligero Nativo por Hash FNV-1a (Cero Regex)
- **Sin Motor Regex Pesado:** Evita evaluar expresiones regulares complejas en la PPU.
- **Tabla Binaria Compacta (120 KB):** Lista de 15.000 dominios de telemetr�a y anuncios pre-hasheados con FNV-1a (64-bit). Se eval�a en tiempo \(1)\$ antes de que el motor resuelva la consulta DNS.

### 6. Integraci�n con el Sistema Operativo PS3 y Calidad de Vida (QoL)
- **Inhibici�n de Salvapantallas:** Llamada peri�dica de actividad al sistema para evitar que el XMB apague la pantalla durante la lectura o descargas largas.
- **Salida Limpia con Bot�n PS (\SYSUTIL_EXIT_GAME\):** Cierra descriptores abiertos en disco, detiene hilos de SPU y libera el contexto RSX para evitar triples pitidos de la consola.
- **Certificados CA Actualizados (\cacert.pem\):** Ubicados en \/dev_hdd0/game/EONSURF01/USRDIR/certs/cacert.pem\, actualizables por FTP o pendrive.

### 7. Alineaci�n Estricta DMA para SPUs (MFC)
- **Alineaci�n a 128 Bytes:** Todos los descriptores de la cola circular FIFO y buffers de paquetes SPU se declaran con \__attribute__((aligned(128)))\ y transferencias m�ltiplos de 16 bytes para alcanzar el ancho de banda pico del bus EIB (204.8 GB/s).

### 4. Resoluci�n Obligatoria de Stubs PRX con \sprxlinker\:
* **Causa:** Un binario ELF compilado directamente con \powerpc64-ps3-elf-gcc\ contiene stubs sin resolver (\ x00000000\) para las librer�as din�micas del sistema de Sony (\sysutil\, \sx\, \
et\, \io\). Al ejecutarse en RPCS3 o consola real, cualquier llamada provoca un salto a memoria no mapeada con error: *\VM: Access violation executing location 0x7c0802a4 / Unknown opcode: 0x00000000\*.
* **Soluci�n Obligatoria:** Ejecutar siempre **\sprxlinker EonSurf.elf\** inmediatamente despu�s de la compilaci�n de GCC y antes de la firma con \make_fself\. \sprxlinker\ parchea los stubs del ELF vincul�ndolos con la tabla OPD real del sistema operativo PS3.

### 8. Detalles Quir�rgicos de Implementaci�n en C�digo C (PSL1GHT & Lv2)

1. **Prioridad del Hilo de Escritura SATA (\sysThreadCreate\):**
   - En el kernel Lv2 de PS3, un n�mero de prioridad menor significa mayor prioridad de CPU (0 es la m�xima, 3071 la m�nima).
   - El hilo principal corre en prioridad est�ndar (\1001\).
   - El hilo de volcado a disco para descargas de \.pkg\ se instancia con **prioridad \1500\ o \2000\**. De este modo, el planificador del Cell jam�s roba ciclos al bucle de renderizado ni pierde un solo VSync del RSX (garantizando los 60 FPS inquebrantables).

2. **Apertura de Fuentes en \/dev_flash\ con Flag Estricto \SYS_O_RDONLY\:**
   - La fuente \/dev_flash/data/font/SCE-PS3-VR-R-LATIN.TTF\ se debe abrir estrictamente en modo solo lectura (\SYS_O_RDONLY\).
   - Solicitar permisos de escritura sobre \/dev_flash\ dispara la protecci�n del hipervisor de Sony en CFW Evilnat o PS3HEN, provocando un fallo de descriptor (\EBADF\ o \EACCES\).

3. **B�squeda Binaria (\search\) sobre la Tabla FNV-1a de 120 KB:**
   - Al compilar la tabla est�tica binaria de 15.000 dominios, los hashes \uint64_t\ se guardan estrictamente ordenados de menor a mayor.
   - La evaluaci�n en tiempo de ejecuci�n en la PPU utiliza la funci�n est�ndar \search()\.
   - Se determina si un dominio es publicitario en un m�ximo de **14 operaciones de comparaci�n** (\$\log_2(15000) \approx 13.8\$), operando directamente dentro de los 512 KB de la cach� L2 de la PPU sin tocar la RAM.

---

## 21. Matriz de Ingenier�a Definitiva de EonSurf

| M�dulo / Subsistema | Par�metro / Constante de Bajo Nivel | Mecanismo en PSL1GHT / C | Garant�a en Consola F�sica PS3 |
| :--- | :--- | :--- | :--- |
| **Stack de Red (\sys_net\)** | \max_fetchers: 8\ / \per_host: 4\ / \SO_RCVBUF: 256KB\ | Reutilizaci�n \CURLOPT_SSL_SESSIONID_CACHE = 1L\ | Descargas a tope de l�nea sin agotar sockets del kernel Lv2. |
| **Memoria DOM/CSS** | Pool est�tico XDR de **64 MB** | Asignador **TLSF** (\(1)\$) + \rena_destroy()\ por pesta�a | Cero fragmentaci�n de memoria en sesiones de uso prolongadas. |
| **I/O Disco Duro (SATA I)** | Buffer RAM de **2 MB** + Extensi�n \.part\ | \sysFsFtruncate()\ + \sysFsWrite()\ en hilo secundario (Prioridad ~1500) | Descargas limpias a \/dev_hdd0/packages\ sin tirones en los 60 FPS. |
| **Motor Tipogr�fico** | \/dev_flash/.../SCE-PS3-VR-R-LATIN.TTF\ (\SYS_O_RDONLY\) | FreeType2 + **Atlas de Glifos** pre-rasterizados en RAM XDR | Texto 1080p con anti-aliasing sin recalcular curvas en el scroll. |
| **AdBlocker Nativo** | Tabla binaria ordenada de **120 KB** | Hash **FNV-1a (64-bit)** + \search()\ pre-DNS (\(\log N)\$) | Bloqueo instant�neo en cach� L2 con 0% de impacto en CPU. |
| **Ciclo de Vida XMB** | \SYSUTIL_EXIT_GAME\ + \USRDIR/certs/cacert.pem\ | Cierre ordenado de SPUs/descriptores + Certificados por FTP | Cero triple pitido al salir al XMB y soporte HTTPS a futuro. |
| **Bus EIB y MFC (SPUs)** | \__attribute__((aligned(128)))\ | Cola circular FIFO + Hardware Mailboxes (\spu_readch\) | Transferencias DMA a velocidad de r�faga sin bus errors. |

---

## 22. Resoluci�n Definitiva de los 4 Dilemas de Arquitectura Legendaria

Para blindar la estabilidad, el consumo de memoria y la fluidez gr�fica a 60 FPS en situaciones extremas (im�genes gigantes, JavaScript, composici�n RSX y recuperaci�n de fallos):

### 1. Manejo de Im�genes Gigantes (4K / 8K): M�dulos SPRX Nativos + IDCT Downsampling
- **M�dulos Oficiales en SPUs:** Uso de \SYSMODULE_JPGDEC\ y \SYSMODULE_PNGDEC\ (\libjpgdec\ y \libpngdec\) provistos por el firmware de la consola. Sony dise�� estos m�dulos para ejecutarse de forma transparente y acelerada dentro de las SPUs del Cell.
- **IDCT Scaling al Vuelo (Cero Desperdicio de RAM):** 
  - Al recibir las cabeceras HTTP de una imagen, se lee su resoluci�n original con \jpgDecReadHeader()\.
  - Si la imagen supera 1920x1080 (ej. una foto 4K de 3840x2160), se configura el decodificador (\jpgDecInParam\) con factor de escala \scale_factor\ a 1/2 o 1/4.
  - La SPU descarta frecuencias altas matem�ticamente en la Transformada de Coseno Discreta y vuelca a la RAM XDR directamente una textura de 960x540 o 1920x1080 (ocupando solo ~4 MB en formato RGB565 o ~8 MB en XRGB8888, en lugar de los 48 MB que exigir�a descomprimir la imagen completa en crudo).

### 2. Motor JavaScript: Duktape Contenido con Sandbox de Memoria y Tiempo
- **Por qu� Duktape supera a QuickJS en PS3:**
  - **Soporte Nativo Big-Endian:** La PPU del Cell opera en arquitectura PowerPC Big-Endian. Duktape fue dise�ado en ANSI C89 con control estricto de orden de bytes (\DUK_USE_BYTEORDER\), evitando los errores de empaquetado de punteros (*NaN-boxing*) que sufre QuickJS en arquitecturas no x86/ARM.
  - **Bindings Nativos de NetSurf (\
sgenbind\):** NetSurf ya integra la capa de bindings entre su �rbol DOM (\libdom\) y Duktape, ahorrando la reescritura manual de miles de l�neas de enlace C.
- **Sandbox Estricto Anti-Cuelgues:**
  - **L�mite de Memoria (Pool TLSF de 8 MB):** Al crear el contexto con \duk_create_heap()\, se delega a un sub-pool TLSF de 8 MB. Si un script intenta crear arrays infinitos, recibe \NULL\ a los 8 MB y arroja \RangeError: out of memory\, sin comprometer el resto del navegador.
  - **Watchdog de Tiempo (\DUK_USE_INTERRUPT_COUNTER\):** Compilado con contador de interrupciones que ejecuta un callback cada \\$ instrucciones. Compara el registro de tiempo por hardware (\mftb\ - *Move From Time Base*). Si un script corre por m�s de 250 milisegundos, el callback fuerza la cancelaci�n del bucle infinito sin congelar cuadros de video.

### 3. Pipeline Gr�fico RSX: Enfoque H�brido de 2 Capas
- **Capa 0 (Documento Web en RAM XDR):** El motor NetSurf dibuja la p�gina, fuentes y estilos en la superficie \NSFB_SURFACE_RAM\ en memoria XDR. Se transfiere a la VRAM como una �nica textura grande por DMA solo cuando la p�gina cambia o se produce scroll.
- **Capa 1 (UI, Cursor Magn�tico y Zoom por Hardware RSX):**
  - **Movimiento de Cursor con Coste Cero de DMA:** Cuando el usuario mueve el cursor anal�gico sin hacer scroll, no se vuelcan 8.2 MB de frame por DMA. El RSX re-dibuja el quad de la p�gina que ya reside en VRAM y superpone el sprite del cursor (con su halo ne�n) por hardware en menos de 0.1 ms.
  - **Zoom Cinematogr�fico Gratuito:** Mantener presionado L2 y mover el stick derecho altera las coordenadas de textura (U/V) de los 4 v�rtices del quad en el RSX con filtrado bilineal (\GCM_TEXTURE_LINEAR\), logrando zoom a 60 FPS sin recalcular p�xeles en la CPU.

### 4. Escudo de Recuperaci�n Anti-Crash en 3 Niveles (Cero "Triple Pitido")
- **Nivel 1: Paraca�das de Memoria OOM con \setjmp\ / \longjmp\:**
  - En el wrapper del asignador TLSF, si el pool de 64 MB alcanza el 95% de ocupaci�n, se invoca \longjmp(oom_recovery_env, 1)\ saltando directamente al bucle principal de \main.c\.
  - Se ejecuta \rena_destroy()\, liberando de golpe la pesta�a colapsada y mostrando en pantalla: *"La p�gina excedi� el l�mite de memoria de PS3. Volviendo al inicio..."*.
- **Nivel 2: Hilo Watchdog con Cancelaci�n Limpia de Red:**
  - Un hilo PPU de prioridad alta (\500\) monitorea un contador at�mico de actividad (\heartbeat_counter\).
  - Si una transferencia de red se bloquea por m�s de 8 segundos, el callback \CURLOPT_XFERINFOFUNCTION\ retorna error para que cURL cierre los sockets internamente sin dejar descriptores hu�rfanos en Lv2.
- **Nivel 3: Auto-Reinicio en Caliente (\sysProcessExitSpawn2\):**
  - Si el hilo principal permanece congelado por 10 segundos:
    1. Activa la bandera global \g_emergency_shutdown = 1\.
    2. Cierra ordenadamente cualquier descriptor de archivo abierto en disco (\sysFsClose(g_download_fd)\).
    3. Crea el archivo de seguridad \/dev_hdd0/game/EONSURF01/USRDIR/safe_mode.flag\.
    4. Invoca la llamada del kernel: \sysProcessExitSpawn2("/dev_hdd0/game/EONSURF01/USRDIR/EBOOT.BIN", NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M)\.
    5. El kernel Lv2 descarga el proceso de forma limpia y reinicia EonSurf instant�neamente sin salir al XMB y sin emitir pitidos de error. Al arrancar, \main.c\ detecta la bandera, la elimina y entra en Modo Seguro.

---

## 23. Arquitectura Multimedia, Encolado PKG Nativo y Gesti�n T�rmica (Secretos de la Scene)

Para completar el blindaje de ingenier�a de nivel comercial de EonSurf, se integran los 3 secretos mejor guardados de la scene de PlayStation 3:

### 1. Video HTML5 / YouTube Liviano: El Secreto de Movian (cellVdec + Shader YUV en RSX)
- **Demuxing Ligero en PPU:** Uso de versi�n m�nima de \libavformat\ para extraer NAL units H.264 y audio AAC en XDR.
- **Decodificaci�n por Hardware en SPUs (\SYSMODULE_VDEC\ / \cellVdec\):** 
  - Inicializaci�n con \CELL_VDEC_CODEC_TYPE_AVC\ (soporta hasta High Profile Level 4.2).
  - Sony procesa el bitstream comprimido internamente en las SPUs y devuelve planos crudos **YUV420 Planar (NV12 / YV12)**.
- **Conversi�n YUV ? RGB en el RSX (Cero Carga de PPU):**
  - Se suben los 3 planos (Y, U, V) por DMA a la VRAM como texturas \LUMINANCE8\.
  - Un Fragment Shader simple en el RSX realiza la multiplicaci�n matricial YUV-RGB directamente en la GPU. La CPU principal (PPU) queda libre al 95% permitiendo 60 FPS estables en 720p/1080p.
- **Integraci�n Invidious / MP4:** Para flujos web (itag 18 [360p] o itag 22 [720p]), se superpone un reproductor hardware en la Capa 1 del RSX.

### 2. Instalaci�n de PKGs en Caliente: El Sistema Dual (webMAN MOD Loopback + Burbuja XMB .pdb)
- **M�todo A: Puente Local con webMAN MOD (Instalaci�n Inmediata en 1 Toque):**
  - webMAN MOD escucha en \127.0.0.1:80\ dentro del proceso \sh.self\.
  - Al finalizar una descarga en \/dev_hdd0/packages/archivo.pkg\, EonSurf emite una petici�n HTTP local GET:
    \GET /install.ps3/dev_hdd0/packages/archivo.pkg\ o notificaci�n con trofeo: \GET /popup.ps3?�Descarga_EonSurf_Completada!\
  - webMAN MOD intercepta el comando y realiza el montaje o instalaci�n desatendida.
- **M�todo B: Cola Nativa en Segundo Plano de PKGi (\/dev_hdd0/vsh/task/\):**
  - Para descargas masivas en segundo plano sin mantener EonSurf abierto:
  - Se crea la carpeta numerada en \/dev_hdd0/vsh/task/0000000X/\.
  - Se generan los 4 archivos de base de datos de Sony: \d0.pdb\, \d1.pdb\, \0.pdb\ e \ICON_FILE\.
  - El sistema inyecta la tarea directamente en el gestor de descargas oficial de la burbuja de red del XMB.

### 3. Gesti�n T�rmica y Ventilador: Syscall 383 y Convivencia Pac�fica con webMAN MOD
- **Lectura Continua de Temperaturas (Syscall 383):**
  - Compatible tanto en CFW Evilnat como en PS3HEN:
    \\\c
    // Syscall 383: tzone 0 = CELL, tzone 1 = RSX
    uint32_t temp_cell = 0, temp_rsx = 0;
    lv2syscall2(383, 0, (uint64_t)&temp_cell);
    lv2syscall2(383, 1, (uint64_t)&temp_rsx);
    uint8_t celsius_cell = (uint8_t)(temp_cell >> 24);
    uint8_t celsius_rsx  = (uint8_t)(temp_rsx >> 24);
    \\\
  - Se actualiza cada 2�3 segundos en el HUD: Verde (< 70�C), Amarillo (70�77�C) y Rojo Alerta (= 78�C).
- **Control del Ventilador sin Conflictos de Syscon (Syscall 409 vs 389):**
  - **Detecci�n de webMAN MOD:** Comprueba si webMAN est� activo en puerto 80 o mediante syscall 409 (\sys_sm_get_fan_policy\). Si el modo din�mico est� activo (\mode == 2\), EonSurf **no toca la syscall 389**, evitando la "guerra de payloads" que hace tironear la turbina de la consola.
  - **Fallback de Protecci�n en Consolas Stock (SYSCON Mode 1):** Si se detecta que la consola est� en modo f�brica de Sony y el CELL supera los 75�C durante descargas pesadas, EonSurf eleva el cooler al 40% (\ x66\) v�a syscall 389, restaurando el control original al salir.

---

## 24. Los 5 Pilares Finales de la Biblia de Ingenier�a de EonSurf

Para culminar la especificaci�n m�s completa y robusta de la historia del homebrew en PlayStation 3:

### 1. Detecci�n en Caliente del Entorno (CFW Evilnat vs PS3HEN vs Syscalls Deshabilitadas)
- **El Peligro:** Invocar syscalls de desarrollador cuando el usuario limpi� los registros para entrar a PSN (o en entornos HEN restringidos) provoca un error de kernel \ENOSYS (0x80010003)\ o un cuelgue.
- **Rutina Maestra de Detecci�n (Microsegundos en main.c):**
  \\\c
  #include <ppu-lv2.h>

  typedef enum {
      ENV_SYSCALLS_DISABLED = 0, // Modo seguro / PSN (Syscalls limpias)
      ENV_PS3HEN            = 1, // Entorno H�brido PS3HEN
      ENV_CFW_COBRA_EVILNAT = 2, // Custom Firmware completo (Cobra / Mamba)
      ENV_CFW_STANDARD      = 3  // CFW cl�sico sin payload Cobra
  } ps3_env_t;

  ps3_env_t detect_ps3_environment(void) {
      // 1. Probar Opcode 0x1337 (Exclusivo de PS3HEN: devuelve 0x1337 o versi�n HEN)
      lv2syscall1(8, 0x1337ULL);
      if (p1 == 0x1337ULL || (p1 & 0xFFFF0000ULL) == 0x13370000ULL) {
          return ENV_PS3HEN;
      }
      // 2. Probar Opcode 0x7000 (SYSCALL8_OPCODE_GET_VERSION de Cobra/Mamba)
      uint32_t cobra_ver = 0;
      lv2syscall2(8, 0x7000ULL, (uint64_t)&cobra_ver);
      if (p1 == 0 && cobra_ver > 0) {
          return ENV_CFW_COBRA_EVILNAT;
      }
      // 3. Si Syscall 8 devolvi� ENOSYS (0x80010003), probar Peek Lv2 b�sico (Syscall 6)
      lv2syscall1(6, 0x8000000000000000ULL);
      if (p1 == 0x80010003ULL) {
          return ENV_SYSCALLS_DISABLED; // Syscalls desactivadas por seguridad para PSN
      }
      return ENV_CFW_STANDARD;
  }
  \\\

### 2. Scroll a 60 FPS con Ring Buffer Virtual (Y-Wrap) y Dirty Strip DMA
- **El Problema del Scroll Tradicional:** Redibujar y transferir los 3.68 MB completos de pantalla (720p) en cada fotograma satura el bus EIB y a \libnsfb\.
- **T�cnica de Hardware Wrap-Around:**
  - Se mantiene un buffer circular en RAM XDR con offset vertical: \scroll_y_offset = (scroll_y) % 720\.
  - **Dirty Strip DMA:** Si el usuario desciende 16 p�xeles, \libnsfb\ solo dibuja la franja nueva de 1280x16 p�xeles (**apenas 80 KB** en lugar de 3.68 MB) sobrescribiendo la l�nea m�s antigua en el buffer circular.
  - Se transfieren por DMA solo esos 80 KB a la VRAM.
  - El RSX muestra la textura desplazando la coordenada de textura vertical \ + (scroll\_y\_offset / 720.0f)\$ usando modo \GCM_TEXTURE_REPEAT\.
  - **Resultado:** Reducci�n del ancho de banda de DMA de 220 MB/s a **menos de 4.8 MB/s**. Scroll sedoso e id�ntico a un monitor de alta tasa de refresco.

### 3. Audio de Fondo y Radio Web con Event Queue de Hardware
- **UI & Efectos de Sonido:** Integraci�n de **\spu_soundlib\** (de Hermes/Estwald) ejecut�ndose 100% en una SPU con 0.0% de consumo de PPU.
- **Streams MP3 / AAC en Vivo:** Decodificaci�n acelerada con \SYSMODULE_ADEC\ (\cellAdec\) o \minimp3\ vectorial en hilo PPU de prioridad alta (\500\).
- **Prevenci�n de Tartamudeo (Crackling):** En lugar de bucles \usleep()\, se vincula el puerto de audio a una cola de eventos por interrupci�n con \udioCreateNotifyEventQueue()\. El hardware de sonido de la PS3 despierta el hilo exactamente cada 5.33 ms para transferir los 256 samples PCM y vuelve a dormir sin consumir ciclos par�sitos.

### 4. Cookies e Historial: Estrategia RAM-First con Volcado Diferido (Prohibido SQLite3)
- **Por qu� evitar SQLite3 en SATA I:** SQLite3 dispara bloqueos \sync()\ sobre el sistema de archivos cifrado de la PS3 con latencias mec�nicas de 15 ms, trabando el navegador por m�s de 100 ms por cada cookie actualizada.
- **Soluci�n RAM-First de NetSurf (\urldb\):**
  - Toda la base de datos de historial y cookies reside en RAM durante la ejecuci�n.
  - Al arrancar se lee de un saque en < 5 ms: \/dev_hdd0/game/EONSURF01/USRDIR/data/cookies.dat\ e \history.dat\.
  - Durante la navegaci�n, las operaciones ocurren en memoria a velocidad de nanosegundos marcando \g_userdata_dirty = true\.
  - El hilo secundario de disco (prioridad \1500\) vuelca los datos una vez cada 60 segundos o al invocar \SYSUTIL_EXIT_GAME\, evitando todo desgaste y latencia del disco.

### 5. M�quina de Estados As�ncrona para el Teclado Oficial (\sysutil/osk\)
- **Causa del Cuelgue en Otros Homebrews:** Bloquear el bucle con un \while(osk_active)\ detiene las llamadas a \gcmSetFlip()\ del RSX. Al no recibir VSync, el sistema operativo de Sony (\sh.self\) entra en timeout y congela la pantalla.
- **Patr�n As�ncrono Perfecto a 60 FPS:**
  1. **Disparo No Bloqueante:** Al hacer clic en un campo de texto, se llama a \oskLoadAsync()\ y se fija la bandera \g_osk_state = OSK_STATE_OPEN\. El bucle principal jam�s se detiene.
  2. **Aislamiento de Entrada (Congelar Pad de EonSurf, NO el RSX):**
     - Si \g_osk_state == OSK_STATE_OPEN\, se ignoran las lecturas del stick/botones en el puntero de EonSurf (para que el cursor no se mueva detr�s mientras se eligen letras).
     - Se contin�a dibujando el �ltimo frame est�tico en el RSX ejecutando estrictamente:
       \gcmSetFlip(ctx, cur); rsxFlushBuffer(ctx); gcmSetWaitFlip(ctx); sysUtilCheckCallback();\
  3. **Cierre Fluido en Callback:** Al presionar START en el teclado de Sony, el callback captura \SYSUTIL_OSK_DONE\, extrae el texto UTF-16 convirti�ndolo a UTF-8 para NetSurf con \oskUnloadAsync()\, y al recibir \SYSUTIL_OSK_UNLOADED\ destruye el contenedor de memoria y restaura \g_osk_state = OSK_STATE_CLOSED\.
  - **Resultado:** El teclado de Sony se desliza en 0.1 segundos a 60 FPS con suavidad de juego triple A sobre la interfaz de EonSurf.


---

**Created by JuAnKLiMoNN**

## 25. Los 10 Mandamientos Finales del Silicio (Arquitectura Extrema Cell & RSX)

1. **Hilos de Hardware PPU (SMT 0 vs 1) & Registros HMT:** Prioridades pareadas (1000 y 1001) + control por silicio con `asm volatile("or 1,1,1")` y `"or 2,2,2"`.
2. **Compresion HTTP en SPUs:** Inflate (zlib/miniz) en SPU junto a TLS 1.3, cabecera `Accept-Encoding: gzip, deflate` y Brotli prohibido.
3. **230 MB Libres de VRAM:** Texture Pool en GDDR3 y Snapshots de Paginas con `gcmSetTransferImage(LOCAL_TO_LOCAL)` para boton Atras en 0.2 ms.
4. **SIMD AltiVec / VMX (128-bit) en PPU:** `-maltivec -mabi=altivec`, `vec_lvsl`, jvec_perm` y `vec_mladd` para 4 pixeles RGBA por ciclo.
5. **Memoria Extra sin romper Lv1:** Prestamo oficial de VSH con `sysMemContainerCreate(&container, 16mb)` mientras el OSK este cerrado.
6. **Prefetching Manual L1/L2 (dcbt) especifico para nucleo In-Order:** `__builtin_prefetch(node->next->next, 0, 3)` y `dcbtst` de 256 bytes en blitting.
7. **Triple Buffering Asincrono RSX:** Etiquetas de hardware con `gcmGetLabelAddress(64)` y `gcmSetWriteBackEndLabel` sin `sys_spu_flip_status` bloqueante.
8. **Audio Opus en SPU:** Decodificador libopus compilado con `-DFIXED_POINT=1` (75 KB de codigo) sin SPU Overlays.
9. **USB NTFS / exFAT > 4GB:** Enlace de `libntfs_ext.a` con 128 paginas de cache en RAM y desmontaje seguro antes de salir.
10. **Corte Termico de Emergencia (>83°C):** Buzzer (Syscall 392) + SysFsSync + Apagado limpio por Syscon con Syscall 379 (0x1100).
